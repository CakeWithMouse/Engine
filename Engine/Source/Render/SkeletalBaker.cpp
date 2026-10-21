#include <Engine/Render/SkeletalBaker.h>
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace DirectX;

SkeletalBakeResult BakeSkeletalModel(const SkeletalModel& model, float sampleFps)
{
    SkeletalBakeResult result;

    const size_t boneCount = model.ModelSkeleton.Bones.size();
    if (boneCount == 0)
    {
        result.ErrorMessage = "Model has no bones to bake";
        return result;
    }
    if (boneCount > MaxSkeletalBones)
    {
        result.ErrorMessage = "Bone count exceeds MaxSkeletalBones (64)";
        return result;
    }
    if (model.Clips.empty())
    {
        result.ErrorMessage = "Model has no animation clips to bake";
        return result;
    }
    if (sampleFps <= 0.0f)
    {
        result.ErrorMessage = "Sample FPS must be positive";
        return result;
    }

    const uint32_t width = static_cast<uint32_t>(boneCount * 3);
    uint32_t totalHeight = 0;

    std::vector<BakedClipInfo> clipInfos;
    clipInfos.reserve(model.Clips.size());

    for (size_t clipIndex = 0; clipIndex < model.Clips.size(); ++clipIndex)
    {
        const auto& clip = model.Clips[clipIndex];
        BakedClipInfo info;
        info.Name = clip.Name;
        info.StartRow = totalHeight;
        info.Duration = clip.Duration;
        info.Fps = sampleFps;

        if (clip.Duration > 0.0f)
        {
            info.FrameCount = static_cast<uint32_t>(std::ceil(clip.Duration * sampleFps));
            if (info.FrameCount == 0) info.FrameCount = 1;
        }
        else
        {
            info.FrameCount = 1;
        }

        totalHeight += info.FrameCount;
        clipInfos.push_back(info);
    }

    if (totalHeight == 0)
    {
        result.ErrorMessage = "Total animation frames is zero";
        return result;
    }
    if (totalHeight > 16384)
    {
        result.ErrorMessage = "Total animation frames exceed D3D11 2D texture height limit (16384)";
        return result;
    }

    BakedAnimationTexture baked;
    baked.Width = width;
    baked.Height = totalHeight;
    baked.BoneCount = static_cast<uint32_t>(boneCount);
    baked.Clips = std::move(clipInfos);
    baked.RawData.resize(static_cast<size_t>(width) * totalHeight * 4, 0.0f);

    std::vector<XMFLOAT4X4> palette;
    palette.reserve(boneCount);

    for (size_t clipIndex = 0; clipIndex < model.Clips.size(); ++clipIndex)
    {
        const auto& info = baked.Clips[clipIndex];
        const float duration = info.Duration;
        const uint32_t frameCount = info.FrameCount;

        for (uint32_t frame = 0; frame < frameCount; ++frame)
        {
            float time = 0.0f;
            if (frameCount > 1 && duration > 0.0f)
            {
                time = static_cast<float>(frame) / sampleFps;
                if (time > duration) time = duration;
            }

            if (!EvaluateSkeletalAnimation(model, clipIndex, time, palette, false))
            {
                result.ErrorMessage = "Failed to evaluate skeletal animation at frame " + std::to_string(frame);
                return result;
            }

            const uint32_t y = info.StartRow + frame;
            for (size_t b = 0; b < boneCount; ++b)
            {
                XMMATRIX m = XMLoadFloat4x4(&palette[b]);
                XMMATRIX mTranspose = XMMatrixTranspose(m);

                XMFLOAT4X4 transposed4x4;
                XMStoreFloat4x4(&transposed4x4, mTranspose);

                // Row 0 of M^T
                const size_t offsetRow0 = (static_cast<size_t>(y) * width + (b * 3 + 0)) * 4;
                baked.RawData[offsetRow0 + 0] = transposed4x4._11;
                baked.RawData[offsetRow0 + 1] = transposed4x4._12;
                baked.RawData[offsetRow0 + 2] = transposed4x4._13;
                baked.RawData[offsetRow0 + 3] = transposed4x4._14;

                // Row 1 of M^T
                const size_t offsetRow1 = (static_cast<size_t>(y) * width + (b * 3 + 1)) * 4;
                baked.RawData[offsetRow1 + 0] = transposed4x4._21;
                baked.RawData[offsetRow1 + 1] = transposed4x4._22;
                baked.RawData[offsetRow1 + 2] = transposed4x4._23;
                baked.RawData[offsetRow1 + 3] = transposed4x4._24;

                // Row 2 of M^T
                const size_t offsetRow2 = (static_cast<size_t>(y) * width + (b * 3 + 2)) * 4;
                baked.RawData[offsetRow2 + 0] = transposed4x4._31;
                baked.RawData[offsetRow2 + 1] = transposed4x4._32;
                baked.RawData[offsetRow2 + 2] = transposed4x4._33;
                baked.RawData[offsetRow2 + 3] = transposed4x4._34;
            }
        }
    }

    result.Success = true;
    result.Texture = std::move(baked);
    return result;
}

bool CreateBakedTextureResource(
    ID3D11Device* device,
    const BakedAnimationTexture& baked,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& outTexture,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV)
{
    if (!device || !baked.IsValid())
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = baked.Width;
    desc.Height = baked.Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = baked.RawData.data();
    initData.SysMemPitch = static_cast<UINT>(baked.Width * sizeof(float) * 4);
    initData.SysMemSlicePitch = 0;

    HRESULT hr = device->CreateTexture2D(&desc, &initData, outTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cerr << "CreateBakedTextureResource: failed to create Texture2D (hr=0x"
                  << std::hex << hr << ")" << std::endl;
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(outTexture.Get(), &srvDesc, outSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cerr << "CreateBakedTextureResource: failed to create SRV (hr=0x"
                  << std::hex << hr << ")" << std::endl;
        outTexture.Reset();
        return false;
    }

    return true;
}
