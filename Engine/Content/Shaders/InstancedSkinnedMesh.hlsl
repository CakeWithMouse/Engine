#if defined(SHADOW_PASS)
cbuffer ShadowBuffer : register(b0)
{
    float4x4 worldMatrix;
    float4x4 lightViewProjection;
};
#else
#include "Common.hlsli"
#endif

#include "Instancing.hlsli"

static const float UnitSpecularWeight = 0.35f;
static const float UnitAmbientModel = 1.0f;

StructuredBuffer<InstData> InstBuf : register(t0);
Texture2D<float4> AnimTex : register(t1);

struct ClipInfo
{
    uint StartRow;
    uint FrameCount;
    float Duration;
    float Fps;
};

cbuffer AnimClipBuffer : register(b1)
{
    ClipInfo gClips[16];
    uint gClipCount;
    float3 gClipPadding;
};

struct VS_SKINNED_IN
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float fade : TEXCOORD2;
    float4 color : COLOR;
};

#if !defined(SHADOW_PASS)
Texture2D tex : register(t0);
SamplerState samp : register(s0);
#endif

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

#if !defined(SHADOW_PASS)

PS_IN VSMain(VS_SKINNED_IN input, uint instanceId : SV_InstanceID)
{
    PS_IN output = (PS_IN)0;

    InstData instance = InstBuf[instanceId];
    float3 skinnedPos, skinnedNormal;
    SkinVertex(input, instance, skinnedPos, skinnedNormal);

    float4 worldPos = mul(float4(skinnedPos, 1.0f), instance.World);
    float4 viewPos = mul(worldPos, viewMatrix);
    output.pos = mul(viewPos, projectionMatrix);
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul(skinnedNormal, (float3x3)instance.World));
    output.texCoord = input.texCoord;
    output.color = ObjectColor * instance.Color;
    output.fade = instance.Params.x;
    return output;
}

float4 SampleBaseColor(PS_IN input)
{
    return HasTexture > 0.5f ? tex.Sample(samp, input.texCoord) * input.color : input.color;
}

#if defined(DEFERRED_GBUFFER)
GBufferOut PSMain(PS_IN input)
{
    ClipByFade(input.fade, input.pos.xy);
    float4 baseColor = SampleBaseColor(input);
    MaterialLighting material = MakeMaterialLighting(UnitSpecularWeight, UnitAmbientModel);
    return EncodeGBuffer(baseColor.rgb, input.normal, input.worldPos, material);
}
#else
float4 PSMain(PS_IN input) : SV_Target
{
    ClipByFade(input.fade, input.pos.xy);
    float4 baseColor = SampleBaseColor(input);
    MaterialLighting material = MakeMaterialLighting(UnitSpecularWeight, UnitAmbientModel);
    float3 litColor = ComputeSceneLighting(baseColor.rgb, normalize(input.normal), input.worldPos, material);
    return float4(saturate(litColor), baseColor.a);
}
#endif

#else // SHADOW_PASS

struct VS_SHADOW_OUT
{
    float4 pos : SV_POSITION;
};

VS_SHADOW_OUT VSShadowMain(VS_SKINNED_IN input, uint instanceId : SV_InstanceID)
{
    VS_SHADOW_OUT output = (VS_SHADOW_OUT)0;

    InstData instance = InstBuf[instanceId];
    float3 skinnedPos, skinnedNormal;
    SkinVertex(input, instance, skinnedPos, skinnedNormal);

    float4 worldPos = mul(float4(skinnedPos, 1.0f), instance.World);
    output.pos = mul(worldPos, lightViewProjection);
    return output;
}

#endif
