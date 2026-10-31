#include <Engine/Render/MeshSimplifier.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <meshoptimizer.h>

namespace
{
    struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    Vec3 Subtract(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b)
    {
        return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
    }

    Vec3 Cross(const Vec3& a, const Vec3& b)
    {
        return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }

    float Dot(const Vec3& a, const Vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    float Length(const Vec3& v)
    {
        return std::sqrt(Dot(v, v));
    }

    Vec3 Normalized(const Vec3& v, const Vec3& fallback)
    {
        const float length = Length(v);
        if (length < 1e-8f)
        {
            return fallback;
        }
        return Vec3{v.x / length, v.y / length, v.z / length};
    }

    DirectX::XMFLOAT3 CenterOf(const std::vector<DirectX::XMFLOAT4>& positions)
    {
        if (positions.empty())
        {
            return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
        }
        DirectX::XMFLOAT3 minimum{positions[0].x, positions[0].y, positions[0].z};
        DirectX::XMFLOAT3 maximum = minimum;
        for (const DirectX::XMFLOAT4& position : positions)
        {
            minimum.x = std::min(minimum.x, position.x);
            minimum.y = std::min(minimum.y, position.y);
            minimum.z = std::min(minimum.z, position.z);
            maximum.x = std::max(maximum.x, position.x);
            maximum.y = std::max(maximum.y, position.y);
            maximum.z = std::max(maximum.z, position.z);
        }
        return DirectX::XMFLOAT3((minimum.x + maximum.x) * 0.5f,
                                 (minimum.y + maximum.y) * 0.5f,
                                 (minimum.z + maximum.z) * 0.5f);
    }

    float RadiusAbout(const std::vector<DirectX::XMFLOAT4>& positions, const DirectX::XMFLOAT3& center)
    {
        float maxRadiusSq = 0.0f;
        for (const DirectX::XMFLOAT4& position : positions)
        {
            const float dx = position.x - center.x;
            const float dy = position.y - center.y;
            const float dz = position.z - center.z;
            maxRadiusSq = std::max(maxRadiusSq, dx * dx + dy * dy + dz * dz);
        }
        return std::sqrt(maxRadiusSq);
    }

    /** Radius of only the vertices an index buffer still uses: what the level actually draws. */
    float RadiusAbout(const std::vector<DirectX::XMFLOAT4>& positions,
                      const std::vector<uint32_t>& indices,
                      const DirectX::XMFLOAT3& center)
    {
        float maxRadiusSq = 0.0f;
        for (uint32_t index : indices)
        {
            const DirectX::XMFLOAT4& position = positions[index];
            const float dx = position.x - center.x;
            const float dy = position.y - center.y;
            const float dz = position.z - center.z;
            maxRadiusSq = std::max(maxRadiusSq, dx * dx + dy * dy + dz * dz);
        }
        return std::sqrt(maxRadiusSq);
    }

    /** Position-only key, so vertices split for normals or UVs weld back together. */
    struct WeldKey
    {
        float x, y, z;
        bool operator==(const WeldKey& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct WeldKeyHash
    {
        size_t operator()(const WeldKey& key) const
        {
            size_t hash = 0;
            for (float value : {key.x, key.y, key.z})
            {
                // -0.0f and 0.0f must land in the same bucket.
                const float normalized = value == 0.0f ? 0.0f : value;
                uint32_t bits = 0;
                std::memcpy(&bits, &normalized, sizeof(bits));
                hash = hash * 1099511628211ull ^ static_cast<size_t>(bits);
            }
            return hash;
        }
    };

    /**
     * One output vertex per (position, shading cluster): faces that meet at an angle sharper than
     * the crease stay separate, so a box keeps its edges while a curved panel stays smooth.
     */
    struct ShadingCluster
    {
        Vec3 Sum{0.0f, 0.0f, 0.0f};
        Vec3 Reference{0.0f, 1.0f, 0.0f};
        uint32_t OutputIndex = 0;
    };

    MeshLodData RebuildNormals(const std::vector<DirectX::XMFLOAT4>& positions,
                               const std::vector<DirectX::XMFLOAT4>& texCoords,
                               const std::vector<Vec3>& referenceNormals,
                               const std::vector<uint32_t>& indices,
                               float creaseAngleDegrees)
    {
        MeshLodData result;
        if (indices.empty())
        {
            return result;
        }

        const float creaseCos = std::cos(creaseAngleDegrees * 3.14159265f / 180.0f);
        const size_t triangleCount = indices.size() / 3;

        std::vector<Vec3> faceNormals(triangleCount);
        for (size_t triangle = 0; triangle < triangleCount; ++triangle)
        {
            const uint32_t ia = indices[triangle * 3 + 0];
            const uint32_t ib = indices[triangle * 3 + 1];
            const uint32_t ic = indices[triangle * 3 + 2];
            // Not normalised: the length is the area, which weights the average by face size.
            Vec3 faceNormal = Cross(Subtract(positions[ib], positions[ia]),
                                    Subtract(positions[ic], positions[ia]));

            // Which side is outside cannot be read from the winding: the engine draws with culling
            // off, so nothing ever forced the exporter (or the procedural meshes) to wind every
            // face the same way. The normals the model came with say where outside is, so the
            // computed one is flipped to agree with them.
            const Vec3 reference{referenceNormals[ia].x + referenceNormals[ib].x + referenceNormals[ic].x,
                                 referenceNormals[ia].y + referenceNormals[ib].y + referenceNormals[ic].y,
                                 referenceNormals[ia].z + referenceNormals[ib].z + referenceNormals[ic].z};
            if (Dot(faceNormal, reference) < 0.0f)
            {
                faceNormal = Vec3{-faceNormal.x, -faceNormal.y, -faceNormal.z};
            }
            faceNormals[triangle] = faceNormal;
        }

        std::vector<std::vector<ShadingCluster>> clusters(positions.size());
        std::vector<uint32_t> cornerCluster(indices.size(), 0);

        for (size_t corner = 0; corner < indices.size(); ++corner)
        {
            const uint32_t vertex = indices[corner];
            const Vec3& faceNormal = faceNormals[corner / 3];
            const Vec3 unitNormal = Normalized(faceNormal, Vec3{0.0f, 1.0f, 0.0f});

            std::vector<ShadingCluster>& vertexClusters = clusters[vertex];
            size_t chosen = vertexClusters.size();
            for (size_t index = 0; index < vertexClusters.size(); ++index)
            {
                if (Dot(vertexClusters[index].Reference, unitNormal) >= creaseCos)
                {
                    chosen = index;
                    break;
                }
            }

            if (chosen == vertexClusters.size())
            {
                ShadingCluster cluster;
                cluster.Reference = unitNormal;
                vertexClusters.push_back(cluster);
            }

            ShadingCluster& cluster = vertexClusters[chosen];
            cluster.Sum.x += faceNormal.x;
            cluster.Sum.y += faceNormal.y;
            cluster.Sum.z += faceNormal.z;
            cornerCluster[corner] = static_cast<uint32_t>(chosen);
        }

        for (size_t vertex = 0; vertex < clusters.size(); ++vertex)
        {
            for (ShadingCluster& cluster : clusters[vertex])
            {
                const Vec3 normal = Normalized(cluster.Sum, cluster.Reference);
                cluster.OutputIndex = static_cast<uint32_t>(result.Vertices.size());

                MeshVertex output;
                output.Position = positions[vertex];
                output.Position.w = 1.0f;
                output.Normal = DirectX::XMFLOAT4(normal.x, normal.y, normal.z, 0.0f);
                output.TexCoord = texCoords[vertex];
                result.Vertices.push_back(output);
            }
        }

        result.Indices.reserve(indices.size());
        for (size_t corner = 0; corner < indices.size(); ++corner)
        {
            const uint32_t vertex = indices[corner];
            result.Indices.push_back(clusters[vertex][cornerCluster[corner]].OutputIndex);
        }
        return result;
    }
}

MeshLodData SimplifyMesh(const MeshLodData& source,
                         const MeshSimplifySettings& settings,
                         MeshSimplifyResult* outResult)
{
    MeshSimplifyResult report;
    report.SourceTriangles = static_cast<int>(source.Indices.size() / 3);

    auto finish = [&](MeshLodData mesh) -> MeshLodData
    {
        report.ResultTriangles = static_cast<int>(mesh.Indices.size() / 3);
        if (outResult != nullptr)
        {
            *outResult = report;
        }
        return mesh;
    };

    if (source.Vertices.empty() || source.Indices.size() < 3 || source.Indices.size() % 3 != 0)
    {
        return finish(MeshLodData{});
    }

    // Weld by position: the simplifier only collapses vertices that are already shared, and a
    // mesh with split normals shares nothing.
    std::vector<uint32_t> weldRemap(source.Vertices.size(), 0);
    std::unordered_map<WeldKey, uint32_t, WeldKeyHash> weldLookup;
    weldLookup.reserve(source.Vertices.size());

    std::vector<DirectX::XMFLOAT4> weldedPositions;
    std::vector<DirectX::XMFLOAT4> weldedTexCoords;
    // Sum of the normals of every vertex that welded into this one: it points away from the
    // surface even at a corner where the parts disagree, which is all the sign test needs.
    std::vector<Vec3> weldedNormals;
    weldedPositions.reserve(source.Vertices.size());
    weldedTexCoords.reserve(source.Vertices.size());
    weldedNormals.reserve(source.Vertices.size());

    for (size_t vertex = 0; vertex < source.Vertices.size(); ++vertex)
    {
        const DirectX::XMFLOAT4& position = source.Vertices[vertex].Position;
        const DirectX::XMFLOAT4& normal = source.Vertices[vertex].Normal;
        const WeldKey key{position.x, position.y, position.z};
        const auto found = weldLookup.find(key);
        if (found != weldLookup.end())
        {
            weldRemap[vertex] = found->second;
            Vec3& accumulated = weldedNormals[found->second];
            accumulated.x += normal.x;
            accumulated.y += normal.y;
            accumulated.z += normal.z;
            continue;
        }

        const uint32_t newIndex = static_cast<uint32_t>(weldedPositions.size());
        weldLookup.emplace(key, newIndex);
        weldRemap[vertex] = newIndex;
        weldedPositions.push_back(position);
        weldedNormals.push_back(Vec3{normal.x, normal.y, normal.z});
        // The UV of the first vertex at this position wins; UV seams do not survive welding,
        // which is why generated levels are meant for distances where the texture is a blur.
        weldedTexCoords.push_back(source.Vertices[vertex].TexCoord);
    }

    std::vector<uint32_t> weldedIndices(source.Indices.size());
    for (size_t index = 0; index < source.Indices.size(); ++index)
    {
        const uint32_t sourceIndex = source.Indices[index];
        if (sourceIndex >= weldRemap.size())
        {
            return finish(MeshLodData{});
        }
        weldedIndices[index] = weldRemap[sourceIndex];
    }

    const float ratio = std::max(0.001f, std::min(settings.TargetRatio, 1.0f));
    size_t targetIndexCount = static_cast<size_t>(static_cast<float>(weldedIndices.size()) * ratio);
    targetIndexCount -= targetIndexCount % 3;
    targetIndexCount = std::max<size_t>(targetIndexCount, 3);
    report.TargetTriangles = static_cast<int>(targetIndexCount / 3);

    const float* positions = &weldedPositions[0].x;
    const size_t positionStride = sizeof(DirectX::XMFLOAT4);

    std::vector<uint32_t> simplified(weldedIndices.size());
    float resultError = 0.0f;
    size_t resultCount = meshopt_simplify(simplified.data(), weldedIndices.data(), weldedIndices.size(),
                                          positions, weldedPositions.size(), positionStride,
                                          targetIndexCount, settings.TargetError, 0, &resultError);

    // The exact simplifier preserves topology, so a model made of many disconnected parts can
    // stall well above the target. The sloppy one ignores topology and always reaches it - but
    // left to itself it also eats thin parts whole, and a vehicle loses its gun. So it is tried
    // from aggressive to careful, and the first result that keeps the silhouette wins.
    const size_t exactCount = resultCount;
    if (settings.bAllowSloppy && exactCount > targetIndexCount * 3 / 2)
    {
        const DirectX::XMFLOAT3 center = CenterOf(weldedPositions);
        const float sourceRadius = RadiusAbout(weldedPositions, center);
        // From aggressive to careful; a tighter error keeps more triangles and more shape.
        const float errorLimits[] = {1.0f, 0.3f, 0.12f, 0.05f};

        for (float errorLimit : errorLimits)
        {
            std::vector<uint32_t> sloppy(weldedIndices.size());
            float sloppyError = 0.0f;
            const size_t sloppyCount = meshopt_simplifySloppy(sloppy.data(), weldedIndices.data(), weldedIndices.size(),
                                                              positions, weldedPositions.size(), positionStride,
                                                              targetIndexCount, errorLimit, &sloppyError);
            // Only worth taking if it beats what the exact simplifier managed.
            if (sloppyCount < 3 || sloppyCount >= exactCount)
            {
                continue;
            }
            sloppy.resize(sloppyCount);

            const float drift = sourceRadius > 0.0001f
                                    ? std::abs(RadiusAbout(weldedPositions, sloppy, center) - sourceRadius) / sourceRadius
                                    : 0.0f;

            // The first attempt is kept whatever it costs, so there is always a cheap level;
            // every later one replaces it only if it holds the silhouette better.
            if (!report.bUsedSloppy || drift < report.SilhouetteDrift)
            {
                simplified = sloppy;
                resultCount = sloppyCount;
                resultError = sloppyError;
                report.SilhouetteDrift = drift;
                report.bUsedSloppy = true;
            }
            if (report.SilhouetteDrift <= settings.MaxSilhouetteDrift)
            {
                break;
            }
        }
    }

    if (resultCount < 3)
    {
        return finish(MeshLodData{});
    }
    simplified.resize(resultCount);
    report.Error = resultError;

    meshopt_optimizeVertexCache(simplified.data(), simplified.data(), simplified.size(), weldedPositions.size());

    MeshLodData result = RebuildNormals(weldedPositions, weldedTexCoords, weldedNormals,
                                        simplified, settings.CreaseAngleDegrees);
    if (result.Vertices.empty())
    {
        return finish(MeshLodData{});
    }

    meshopt_optimizeVertexFetch(result.Vertices.data(), result.Indices.data(), result.Indices.size(),
                                result.Vertices.data(), result.Vertices.size(), sizeof(MeshVertex));
    return finish(std::move(result));
}
