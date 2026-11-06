#pragma once

#include <cstdint>
#include <directxmath.h>
#include <vector>

/** Vertex of a mesh: matches VertexFormat::Mesh (position, normal, uv, tangent as float4). */
struct MeshVertex
{
    DirectX::XMFLOAT4 Position{0.0f, 0.0f, 0.0f, 1.0f};
    DirectX::XMFLOAT4 Normal{0.0f, 1.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 TexCoord{0.0f, 0.0f, 0.0f, 0.0f};
    /**
     * xyz points along +u of the texture, w is the sign of the bitangent. Storing the sign
     * instead of a third vector keeps the vertex to four float4 and cannot drift out of step
     * with the normal: the shader rebuilds the bitangent as cross(normal, tangent) * w.
     *
     * A w of 0 means the vertex has no tangent yet, which is the default: geometry that never went
     * through GenerateTangents must fall back to screen-space derivatives in NormalFromMap, not
     * pass an arbitrary vector off as a real basis.
     *
     * Last in the struct on purpose: the producers assign fields by name, but moving TexCoord
     * would change its byte offset and every input layout and shader with it.
     */
    DirectX::XMFLOAT4 Tangent{1.0f, 0.0f, 0.0f, 0.0f};
};

/** Geometry of one detail level, CPU side and device independent. */
struct MeshLodData
{
    std::vector<MeshVertex> Vertices;
    std::vector<uint32_t> Indices;
};

/** Axis-aligned bounds of a mesh plus the radius of the sphere around its centre. */
struct MeshBounds
{
    DirectX::XMFLOAT3 Min{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 Max{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 Center{0.0f, 0.0f, 0.0f};
    float Radius = 0.0f;
};

MeshBounds ComputeMeshBounds(const MeshLodData& mesh);

/**
 * Fills Tangent on every vertex from the texture coordinates.
 *
 * A normal map stores directions in the space of the texture, so the mesh has to say where +u
 * and +v point in the world before such a map means anything. The tangent is accumulated per
 * triangle, weighted by the area the triangle covers in uv space, then made perpendicular to the
 * vertex normal; the bitangent's sign goes into w, which is what mirrored uv islands need.
 *
 * A triangle with a degenerate uv mapping contributes nothing. A vertex left with no usable tangent
 * gets a finite vector in the surface and a w of 0, the same "no basis" a vertex starts with, so
 * a NaN cannot reach the lighting.
 */
void GenerateTangents(MeshLodData& mesh);
