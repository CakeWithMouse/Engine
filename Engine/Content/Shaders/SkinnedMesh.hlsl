#include "Common.hlsli"

cbuffer BoneBuffer : register(b1)
{
    float4x4 gBones[64];
};

Texture2D diffuseMap : register(t0);
SamplerState diffuseSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    uint4  BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;

    float3 skinnedPos = float3(0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
    if (dot(input.BoneWeights, float4(1, 1, 1, 1)) <= 0.0f)
    {
        skinnedPos = input.Position;
        skinnedNormal = input.Normal;
    }

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float w = input.BoneWeights[i];
        if (w > 0.0f)
        {
            uint b = input.BoneIndices[i];
            float4x4 bMat = gBones[b];
            skinnedPos += w * mul(float4(input.Position, 1.0f), bMat).xyz;
            skinnedNormal += w * mul(input.Normal, (float3x3)bMat);
        }
    }

    float4 worldPos = mul(float4(skinnedPos, 1.0f), worldMatrix);
    output.WorldPos = worldPos.xyz;
    float4 viewPos = mul(worldPos, viewMatrix);
    output.Position = mul(viewPos, projectionMatrix);
    output.Normal = normalize(mul(skinnedNormal, (float3x3)normalMatrix));
    output.TexCoord = input.TexCoord;

    return output;
}

float4 PSMain(PS_INPUT input) : SV_Target
{
    float4 texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    if (HasTexture > 0.5f)
    {
        texColor = diffuseMap.Sample(diffuseSampler, input.TexCoord);
    }

    float4 albedo = texColor * ObjectColor;

    // Forward rendering without shadows
    float3 toLight = normalize(-LightDirection.xyz);
    float diff = max(dot(input.Normal, toLight), 0.0f);

    float3 lightColor = DirectionalLightColorIntensity.rgb * DirectionalLightColorIntensity.a;
    if (length(lightColor) < 0.01f)
    {
        lightColor = float3(1.0f, 1.0f, 1.0f);
    }

    float3 ambient = float3(0.35f, 0.35f, 0.35f);
    float3 litColor = albedo.rgb * (ambient + diff * lightColor);

    return float4(litColor, albedo.a);
}
