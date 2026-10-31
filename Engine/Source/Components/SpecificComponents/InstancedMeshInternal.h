#pragma once

#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Render/ShaderCompiler.h>
#include <Engine/Render/SkeletalBaker.h>
#include <Engine/Render/TextureLoader.h>
#include <Engine/Resources/Paths.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>

namespace InstancedMeshDetail
{
    constexpr UINT MeshVertexStride = sizeof(MeshVertex);

    /** Layout of cbuffer ImpostorBuffer (b1) in Shaders/Impostor.hlsl. */
    struct ImpostorConstants
    {
        DirectX::XMFLOAT4 AtlasParams;
        DirectX::XMFLOAT4 ModelParams;
    };

    struct GpuClipInfo
    {
        uint32_t StartRow = 0;
        uint32_t FrameCount = 0;
        float Duration = 0.0f;
        float Fps = 30.0f;
    };

    struct GpuAnimClipBufferData
    {
        GpuClipInfo Clips[16] = {};
        uint32_t ClipCount = 0;
        float Padding[3] = {};
    };

    /**
     * A stable 0..1 value per instance. Stable matters: the unit keeps its place in the shadow
     * fade from frame to frame, so the shadows thin out instead of flickering.
     */
    inline float StableUnitFraction(size_t index)
    {
        uint32_t hash = static_cast<uint32_t>(index) * 2654435761u;
        hash ^= hash >> 15;
        hash *= 2246822519u;
        hash ^= hash >> 13;
        return static_cast<float>(hash & 0xFFFFFFu) / 16777215.0f;
    }

    inline const glm::vec4 LodDebugColors[InstancedMeshComponent::MaxLodCount] =
    {
        glm::vec4(0.25f, 1.0f, 0.35f, 1.0f),
        glm::vec4(1.0f, 0.85f, 0.2f, 1.0f),
        glm::vec4(1.0f, 0.4f, 0.2f, 1.0f),
        glm::vec4(0.6f, 0.4f, 1.0f, 1.0f)
    };

    /**
     * World matrix of one instance, already transposed for HLSL.
     * Translation, yaw and uniform scale only; this keeps per-instance CPU work small and lets
     * the shader transform normals with the same matrix.
     */
    inline DirectX::XMFLOAT4X4 MakeInstanceMatrix(const glm::vec3& position, float yaw, float scale)
    {
        const float c = std::cos(yaw) * scale;
        const float s = std::sin(yaw) * scale;
        return DirectX::XMFLOAT4X4(
            c, 0.0f, s, position.x,
            0.0f, scale, 0.0f, position.y,
            -s, 0.0f, c, position.z,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }
}
