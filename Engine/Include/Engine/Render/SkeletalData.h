#pragma once

#include <cstdint>
#include <directxmath.h>
#include <directxpackedvector.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

constexpr size_t MaxSkeletalBones = 64;

/**
 * Vertex with skinning attributes.
 * Size is 64 bytes (16-byte aligned).
 */
struct SkinnedVertex
{
    DirectX::XMFLOAT3 Position{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 Normal{0.0f, 1.0f, 0.0f};
    DirectX::XMFLOAT2 TexCoord{0.0f, 0.0f};
    uint32_t BoneIndices[4]{0, 0, 0, 0};
    float BoneWeights[4]{0.0f, 0.0f, 0.0f, 0.0f};
};

static_assert(sizeof(SkinnedVertex) == 64, "SkinnedVertex must be 64 bytes");

/**
 * Compact vertex for crowds and instanced rendering.
 * Size is 32 bytes (exact 50% reduction in vertex fetch bandwidth).
 * Layout matches D3D11 input layout:
 * - Position:    DXGI_FORMAT_R32G32B32_FLOAT    (12 bytes)
 * - Normal:      DXGI_FORMAT_R16G16B16A16_FLOAT (8 bytes, half4)
 * - TexCoord:    DXGI_FORMAT_R16G16_FLOAT       (4 bytes, half2)
 * - BoneIndices: DXGI_FORMAT_R8G8B8A8_UINT      (4 bytes, uint8[4])
 * - BoneWeights: DXGI_FORMAT_R8G8B8A8_UNORM     (4 bytes, unorm8[4])
 */
struct CrowdSkinnedVertex
{
    DirectX::XMFLOAT3 Position{0.0f, 0.0f, 0.0f};
    uint16_t Normal[4]{0, 0, 0, 0};      // half4 (x, y, z, 0)
    uint16_t TexCoord[2]{0, 0};          // half2 (u, v)
    uint8_t BoneIndices[4]{0, 0, 0, 0};  // uint8[4]
    uint8_t BoneWeights[4]{0, 0, 0, 0};  // unorm8[4], sums to 255 (1.0f)
};

static_assert(sizeof(CrowdSkinnedVertex) == 32, "CrowdSkinnedVertex must be 32 bytes");

inline CrowdSkinnedVertex ConvertToCrowdVertex(const SkinnedVertex& v)
{
    CrowdSkinnedVertex out;
    out.Position = v.Position;

    // Normal to half4
    out.Normal[0] = DirectX::PackedVector::XMConvertFloatToHalf(v.Normal.x);
    out.Normal[1] = DirectX::PackedVector::XMConvertFloatToHalf(v.Normal.y);
    out.Normal[2] = DirectX::PackedVector::XMConvertFloatToHalf(v.Normal.z);
    out.Normal[3] = 0;

    // TexCoord to half2
    out.TexCoord[0] = DirectX::PackedVector::XMConvertFloatToHalf(v.TexCoord.x);
    out.TexCoord[1] = DirectX::PackedVector::XMConvertFloatToHalf(v.TexCoord.y);

    // Bone indices
    for (int i = 0; i < 4; ++i)
    {
        out.BoneIndices[i] = static_cast<uint8_t>(std::min<uint32_t>(v.BoneIndices[i], 255u));
    }

    // Bone weights quantized to unorm8 (0..255) ensuring sum is exactly 255 (1.0 in UNORM)
    float sum = v.BoneWeights[0] + v.BoneWeights[1] + v.BoneWeights[2] + v.BoneWeights[3];
    if (sum > 1e-6f)
    {
        int q[4];
        int qSum = 0;
        int maxIdx = 0;
        for (int i = 0; i < 4; ++i)
        {
            float normalized = v.BoneWeights[i] / sum;
            q[i] = static_cast<int>(std::round(normalized * 255.0f));
            qSum += q[i];
            if (v.BoneWeights[i] > v.BoneWeights[maxIdx])
            {
                maxIdx = i;
            }
        }
        int diff = 255 - qSum;
        q[maxIdx] = std::clamp(q[maxIdx] + diff, 0, 255);
        for (int i = 0; i < 4; ++i)
        {
            out.BoneWeights[i] = static_cast<uint8_t>(q[i]);
        }
    }
    else
    {
        out.BoneWeights[0] = 255;
        out.BoneWeights[1] = 0;
        out.BoneWeights[2] = 0;
        out.BoneWeights[3] = 0;
    }

    return out;
}

struct Bone
{
    std::string Name;
    int32_t ParentIndex = -1; // -1 for root bone
    DirectX::XMFLOAT4X4 InverseBindPose; // Transforms vertex from mesh space to local bone space
    DirectX::XMFLOAT4X4 LocalTransform;  // Default bind local TRS transform
    DirectX::XMFLOAT3 RestTranslation{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 RestRotation{0.0f, 0.0f, 0.0f, 1.0f};
    DirectX::XMFLOAT3 RestScale{1.0f, 1.0f, 1.0f};
};

struct Skeleton
{
    std::vector<Bone> Bones;

    int32_t FindBoneIndex(std::string_view name) const
    {
        for (size_t i = 0; i < Bones.size(); ++i)
        {
            if (Bones[i].Name == name)
            {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }
};

struct VectorKey
{
    float Time = 0.0f;
    DirectX::XMFLOAT3 Value{0.0f, 0.0f, 0.0f};
};

struct QuatKey
{
    float Time = 0.0f;
    DirectX::XMFLOAT4 Value{0.0f, 0.0f, 0.0f, 1.0f}; // x, y, z, w
};

struct NodeAnimationChannel
{
    std::string NodeName;
    int32_t BoneIndex = -1;
    std::vector<VectorKey> PositionKeys;
    std::vector<QuatKey> RotationKeys;
    std::vector<VectorKey> ScalingKeys;
};

struct AnimationClip
{
    std::string Name;
    float Duration = 0.0f;          // In seconds
    float TicksPerSecond = 25.0f;
    std::vector<NodeAnimationChannel> Channels;
};

struct SkeletalMesh
{
    std::vector<SkinnedVertex> Vertices;
    std::vector<uint32_t> Indices;
};

struct SkeletalModel
{
    std::vector<SkeletalMesh> Meshes;
    Skeleton ModelSkeleton;
    std::vector<AnimationClip> Clips;
    int TotalIndexCount = 0;
    float BoundingRadius = 0.0f;
    DirectX::XMFLOAT4X4 MeshToModel;

    SkeletalModel()
    {
        DirectX::XMStoreFloat4x4(&MeshToModel, DirectX::XMMatrixIdentity());
    }
};
