#pragma once

#include <cstdint>
#include <directxmath.h>
#include <vector>

/** Vertex of a mesh: matches VertexFormat::Mesh (position, normal, uv padded to float4). */
struct MeshVertex
{
    DirectX::XMFLOAT4 Position{0.0f, 0.0f, 0.0f, 1.0f};
    DirectX::XMFLOAT4 Normal{0.0f, 1.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 TexCoord{0.0f, 0.0f, 0.0f, 0.0f};
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
