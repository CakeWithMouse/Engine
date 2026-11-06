#include <Engine/Render/MeshData.h>

#include <cmath>

using namespace DirectX;

namespace
{
    XMFLOAT3 Subtract(const XMFLOAT4& a, const XMFLOAT4& b)
    {
        return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    XMFLOAT3 Cross(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return XMFLOAT3(a.y * b.z - a.z * b.y,
                        a.z * b.x - a.x * b.z,
                        a.x * b.y - a.y * b.x);
    }

    float Dot(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    bool Normalize(XMFLOAT3& v)
    {
        const float length = std::sqrt(Dot(v, v));
        if (!(length > 1e-8f))
        {
            return false;
        }
        v.x /= length;
        v.y /= length;
        v.z /= length;
        return true;
    }

    /** Any unit vector perpendicular to n, for vertices the texture coordinates cannot orient. */
    XMFLOAT3 AnyPerpendicular(const XMFLOAT3& n)
    {
        // Cross with whichever axis the normal is least aligned to, so the result never collapses.
        const XMFLOAT3 axis = std::abs(n.x) < 0.9f ? XMFLOAT3(1.0f, 0.0f, 0.0f) : XMFLOAT3(0.0f, 1.0f, 0.0f);
        XMFLOAT3 perpendicular = Cross(n, axis);
        if (!Normalize(perpendicular))
        {
            return XMFLOAT3(1.0f, 0.0f, 0.0f);
        }
        return perpendicular;
    }
}

void GenerateTangents(MeshLodData& mesh)
{
    const size_t vertexCount = mesh.Vertices.size();
    if (vertexCount == 0)
    {
        return;
    }

    std::vector<XMFLOAT3> tangentSum(vertexCount, XMFLOAT3(0.0f, 0.0f, 0.0f));
    std::vector<XMFLOAT3> bitangentSum(vertexCount, XMFLOAT3(0.0f, 0.0f, 0.0f));

    for (size_t index = 0; index + 2 < mesh.Indices.size(); index += 3)
    {
        const uint32_t i0 = mesh.Indices[index];
        const uint32_t i1 = mesh.Indices[index + 1];
        const uint32_t i2 = mesh.Indices[index + 2];
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
        {
            continue;
        }

        const MeshVertex& v0 = mesh.Vertices[i0];
        const MeshVertex& v1 = mesh.Vertices[i1];
        const MeshVertex& v2 = mesh.Vertices[i2];

        const XMFLOAT3 edge1 = Subtract(v1.Position, v0.Position);
        const XMFLOAT3 edge2 = Subtract(v2.Position, v0.Position);
        const float du1 = v1.TexCoord.x - v0.TexCoord.x;
        const float dv1 = v1.TexCoord.y - v0.TexCoord.y;
        const float du2 = v2.TexCoord.x - v0.TexCoord.x;
        const float dv2 = v2.TexCoord.y - v0.TexCoord.y;

        // Twice the area of the triangle in uv space. Zero means the mapping collapses the
        // triangle to a line or a point, and no tangent can be read out of it.
        const float determinant = du1 * dv2 - du2 * dv1;
        if (std::abs(determinant) < 1e-12f)
        {
            continue;
        }

        // Not normalised on purpose: the length weights the triangle by how much of the texture
        // it covers, so a large face is not outvoted by the slivers next to it.
        const float inverse = 1.0f / determinant;
        const XMFLOAT3 tangent(
            (dv2 * edge1.x - dv1 * edge2.x) * inverse,
            (dv2 * edge1.y - dv1 * edge2.y) * inverse,
            (dv2 * edge1.z - dv1 * edge2.z) * inverse);
        const XMFLOAT3 bitangent(
            (du1 * edge2.x - du2 * edge1.x) * inverse,
            (du1 * edge2.y - du2 * edge1.y) * inverse,
            (du1 * edge2.z - du2 * edge1.z) * inverse);

        for (const uint32_t vertexIndex : {i0, i1, i2})
        {
            tangentSum[vertexIndex].x += tangent.x;
            tangentSum[vertexIndex].y += tangent.y;
            tangentSum[vertexIndex].z += tangent.z;
            bitangentSum[vertexIndex].x += bitangent.x;
            bitangentSum[vertexIndex].y += bitangent.y;
            bitangentSum[vertexIndex].z += bitangent.z;
        }
    }

    for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        MeshVertex& vertex = mesh.Vertices[vertexIndex];
        XMFLOAT3 normal(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
        if (!Normalize(normal))
        {
            normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
        }

        // Gram-Schmidt: the tangent has to lie in the surface, and the accumulated one does not
        // after averaging across faces that meet at an angle.
        XMFLOAT3 tangent = tangentSum[vertexIndex];
        const float projection = Dot(tangent, normal);
        tangent.x -= normal.x * projection;
        tangent.y -= normal.y * projection;
        tangent.z -= normal.z * projection;

        if (!Normalize(tangent))
        {
            // No direction to read from the uv. A w of 0 hands the pixel to the fallback in
            // NormalFromMap; the vector itself only has to stay finite and in the surface.
            tangent = AnyPerpendicular(normal);
            vertex.Tangent = XMFLOAT4(tangent.x, tangent.y, tangent.z, 0.0f);
            continue;
        }

        // Mirrored uv islands have a bitangent that points the other way; the sign carries that
        // so the shader does not have to store a second vector per vertex.
        const XMFLOAT3 expected = Cross(normal, tangent);
        const float handedness = Dot(expected, bitangentSum[vertexIndex]) < 0.0f ? -1.0f : 1.0f;

        vertex.Tangent = XMFLOAT4(tangent.x, tangent.y, tangent.z, handedness);
    }
}
