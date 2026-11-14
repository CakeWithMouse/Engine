#pragma once

#include <Engine/Render/SkeletalBaker.h>

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

namespace SkeletalAnimationBoundsTests
{
    inline void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    inline SkeletalModel BuildMovingCrowdModel()
    {
        using namespace DirectX;
        SkeletalModel model;

        Bone root;
        root.Name = "Root";
        root.ParentIndex = -1;
        XMStoreFloat4x4(&root.LocalTransform, XMMatrixIdentity());
        XMStoreFloat4x4(&root.InverseBindPose, XMMatrixIdentity());
        model.ModelSkeleton.Bones.push_back(root);

        Bone moving;
        moving.Name = "Moving";
        moving.ParentIndex = 0;
        XMStoreFloat4x4(&moving.LocalTransform, XMMatrixIdentity());
        XMStoreFloat4x4(&moving.InverseBindPose, XMMatrixIdentity());
        model.ModelSkeleton.Bones.push_back(moving);

        AnimationClip clip;
        clip.Name = "Extend";
        clip.Duration = 1.0f;
        clip.TicksPerSecond = 2.0f;
        NodeAnimationChannel channel;
        channel.NodeName = "Moving";
        channel.BoneIndex = 1;
        channel.PositionKeys.push_back({0.0f, {0.0f, 0.0f, 0.0f}});
        channel.PositionKeys.push_back({1.0f, {4.0f, 0.0f, 0.0f}});
        clip.Channels.push_back(channel);
        model.Clips.push_back(clip);

        SkeletalMesh mesh;
        const DirectX::XMFLOAT3 positions[] = {
            {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
        };
        for (const DirectX::XMFLOAT3& position : positions)
        {
            SkinnedVertex vertex;
            vertex.Position = position;
            vertex.BoneIndices[0] = 0;
            vertex.BoneIndices[1] = 1;
            vertex.BoneWeights[0] = 0.3f;
            vertex.BoneWeights[1] = 0.7f;
            mesh.Vertices.push_back(vertex);
        }
        mesh.Indices = {0, 1, 2};
        model.Meshes.push_back(std::move(mesh));
        return model;
    }

    // Mirrors SkinVertex's crowd path: UNORM8 weights, <=1e-4 influence rejection, three
    // transposed palette rows dotted with (position,1), and the zero-total-weight fallback.
    inline DirectX::XMFLOAT3 SkinCrowdPosition(
        const CrowdSkinnedVertex& vertex, const std::vector<DirectX::XMFLOAT4X4>& palette)
    {
        float totalWeight = 0.0f;
        for (uint8_t weight : vertex.BoneWeights)
        {
            totalWeight += static_cast<float>(weight) / 255.0f;
        }
        if (totalWeight < 0.0001f)
        {
            return vertex.Position;
        }

        float x = 0.0f, y = 0.0f, z = 0.0f;
        for (int influence = 0; influence < 4; ++influence)
        {
            const float weight = static_cast<float>(vertex.BoneWeights[influence]) / 255.0f;
            if (weight <= 0.0001f) continue;
            const uint32_t bone = vertex.BoneIndices[influence];
            if (bone >= palette.size()) continue;
            const DirectX::XMFLOAT4X4& matrix = palette[bone];
            x += weight * (vertex.Position.x * matrix._11 + vertex.Position.y * matrix._21 +
                           vertex.Position.z * matrix._31 + matrix._41);
            y += weight * (vertex.Position.x * matrix._12 + vertex.Position.y * matrix._22 +
                           vertex.Position.z * matrix._32 + matrix._42);
            z += weight * (vertex.Position.x * matrix._13 + vertex.Position.y * matrix._23 +
                           vertex.Position.z * matrix._33 + matrix._43);
        }
        return {x, y, z};
    }

    inline float Radius(const DirectX::XMFLOAT3& position)
    {
        return std::sqrt(position.x * position.x + position.y * position.y + position.z * position.z);
    }

    /**
     * Bake a synthetic two-bone crowd vertex whose animated point leaves the bind-pose sphere.
     * Compare the baker's bound with the quantized positions from every texture row and every
     * linear row interpolation, including the final-row-to-first-row loop seam.
     */
    inline void AnimatedCrowdBoundsContainSamplesAndInterpolation()
    {
        constexpr float sampleFps = 2.0f;
        constexpr float epsilon = 0.0001f;
        SkeletalModel model = BuildMovingCrowdModel();
        SkeletalBakeResult baked = BakeSkeletalModel(model, sampleFps);
        Require(baked.Success && baked.Texture.IsValid(), "Synthetic VAT bake failed");
        Require(baked.Texture.Clips.size() == 1 && baked.Texture.Clips[0].FrameCount == 2,
                "Synthetic clip did not produce the expected two animation rows");

        std::vector<CrowdSkinnedVertex> vertices;
        std::vector<uint32_t> indices;
        model.FlattenCrowd(vertices, indices);
        Require(vertices.size() == 3, "Synthetic crowd vertices were not flattened");

        // The first point starts at radius 1. At sampled t=.5, the moving bone translates by 2.
        // Its UNORM8 weight is adjusted by ConvertToCrowdVertex to keep the byte sum at 255.
        std::vector<DirectX::XMFLOAT4X4> halfPalette;
        Require(EvaluateSkeletalAnimation(model, 0, 0.5f, halfPalette, false),
                "Could not evaluate the known half-second pose");
        const auto quantizedPoint = SkinCrowdPosition(vertices[0], halfPalette);
        const float q0 = static_cast<float>(vertices[0].BoneWeights[0]) / 255.0f;
        const float q1 = static_cast<float>(vertices[0].BoneWeights[1]) / 255.0f;
        const float expectedX = q0 * 1.0f + q1 * 3.0f;
        Require(std::abs(quantizedPoint.x - expectedX) < 0.00001f,
                "Crowd CPU result disagrees with the shader's quantized-weight transform");
        Require(std::abs(expectedX - (0.3f * 1.0f + 0.7f * 3.0f)) > 0.001f,
                "Fixture does not distinguish quantized weights from source float weights");

        const float restRadius = 1.0f;
        Require(Radius(quantizedPoint) > restRadius * 2.0f,
                "Fixture animation did not extend the mesh beyond its bind-pose radius");

        const BakedClipInfo& clip = baked.Texture.Clips[0];
        std::vector<std::vector<DirectX::XMFLOAT3>> sampledPositions(clip.FrameCount);
        float requiredRadius = 0.0f;
        for (uint32_t frame = 0; frame < clip.FrameCount; ++frame)
        {
            std::vector<DirectX::XMFLOAT4X4> palette;
            const float time = static_cast<float>(frame) / sampleFps;
            Require(EvaluateSkeletalAnimation(model, 0, time, palette, false),
                    "Could not evaluate a sampled VAT pose");
            for (const CrowdSkinnedVertex& vertex : vertices)
            {
                const DirectX::XMFLOAT3 position = SkinCrowdPosition(vertex, palette);
                sampledPositions[frame].push_back(position);
                const float radius = Radius(position);
                if (radius > requiredRadius) requiredRadius = radius;
            }
        }

        Require(baked.AnimationBoundsRadius >= requiredRadius,
                "Animation radius does not contain all quantized sampled poses");
        Require(baked.AnimationBoundsRadius < requiredRadius + 0.001f,
                "Animation radius is unexpectedly loose for the synthetic model");

        for (uint32_t frame = 0; frame < clip.FrameCount; ++frame)
        {
            const uint32_t next = (frame + 1u) % clip.FrameCount;
            for (size_t vertex = 0; vertex < vertices.size(); ++vertex)
            {
                for (float alpha : {0.25f, 0.5f, 0.75f})
                {
                    const auto& a = sampledPositions[frame][vertex];
                    const auto& b = sampledPositions[next][vertex];
                    const DirectX::XMFLOAT3 interpolated = {
                        a.x + (b.x - a.x) * alpha,
                        a.y + (b.y - a.y) * alpha,
                        a.z + (b.z - a.z) * alpha
                    };
                    Require(Radius(interpolated) <= baked.AnimationBoundsRadius + epsilon,
                            "Animation radius does not contain a shader-interpolated pose");
                }
            }
        }
    }
}
