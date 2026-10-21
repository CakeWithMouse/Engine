#pragma once

#include <cstdint>
#include <directxmath.h>
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

struct Bone
{
    std::string Name;
    int32_t ParentIndex = -1; // -1 for root bone
    DirectX::XMFLOAT4X4 InverseBindPose; // Transforms vertex from mesh space to local bone space
    DirectX::XMFLOAT4X4 LocalTransform;  // Default bind local TRS transform
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
};
