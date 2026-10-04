#ifndef ENGINE_COMMON_HLSLI
#define ENGINE_COMMON_HLSLI

// Shared by the materials and the deferred lighting pass.
// Layout must match ConstantBufferData in Source/Public/Render/ShaderConstants.h.
cbuffer ConstantBuffer : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 invViewMatrix;
    float4x4 invProjectionMatrix;
    float4 ObjectColor;
    float2 UVOffset;
    float HasTexture;
    float padding;
    float4 CameraPosition;
    float4 LightPositions[8];
    float4 LightColors[8];
    float4 LightParams[8];
    float4 LightMeta;            // x light count, y ambient, z shininess
    float4 ReflectionData;       // x has cube map, y strength, z fresnel power, w is skybox
    float4x4 LightViewProjection[3];
    float4 CascadeSplits;
    float4 ShadowParams;         // x enabled, y cascades, z bias, w texel size
    float4 LightDirection;
    float4 DirectionalLightColorIntensity;
    float4x4 normalMatrix;       // inverse-transpose of worldMatrix
};

// G-buffer contract:
//   Albedo   rgb = base colour,     a = specular weight
//   Normal   rgb = normal * 0.5+0.5, a = ambient model (0 flat, 1 hemispherical)
//   Position xyz = world position (half float)
struct GBufferOut
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPosition : SV_Target2;
};

// Lighting parameters of a material; stored in the G-buffer so both paths light identically.
struct MaterialLighting
{
    float SpecularWeight;
    float AmbientModel;
};

MaterialLighting MakeMaterialLighting(float specularWeight, float ambientModel)
{
    MaterialLighting material;
    material.SpecularWeight = specularWeight;
    material.AmbientModel = ambientModel;
    return material;
}

GBufferOut EncodeGBuffer(float3 albedo, float3 normal, float3 worldPos, MaterialLighting material)
{
    GBufferOut output;
    output.Albedo = float4(saturate(albedo), material.SpecularWeight);
    output.Normal = float4(normalize(normal) * 0.5f + 0.5f, material.AmbientModel);
    output.WorldPosition = float4(worldPos, 1.0f);
    return output;
}

Texture2DArray shadowMapTex : register(t4);
SamplerState shadowSampler : register(s4);

int SelectCascade(float viewDepth)
{
    if (viewDepth < CascadeSplits.x) return 0;
    if (viewDepth < CascadeSplits.y) return 1;
    return 2;
}

float ComputeShadowFactor(float3 worldPos, float3 normal, float3 toLightDir, float viewDepth)
{
    if (ShadowParams.x < 0.5f)
    {
        return 1.0f;
    }

    int cascadeIndex = SelectCascade(viewDepth);
    float4 lightPos = mul(float4(worldPos, 1.0f), LightViewProjection[cascadeIndex]);
    float3 projCoords = lightPos.xyz / max(lightPos.w, 0.0001f);

    float2 uv;
    uv.x = projCoords.x * 0.5f + 0.5f;
    uv.y = -projCoords.y * 0.5f + 0.5f;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || projCoords.z <= 0.0f || projCoords.z >= 1.0f)
    {
        return 1.0f;
    }

    float baseBias = max(ShadowParams.z, 0.00005f);
    float slopeBias = 1.0f - saturate(dot(normal, toLightDir));
    float bias = max(baseBias * slopeBias, baseBias * 0.25f);
    float texelSize = max(ShadowParams.w, 0.0001f);

    float visibility = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2((float)x, (float)y) * texelSize;
            float sampledDepth = shadowMapTex.SampleLevel(shadowSampler, float3(uv + offset, cascadeIndex), 0.0f).r;
            visibility += ((projCoords.z - bias) <= sampledDepth) ? 1.0f : 0.0f;
        }
    }

    return visibility / 9.0f;
}

// Point lights + directional light with cascaded shadows. Returns the lit colour (not clamped).
float3 ComputeSceneLighting(float3 albedo, float3 normal, float3 worldPos, MaterialLighting material)
{
    float3 viewDir = normalize(CameraPosition.xyz - worldPos);

    float ambient = LightMeta.y;
    if (material.AmbientModel > 0.5f)
    {
        ambient = max(LightMeta.y, 0.18f) + lerp(0.10f, 0.32f, saturate(normal.y * 0.5f + 0.5f));
    }
    float shininess = LightMeta.z;
    int lightCount = (int)LightMeta.x;

    float3 diffuseAccum = float3(0.0f, 0.0f, 0.0f);
    float3 specularAccum = float3(0.0f, 0.0f, 0.0f);

    [loop]
    for (int i = 0; i < 8; ++i)
    {
        if (i >= lightCount)
        {
            break;
        }

        if (LightParams[i].z < 0.5f)
        {
            continue;
        }

        float3 toLight = LightPositions[i].xyz - worldPos;
        float dist = length(toLight);
        float radius = max(LightParams[i].y, 0.001f);
        float3 lightDir = toLight / max(dist, 0.0001f);
        float rangeFade = saturate(1.0f - dist / radius);
        float attenuation = 1.0f / (1.0f + (dist * dist) / max(radius * radius, 0.001f));
        attenuation *= rangeFade;

        float diffuse = max(dot(normal, lightDir), 0.0f);
        float3 reflectDir = reflect(-lightDir, normal);
        float specular = pow(max(dot(viewDir, reflectDir), 0.0f), shininess);
        float3 lightContribution = LightColors[i].xyz * LightParams[i].x * attenuation * 1.45f;

        diffuseAccum += diffuse * lightContribution;
        specularAccum += specular * lightContribution;
    }

    float3 directionalToLight = normalize(-LightDirection.xyz);
    float viewDepth = length(CameraPosition.xyz - worldPos);
    float shadow = ComputeShadowFactor(worldPos, normal, directionalToLight, viewDepth);

    float3 directionalColor = DirectionalLightColorIntensity.rgb;
    float directionalIntensity = DirectionalLightColorIntensity.a;
    float directionalDiffuse = max(dot(normal, directionalToLight), 0.0f);
    float3 directionalReflect = reflect(-directionalToLight, normal);
    float directionalSpecular = pow(max(dot(viewDir, directionalReflect), 0.0f), shininess);
    float3 directionalContribution = directionalColor * directionalIntensity;

    diffuseAccum += directionalDiffuse * directionalContribution * shadow;
    specularAccum += directionalSpecular * directionalContribution * shadow;

    return albedo * (ambient + diffuseAccum) + material.SpecularWeight * specularAccum;
}

#endif
