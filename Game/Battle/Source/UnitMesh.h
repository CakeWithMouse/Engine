#pragma once

#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <array>
#include <string>
#include <vector>

/**
 * Procedural stand-ins for the models designers will provide. Every type is built from boxes with
 * the proportions and triangle ratios of a real asset: LOD0 tessellated, LOD1 the same shapes as
 * plain boxes, LOD2 a silhouette. Replacing this file with imported geometry only changes who
 * fills MeshLodData.
 */
namespace UnitMesh
{
struct Box
{
    DirectX::XMFLOAT3 Center;
    DirectX::XMFLOAT3 HalfExtents;
};

enum class VehicleType
{
    Tank,
    Apc,
    Truck,
    Artillery,
    Count
};

enum class SceneryType
{
    House,
    Tower,
    Wall,
    Count
};

inline void AppendBox(MeshLodData& mesh, const Box& box, int segments)
{
    const int steps = segments < 1 ? 1 : segments;
    // Six faces: axis of the normal, and the two axes spanning the face.
    const std::array<DirectX::XMFLOAT3, 6> normals = {
        DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f),
        DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
        DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f)
    };
    // cross(up, right) == normal on every face: triangles are clockwise seen from outside, the front
    // face of the rasterizer, so the boxes also work with back-face culling (one-sided materials).
    const std::array<DirectX::XMFLOAT3, 6> rightAxis = {
        DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f),
        DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f),
        DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f)
    };
    const std::array<DirectX::XMFLOAT3, 6> upAxis = {
        DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
        DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
        DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f)
    };

    for (size_t face = 0; face < normals.size(); ++face)
    {
        const DirectX::XMFLOAT3& n = normals[face];
        const DirectX::XMFLOAT3& r = rightAxis[face];
        const DirectX::XMFLOAT3& u = upAxis[face];
        const DirectX::XMFLOAT3 faceCenter{
            box.Center.x + n.x * box.HalfExtents.x,
            box.Center.y + n.y * box.HalfExtents.y,
            box.Center.z + n.z * box.HalfExtents.z
        };
        const float halfRight = std::abs(r.x) * box.HalfExtents.x + std::abs(r.y) * box.HalfExtents.y + std::abs(r.z) * box.HalfExtents.z;
        const float halfUp = std::abs(u.x) * box.HalfExtents.x + std::abs(u.y) * box.HalfExtents.y + std::abs(u.z) * box.HalfExtents.z;

        const uint32_t firstVertex = static_cast<uint32_t>(mesh.Vertices.size());
        for (int row = 0; row <= steps; ++row)
        {
            for (int column = 0; column <= steps; ++column)
            {
                const float tx = static_cast<float>(column) / static_cast<float>(steps) * 2.0f - 1.0f;
                const float ty = static_cast<float>(row) / static_cast<float>(steps) * 2.0f - 1.0f;
                MeshVertex vertex;
                vertex.Position = DirectX::XMFLOAT4(
                    faceCenter.x + r.x * tx * halfRight + u.x * ty * halfUp,
                    faceCenter.y + r.y * tx * halfRight + u.y * ty * halfUp,
                    faceCenter.z + r.z * tx * halfRight + u.z * ty * halfUp,
                    1.0f);
                vertex.Normal = DirectX::XMFLOAT4(n.x, n.y, n.z, 0.0f);
                vertex.TexCoord = DirectX::XMFLOAT4(tx * 0.5f + 0.5f, ty * 0.5f + 0.5f, 0.0f, 0.0f);
                mesh.Vertices.push_back(vertex);
            }
        }

        const uint32_t rowStride = static_cast<uint32_t>(steps + 1);
        for (int row = 0; row < steps; ++row)
        {
            for (int column = 0; column < steps; ++column)
            {
                const uint32_t topLeft = firstVertex + static_cast<uint32_t>(row) * rowStride + static_cast<uint32_t>(column);
                const uint32_t topRight = topLeft + 1;
                const uint32_t bottomLeft = topLeft + rowStride;
                const uint32_t bottomRight = bottomLeft + 1;
                mesh.Indices.insert(mesh.Indices.end(), {topLeft, bottomLeft, topRight, topRight, bottomLeft, bottomRight});
            }
        }
    }
}

/** Parts of one vehicle in local space: nose towards +Z, origin on the ground. */
inline std::vector<Box> VehicleBoxes(VehicleType type)
{
    switch (type)
    {
    case VehicleType::Apc:
        return {
            {{0.0f, 1.1f, 0.0f}, {1.3f, 0.6f, 3.2f}},   // hull
            {{0.0f, 1.9f, -0.8f}, {0.7f, 0.3f, 0.8f}},  // small turret
            {{0.0f, 1.9f, 0.4f}, {0.08f, 0.08f, 1.0f}}, // machine gun
            {{-1.35f, 0.6f, 1.8f}, {0.3f, 0.6f, 0.6f}}, // wheels
            {{1.35f, 0.6f, 1.8f}, {0.3f, 0.6f, 0.6f}},
            {{-1.35f, 0.6f, -1.6f}, {0.3f, 0.6f, 0.6f}},
            {{1.35f, 0.6f, -1.6f}, {0.3f, 0.6f, 0.6f}}
        };
    case VehicleType::Truck:
        return {
            {{0.0f, 1.3f, -0.9f}, {1.2f, 0.9f, 2.0f}},  // cargo box
            {{0.0f, 1.1f, 1.9f}, {1.1f, 0.7f, 1.0f}},   // cabin
            {{-1.25f, 0.5f, 1.9f}, {0.25f, 0.5f, 0.5f}},
            {{1.25f, 0.5f, 1.9f}, {0.25f, 0.5f, 0.5f}},
            {{-1.25f, 0.5f, -1.5f}, {0.25f, 0.5f, 0.5f}},
            {{1.25f, 0.5f, -1.5f}, {0.25f, 0.5f, 0.5f}}
        };
    case VehicleType::Artillery:
        return {
            {{0.0f, 0.9f, 0.0f}, {1.5f, 0.45f, 2.6f}},   // low hull
            {{0.0f, 1.7f, -0.6f}, {1.1f, 0.45f, 1.4f}},  // crew compartment
            {{0.0f, 2.1f, 1.8f}, {0.15f, 0.15f, 2.6f}},  // long gun
            {{-1.6f, 0.45f, 0.0f}, {0.2f, 0.45f, 2.6f}}, // tracks
            {{1.6f, 0.45f, 0.0f}, {0.2f, 0.45f, 2.6f}}
        };
    case VehicleType::Tank:
    default:
        return {
            {{0.0f, 1.0f, 0.0f}, {1.4f, 0.5f, 3.0f}},    // hull
            {{0.0f, 1.8f, -0.3f}, {1.0f, 0.4f, 1.2f}},   // turret
            {{0.0f, 1.8f, 1.8f}, {0.12f, 0.12f, 1.4f}},  // barrel
            {{-1.5f, 0.5f, 0.0f}, {0.25f, 0.5f, 3.0f}},  // tracks
            {{1.5f, 0.5f, 0.0f}, {0.25f, 0.5f, 3.0f}}
        };
    }
}

/** Box that covers the whole vehicle: used as the farthest mesh level. */
inline Box VehicleSilhouette(VehicleType type)
{
    switch (type)
    {
    case VehicleType::Apc: return {{0.0f, 1.1f, 0.0f}, {1.65f, 1.1f, 3.2f}};
    case VehicleType::Truck: return {{0.0f, 1.2f, 0.0f}, {1.5f, 1.2f, 3.0f}};
    case VehicleType::Artillery: return {{0.0f, 1.1f, 0.3f}, {1.8f, 1.1f, 3.6f}};
    case VehicleType::Tank:
    default: return {{0.0f, 1.0f, 0.0f}, {1.75f, 1.0f, 3.0f}};
    }
}

inline const char* VehicleName(VehicleType type)
{
    switch (type)
    {
    case VehicleType::Apc: return "Apc";
    case VehicleType::Truck: return "Truck";
    case VehicleType::Artillery: return "Artillery";
    case VehicleType::Tank:
    default: return "Tank";
    }
}

/** How fast this type moves, relative to the scene speed. */
inline float VehicleSpeedScale(VehicleType type)
{
    switch (type)
    {
    case VehicleType::Apc: return 1.25f;
    case VehicleType::Truck: return 1.1f;
    case VehicleType::Artillery: return 0.65f;
    case VehicleType::Tank:
    default: return 1.0f;
    }
}

/**
 * detail multiplies the tessellation of every level, so a stand-in can be made as heavy as a
 * real asset (detail 1 is roughly 2k triangles for LOD0, detail 4 is roughly 35k).
 */
inline MeshLodData BuildVehicleLod(VehicleType type, int level, int detail = 1)
{
    MeshLodData mesh;
    const int scale = detail < 1 ? 1 : detail;
    const std::vector<Box> boxes = VehicleBoxes(type);
    switch (level)
    {
    case 0:
        for (const Box& box : boxes) AppendBox(mesh, box, 6 * scale);
        break;
    case 1:
        for (const Box& box : boxes) AppendBox(mesh, box, 2 * scale);
        break;
    default:
        AppendBox(mesh, VehicleSilhouette(type), 2 * scale);
        break;
    }
    return mesh;
}

inline const char* SceneryName(SceneryType type)
{
    switch (type)
    {
    case SceneryType::Tower: return "Tower";
    case SceneryType::Wall: return "Wall";
    case SceneryType::House:
    default: return "House";
    }
}

/** Map objects that never move; their shadow is baked once. */
inline MeshLodData BuildSceneryLod(SceneryType type, int level, int detail = 1)
{
    MeshLodData mesh;
    const int scale = detail < 1 ? 1 : detail;
    const bool detailed = level == 0;
    switch (type)
    {
    case SceneryType::Tower:
        AppendBox(mesh, Box{{0.0f, 9.0f, 0.0f}, {2.6f, 9.0f, 2.6f}}, detailed ? 4 * scale : 1);
        if (detailed)
        {
            AppendBox(mesh, Box{{0.0f, 18.4f, 0.0f}, {3.2f, 0.4f, 3.2f}}, 2 * scale);
        }
        break;
    case SceneryType::Wall:
        AppendBox(mesh, Box{{0.0f, 1.8f, 0.0f}, {6.0f, 1.8f, 0.7f}}, detailed ? 4 * scale : 1);
        if (detailed)
        {
            AppendBox(mesh, Box{{0.0f, 3.8f, 0.0f}, {1.0f, 0.3f, 0.9f}}, 2 * scale);
        }
        break;
    case SceneryType::House:
    default:
        AppendBox(mesh, Box{{0.0f, 5.0f, 0.0f}, {5.0f, 5.0f, 5.0f}}, detailed ? 4 * scale : 1);
        if (detailed)
        {
            AppendBox(mesh, Box{{0.0f, 10.5f, 0.0f}, {5.6f, 0.5f, 5.6f}}, 2 * scale);
        }
        break;
    }
    return mesh;
}

inline int CountTriangles(const MeshLodData& mesh)
{
    return static_cast<int>(mesh.Indices.size() / 3);
}
}
