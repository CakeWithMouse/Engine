#include "Common.hlsli"

// Reflective model (cube map + Fresnel). Forward-only material: the G-buffer does not store
// reflection data, so the deferred renderer draws it in its forward pass against the G-buffer depth.
static const float ReflectiveSpecularWeight = 0.35f;
static const float ReflectiveAmbientModel = 0.0f;

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
TextureCube envMap : register(t1);
SamplerState envSampler : register(s1);

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;

    float4 worldPos = mul(input.pos, worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    output.pos = mul(viewPos, projectionMatrix);
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul(input.normal, (float3x3)normalMatrix));
    output.texCoord = input.texCoord;
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float4 texColor = HasTexture > 0.5f ? tex.Sample(samp, input.texCoord) : ObjectColor;
    float3 normal = normalize(input.normal);
    float3 toCamera = normalize(CameraPosition.xyz - input.worldPos);

    MaterialLighting material = MakeMaterialLighting(ReflectiveSpecularWeight, ReflectiveAmbientModel);
    float3 litColor = ComputeSceneLighting(texColor.rgb, normal, input.worldPos, material);

    if (ReflectionData.x > 0.5f && ReflectionData.y > 0.001f)
    {
        float3 envAmbient = envMap.Sample(envSampler, normal).rgb;
        litColor += texColor.rgb * envAmbient * 0.28f;

        float3 incident = normalize(input.worldPos - CameraPosition.xyz);
        float3 reflectionDir = reflect(incident, normal);
        float3 envColor = envMap.Sample(envSampler, reflectionDir).rgb;
        float fresnel = pow(1.0f - saturate(dot(normal, toCamera)), max(ReflectionData.z, 0.001f));
        float reflectionWeight = saturate(ReflectionData.y * (0.3f + 0.7f * fresnel));
        litColor = lerp(litColor, envColor, reflectionWeight);
    }

    return float4(saturate(litColor), texColor.a);
}
