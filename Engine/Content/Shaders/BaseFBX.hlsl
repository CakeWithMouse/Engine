#include "Common.hlsli"

// Textured model material. Without a texture the object colour (component colour x sub-mesh
// material colour) is used, so models without images still render.
static const float FBXSpecularWeight = 0.35f;
static const float FBXAmbientModel = 0.0f; // flat ambient

struct VS_IN
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 worldPos : TEXCOORD1;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;

    float4 worldPos = mul(input.pos, worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    output.pos = mul(viewPos, projectionMatrix);
    // Inverse-transpose keeps normals perpendicular under non-uniform scale.
    output.normal = mul(input.normal, (float3x3)normalMatrix);
    output.texCoord = input.texCoord;
    output.worldPos = worldPos.xyz;

    return output;
}

float4 SampleBaseColor(float2 uv)
{
    return HasTexture > 0.5f ? tex.Sample(samp, uv) : ObjectColor;
}

#if defined(DEFERRED_GBUFFER)
GBufferOut PSMain(PS_IN input)
{
    float4 baseColor = SampleBaseColor(input.texCoord);
    MaterialLighting material = MakeMaterialLighting(FBXSpecularWeight, FBXAmbientModel);
    return EncodeGBuffer(baseColor.rgb, input.normal, input.worldPos, material);
}
#else
float4 PSMain(PS_IN input) : SV_Target
{
    float4 baseColor = SampleBaseColor(input.texCoord);
    MaterialLighting material = MakeMaterialLighting(FBXSpecularWeight, FBXAmbientModel);
    float3 litColor = ComputeSceneLighting(baseColor.rgb, normalize(input.normal), input.worldPos, material);
    return float4(saturate(litColor), baseColor.a);
}
#endif
