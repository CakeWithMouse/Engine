#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Engine/Render/SkeletalBaker.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>

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

bool GenerateCrowdSkinnedLod(
    const std::vector<CrowdSkinnedVertex>& inVertices,
    const std::vector<uint32_t>& inIndices,
    float targetRatio,
    std::vector<CrowdSkinnedVertex>& outVertices,
    std::vector<uint32_t>& outIndices)
{
    outVertices.clear();
    outIndices.clear();

    if (inVertices.empty() || inIndices.size() < 3 || targetRatio <= 0.0f)
    {
        return false;
    }

    if (targetRatio >= 1.0f)
    {
        outVertices = inVertices;
        outIndices = inIndices;
        return true;
    }

    // 1. Calculate bounding box
    DirectX::XMFLOAT3 minPos = inVertices[0].Position;
    DirectX::XMFLOAT3 maxPos = inVertices[0].Position;
    for (const auto& v : inVertices)
    {
        minPos.x = (std::min)(minPos.x, v.Position.x);
        minPos.y = (std::min)(minPos.y, v.Position.y);
        minPos.z = (std::min)(minPos.z, v.Position.z);
        maxPos.x = (std::max)(maxPos.x, v.Position.x);
        maxPos.y = (std::max)(maxPos.y, v.Position.y);
        maxPos.z = (std::max)(maxPos.z, v.Position.z);
    }

    const float extentX = maxPos.x - minPos.x;
    const float extentY = maxPos.y - minPos.y;
    const float extentZ = maxPos.z - minPos.z;
    const float bboxSize = (std::max)((std::max)(extentX, extentY), extentZ);

    if (bboxSize <= 1e-6f)
    {
        outVertices = inVertices;
        outIndices = inIndices;
        return true;
    }

    // 2. Compute dominant bone for each vertex
    std::vector<uint8_t> dominantBones(inVertices.size());
    for (size_t i = 0; i < inVertices.size(); ++i)
    {
        const auto& v = inVertices[i];
        uint8_t bestWeight = 0;
        uint8_t bestBone = v.BoneIndices[0];
        for (int k = 0; k < 4; ++k)
        {
            if (v.BoneWeights[k] > bestWeight)
            {
                bestWeight = v.BoneWeights[k];
                bestBone = v.BoneIndices[k];
            }
        }
        dominantBones[i] = bestBone;
    }

    const size_t baseTriangles = inIndices.size() / 3;
    const size_t targetTriangles = (std::max<size_t>)(1, static_cast<size_t>(baseTriangles * targetRatio));


    auto clusterAtRes = [&](int gridRes, std::vector<uint32_t>& vertexMap) -> size_t
    {
        const float cellSize = bboxSize / static_cast<float>(gridRes);
        vertexMap.resize(inVertices.size());

        // Fast 64-bit key: gx(14b) | gy(14b) | gz(14b) | bone(8b)
        std::unordered_map<uint64_t, uint32_t> clusters;
        clusters.reserve(inVertices.size());

        for (size_t i = 0; i < inVertices.size(); ++i)
        {
            const auto& p = inVertices[i].Position;
            const uint32_t gx = static_cast<uint32_t>((std::max)(0, static_cast<int>((p.x - minPos.x) / cellSize))) & 0x3FFF;
            const uint32_t gy = static_cast<uint32_t>((std::max)(0, static_cast<int>((p.y - minPos.y) / cellSize))) & 0x3FFF;
            const uint32_t gz = static_cast<uint32_t>((std::max)(0, static_cast<int>((p.z - minPos.z) / cellSize))) & 0x3FFF;
            const uint64_t key = (static_cast<uint64_t>(gx) << 42) |
                                 (static_cast<uint64_t>(gy) << 28) |
                                 (static_cast<uint64_t>(gz) << 14) |
                                 static_cast<uint64_t>(dominantBones[i]);

            auto it = clusters.find(key);
            if (it == clusters.end())
            {
                clusters[key] = static_cast<uint32_t>(i);
                vertexMap[i] = static_cast<uint32_t>(i);
            }
            else
            {
                vertexMap[i] = it->second;
            }
        }

        size_t validCount = 0;
        for (size_t t = 0; t < baseTriangles; ++t)
        {
            const uint32_t i0 = vertexMap[inIndices[t * 3 + 0]];
            const uint32_t i1 = vertexMap[inIndices[t * 3 + 1]];
            const uint32_t i2 = vertexMap[inIndices[t * 3 + 2]];
            if (i0 != i1 && i1 != i2 && i2 != i0)
            {
                ++validCount;
            }
        }
        return validCount;
    };

    // 3. Binary search for grid resolution
    int low = 6;
    int high = 100;
    int bestRes = 25;
    size_t bestDiff = static_cast<size_t>(-1);
    std::vector<uint32_t> dummyMap;

    for (int iter = 0; iter < 7; ++iter)
    {
        const int mid = (low + high) / 2;
        const size_t triCount = clusterAtRes(mid, dummyMap);
        const size_t diff = (triCount >= targetTriangles) ? (triCount - targetTriangles) : (targetTriangles - triCount);
        if (diff < bestDiff)
        {
            bestDiff = diff;
            bestRes = mid;
        }

        if (triCount < targetTriangles)
        {
            low = mid + 1;
        }
        else
        {
            high = mid - 1;
        }
    }

    // 4. Build final decimated geometry using bestRes
    std::vector<uint32_t> finalVertexMap;
    clusterAtRes(bestRes, finalVertexMap);

    std::unordered_map<uint64_t, bool> seenTriangles;
    seenTriangles.reserve(baseTriangles);

    std::unordered_map<uint32_t, uint32_t> oldToNew;
    oldToNew.reserve(inVertices.size());

    for (size_t t = 0; t < baseTriangles; ++t)
    {
        const uint32_t i0 = finalVertexMap[inIndices[t * 3 + 0]];
        const uint32_t i1 = finalVertexMap[inIndices[t * 3 + 1]];
        const uint32_t i2 = finalVertexMap[inIndices[t * 3 + 2]];

        if (i0 == i1 || i1 == i2 || i2 == i0)
        {
            continue; // Degenerate triangle
        }

        // Canonical hash for deduplication
        uint32_t a = i0, b = i1, c = i2;
        if (a > b) std::swap(a, b);
        if (b > c) std::swap(b, c);
        if (a > b) std::swap(a, b);
        const uint64_t triKey = (static_cast<uint64_t>(a) << 40) ^ (static_cast<uint64_t>(b) << 20) ^ static_cast<uint64_t>(c);

        if (seenTriangles.find(triKey) != seenTriangles.end())
        {
            continue;
        }
        seenTriangles[triKey] = true;

        auto getRemapped = [&](uint32_t origIdx) -> uint32_t
        {
            auto it = oldToNew.find(origIdx);
            if (it != oldToNew.end())
            {
                return it->second;
            }
            const uint32_t newIdx = static_cast<uint32_t>(outVertices.size());
            outVertices.push_back(inVertices[origIdx]);
            oldToNew[origIdx] = newIdx;
            return newIdx;
        };

        outIndices.push_back(getRemapped(i0));
        outIndices.push_back(getRemapped(i1));
        outIndices.push_back(getRemapped(i2));
    }

    return !outVertices.empty() && !outIndices.empty();
}

