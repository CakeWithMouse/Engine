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
#include "InstancingPixel.hlsli"

StructuredBuffer<InstData> InstBuf : register(t0);
#define SKINNING_ANIMATION_TEXTURE
#include "Skinning.hlsli"

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
