#ifndef ENGINE_SKINNING_HLSLI
#define ENGINE_SKINNING_HLSLI

// Skinning of SkinnedVertex and CrowdSkinnedVertex meshes. The skinning data lives in b3
// (b1 and b2 belong to materials):
//     default                       bone palette of one animated object
//     SKINNING_ANIMATION_TEXTURE    crowds: bones baked into a texture (t1), each copy picks its
//                                   clip and phase from InstData.Params.yz

struct VS_SKINNED_IN
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

#if defined(SKINNING_ANIMATION_TEXTURE)
#include "Instancing.hlsli"

Texture2D<float4> AnimTex : register(t1);

struct ClipInfo
{
    uint StartRow;
    uint FrameCount;
    float Duration;
    float Fps;
};

cbuffer AnimClipBuffer : register(b3)
{
    ClipInfo gClips[16];
    uint gClipCount;
    float3 gClipPadding;
};

void SkinVertex(VS_SKINNED_IN input, InstData instance, out float3 outSkinnedPos, out float3 outSkinnedNormal)
{
    uint texWidth, texHeight;
    AnimTex.GetDimensions(texWidth, texHeight);

    uint startRow = 0;
    uint frameCount = max(1u, texHeight);
    if (gClipCount > 0)
    {
        uint clipIdx = min((uint)instance.Params.y, gClipCount - 1u);
        startRow = gClips[clipIdx].StartRow;
        frameCount = max(1u, gClips[clipIdx].FrameCount);
    }

    float phase = frac(instance.Params.z);
    if (phase < 0.0f) phase += 1.0f;
    float frameFloat = phase * (float)frameCount;
    uint f0 = (uint)floor(frameFloat) % frameCount;
    uint y0 = startRow + f0;

#if defined(ANIM_LERP)
    uint f1 = (f0 + 1u) % frameCount;
    uint y1 = startRow + f1;
    float fracPart = frac(frameFloat);
#endif

    float3 skinnedPos = float3(0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (int b = 0; b < 4; ++b)
    {
        float weight = input.boneWeights[b];
        if (weight > 0.0001f)
        {
            uint bone = input.boneIndices[b];
            uint col = bone * 3;

#if defined(ANIM_LERP)
            float4 r0 = lerp(AnimTex.Load(int3(col + 0, y0, 0)), AnimTex.Load(int3(col + 0, y1, 0)), fracPart);
            float4 r1 = lerp(AnimTex.Load(int3(col + 1, y0, 0)), AnimTex.Load(int3(col + 1, y1, 0)), fracPart);
            float4 r2 = lerp(AnimTex.Load(int3(col + 2, y0, 0)), AnimTex.Load(int3(col + 2, y1, 0)), fracPart);
#else
            float4 r0 = AnimTex.Load(int3(col + 0, y0, 0));
            float4 r1 = AnimTex.Load(int3(col + 1, y0, 0));
            float4 r2 = AnimTex.Load(int3(col + 2, y0, 0));
#endif

            float4 p = float4(input.pos.xyz, 1.0f);
            skinnedPos.x += weight * dot(p, r0);
            skinnedPos.y += weight * dot(p, r1);
            skinnedPos.z += weight * dot(p, r2);

            skinnedNormal.x += weight * dot(input.normal, r0.xyz);
            skinnedNormal.y += weight * dot(input.normal, r1.xyz);
            skinnedNormal.z += weight * dot(input.normal, r2.xyz);
        }
    }

    float totalWeight = input.boneWeights.x + input.boneWeights.y + input.boneWeights.z + input.boneWeights.w;
    if (totalWeight < 0.0001f)
    {
        skinnedPos = input.pos.xyz;
        skinnedNormal = input.normal;
    }

    outSkinnedPos = skinnedPos;
    outSkinnedNormal = skinnedNormal;
}
#else
cbuffer BoneBuffer : register(b3)
{
    float4x4 gBones[64];
};

void SkinVertex(VS_SKINNED_IN input, out float3 outSkinnedPos, out float3 outSkinnedNormal)
{
    float3 skinnedPos = float3(0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
    if (dot(input.boneWeights, float4(1, 1, 1, 1)) <= 0.0f)
    {
        skinnedPos = input.pos.xyz;
        skinnedNormal = input.normal;
    }

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float w = input.boneWeights[i];
        if (w > 0.0f)
        {
            uint b = input.boneIndices[i];
            float4x4 bMat = gBones[b];
            skinnedPos += w * mul(float4(input.pos.xyz, 1.0f), bMat).xyz;
            skinnedNormal += w * mul(input.normal, (float3x3)bMat);
        }
    }

    outSkinnedPos = skinnedPos;
    outSkinnedNormal = skinnedNormal;
}
#endif

#endif
