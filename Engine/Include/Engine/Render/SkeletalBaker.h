#pragma once

#include <Engine/Render/SkeletalData.h>
#include <Engine/Render/SkeletalAnimation.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>
#include <vector>

/**
 * Metadata for a single animation clip baked into a vertical range of the texture.
 */
struct BakedClipInfo
{
    std::string Name;
    uint32_t StartRow = 0;   // Starting Y coordinate (row) in the baked texture
    uint32_t FrameCount = 0; // Number of animation frames sampled for this clip
    float Duration = 0.0f;   // Duration in seconds
    float Fps = 30.0f;       // Sampling rate in frames per second
};

/**
 * Animation data baked into a 2D matrix texture.
 * Format: DXGI_FORMAT_R32G32B32A32_FLOAT (RGBA32F).
 * Width: BoneCount * 3 (each bone takes 3 texels representing the 3 rows of M^T).
 * Height: Total frames across all baked clips.
 */
struct BakedAnimationTexture
{
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t BoneCount = 0;
    std::vector<BakedClipInfo> Clips;
    std::vector<float> RawData; // Width * Height * 4 float components

    bool IsValid() const
    {
        return Width > 0 && Height > 0 && !RawData.empty() && RawData.size() == (static_cast<size_t>(Width) * Height * 4);
    }

    const float* GetTexel(uint32_t x, uint32_t y) const
    {
        if (x >= Width || y >= Height) return nullptr;
        return &RawData[(static_cast<size_t>(y) * Width + x) * 4];
    }
};

struct SkeletalBakeResult
{
    bool Success = false;
    std::string ErrorMessage;
    BakedAnimationTexture Texture;
};

/**
 * Bakes all animation clips of a skeletal model into an RGBA32F matrix texture.
 *
 * Each bone matrix M_final is transposed (M^T) and stored as 3 float4 vectors:
 * - Row 0 of M^T at texel (bone * 3 + 0, frame) -> X-axis transform & Tx
 * - Row 1 of M^T at texel (bone * 3 + 1, frame) -> Y-axis transform & Ty
 * - Row 2 of M^T at texel (bone * 3 + 2, frame) -> Z-axis transform & Tz
 *
 * In HLSL:
 * skinnedPos.x = dot(float4(pos, 1.0), row0);
 * skinnedPos.y = dot(float4(pos, 1.0), row1);
 * skinnedPos.z = dot(float4(pos, 1.0), row2);
 *
 * skinnedNormal.x = dot(normal, row0.xyz);
 * skinnedNormal.y = dot(normal, row1.xyz);
 * skinnedNormal.z = dot(normal, row2.xyz);
 */
SkeletalBakeResult BakeSkeletalModel(const SkeletalModel& model, float sampleFps = 30.0f);

/**
 * Creates an immutable Direct3D 11 2D texture and SRV from baked animation data.
 */
bool CreateBakedTextureResource(
    ID3D11Device* device,
    const BakedAnimationTexture& baked,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& outTexture,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV);
