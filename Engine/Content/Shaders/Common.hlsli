#ifndef ENGINE_COMMON_HLSLI
#define ENGINE_COMMON_HLSLI

// Must match MaxShadowCascades in Engine/Include/Engine/Render/ShaderConstants.h.
#define MAX_SHADOW_CASCADES 3
// Must match StaticCasterCascades there: how many near cascades also draw the static scene.
#define STATIC_CASTER_CASCADES 2
// Must match PointShadowMapSize and PointShadowNearPlane there.
#define POINT_SHADOW_SIZE 1024.0f
#define POINT_SHADOW_NEAR 0.5f

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
    float4 LightMeta;            // x light count, y ambient, z shininess, w explicit scene lighting (no implicit ambient)
    float4 ReflectionData;       // x has cube map, y strength, z fresnel power, w is skybox
    // Must match MaxShadowCascades in Engine/Include/Engine/Render/ShaderConstants.h.
    float4x4 LightViewProjection[MAX_SHADOW_CASCADES];
    float4 CascadeSplits;
    float4 ShadowParams;         // x enabled, y cascades, z bias, w texel size
    float4 LightDirection;
    float4 DirectionalLightColorIntensity;
    float4x4 normalMatrix;              // inverse-transpose of worldMatrix
    float4x4 StaticShadowViewProjection; // baked shadows of the static scene
    float4 StaticShadowParams;           // x enabled, y texel size, z bias
    float4 TimeParams;                   // x real seconds, y real frame delta, z scene time
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
Texture2D staticShadowMapTex : register(t5);
// The comparison happens in the sampler: the hardware tests the four nearest texels and blends the
// results. Filtering the depth first and testing once gives a hard edge along the rows of texels.
SamplerComparisonState shadowCompareSampler : register(s5);

/**
 * Nine bilinear comparison taps with binomial weights: together a smooth tent about four texels
 * wide, at the cost the old nine point tests had. The weights must add up to one.
 */
static const float ShadowTapWeight[3] = {0.25f, 0.5f, 0.25f};

/**
 * Shadow of the static scene (terrain, buildings): one map fixed to the world, rendered once.
 * It does not follow the camera, so it is not re-rendered while the camera moves.
 */
float ComputeStaticShadowFactor(float3 worldPos, float3 normal, float3 toLightDir)
{
    if (StaticShadowParams.x < 0.5f)
    {
        return 1.0f;
    }

    float4 lightPos = mul(float4(worldPos, 1.0f), StaticShadowViewProjection);
    float3 projCoords = lightPos.xyz / max(lightPos.w, 0.0001f);

    float2 uv;
    uv.x = projCoords.x * 0.5f + 0.5f;
    uv.y = -projCoords.y * 0.5f + 0.5f;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || projCoords.z <= 0.0f || projCoords.z >= 1.0f)
    {
        return 1.0f;
    }

    float baseBias = max(StaticShadowParams.z, 0.00005f);
    float slopeBias = 1.0f - saturate(dot(normal, toLightDir));
    float bias = max(baseBias * slopeBias, baseBias * 0.25f);
    float texelSize = max(StaticShadowParams.y, 0.0001f);

    float visibility = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2((float)x, (float)y) * texelSize;
            visibility += ShadowTapWeight[y + 1] * ShadowTapWeight[x + 1] *
                staticShadowMapTex.SampleCmpLevelZero(shadowCompareSampler, uv + offset, projCoords.z - bias);
        }
    }

    return visibility;
}

int SelectCascade(float viewDepth)
{
    if (viewDepth < CascadeSplits.x) return 0;
    if (viewDepth < CascadeSplits.y) return 1;
    return 2;
}

/** Edges of a cascade along the view; CascadeSplits.w is the near edge of the first one. */
float CascadeStart(int cascadeIndex)
{
    return cascadeIndex == 0 ? CascadeSplits.w : (cascadeIndex == 1 ? CascadeSplits.x : CascadeSplits.y);
}

float CascadeEnd(int cascadeIndex)
{
    return cascadeIndex == 0 ? CascadeSplits.x : (cascadeIndex == 1 ? CascadeSplits.y : CascadeSplits.z);
}

/** One cascade lookup. Fully lit when the pixel falls outside the map of this cascade. */
float SampleCascade(int cascadeIndex, float3 worldPos, float bias)
{
    float4 lightPos = mul(float4(worldPos, 1.0f), LightViewProjection[cascadeIndex]);
    float3 projCoords = lightPos.xyz / max(lightPos.w, 0.0001f);

    float2 uv;
    uv.x = projCoords.x * 0.5f + 0.5f;
    uv.y = -projCoords.y * 0.5f + 0.5f;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || projCoords.z <= 0.0f || projCoords.z >= 1.0f)
    {
        return 1.0f;
    }

    float texelSize = max(ShadowParams.w, 0.0001f);
    float visibility = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2((float)x, (float)y) * texelSize;
            visibility += ShadowTapWeight[y + 1] * ShadowTapWeight[x + 1] *
                shadowMapTex.SampleCmpLevelZero(shadowCompareSampler, float3(uv + offset, cascadeIndex), projCoords.z - bias);
        }
    }
    return visibility;
}

float ComputeShadowFactor(float3 worldPos, float3 normal, float3 toLightDir, float viewDepth)
{
    if (ShadowParams.x < 0.5f)
    {
        return 1.0f;
    }

    float baseBias = max(ShadowParams.z, 0.00005f);
    float slopeBias = 1.0f - saturate(dot(normal, toLightDir));
    float bias = max(baseBias * slopeBias, baseBias * 0.25f);

    // The near cascades draw the static scene themselves at their own, much finer density. Only
    // the far ones rely on the baked map: taking the darker of the two everywhere would let its
    // coarse edge show around every sharp shadow, which is exactly how columns came out blocky.
    const float baked = ComputeStaticShadowFactor(worldPos, normal, toLightDir);

    int cascadeIndex = SelectCascade(viewDepth);
    float visibility = SampleCascade(cascadeIndex, worldPos, bias);
    if (cascadeIndex >= STATIC_CASTER_CASCADES)
    {
        visibility = min(visibility, baked);
    }

    // Neighbouring cascades hold the same shadow at different resolutions; the last tenth of a
    // cascade fades into the next so the change has no hard edge.
    if (cascadeIndex < MAX_SHADOW_CASCADES - 1)
    {
        float start = CascadeStart(cascadeIndex);
        float end = CascadeEnd(cascadeIndex);
        float band = max((end - start) * 0.1f, 0.001f);
        float blend = saturate((viewDepth - (end - band)) / band);
        if (blend > 0.0f)
        {
            float next = SampleCascade(cascadeIndex + 1, worldPos, bias);
            if (cascadeIndex + 1 >= STATIC_CASTER_CASCADES)
            {
                next = min(next, baked);
            }
            visibility = lerp(visibility, next, blend);
        }
    }

    return visibility;
}

// Point lights + directional light with cascaded shadows. Returns the lit colour (not clamped).
// Must match PointShadowTextureSlot in ShaderConstants.h. Materials own t6 and up, so the cube array
// sits well past them.
TextureCubeArray pointShadowTex : register(t32);

/** Depth a cube face stores for a point this far along its axis; same mapping as its projection. */
float PointShadowDepth(float axisDistance, float farPlane)
{
    return farPlane / (farPlane - POINT_SHADOW_NEAR) * (1.0f - POINT_SHADOW_NEAR / max(axisDistance, POINT_SHADOW_NEAR));
}

/** How much of a point light reaches this surface: 0 fully blocked, 1 nothing in the way. */
float ComputePointShadow(int cube, float3 worldPos, float3 normal, float3 lightPos, float radius)
{
    float3 fromLight = worldPos - lightPos;
    const float distanceToLight = length(fromLight);

    // One texel of a cube face at this distance, in world units. Everything below scales with it:
    // a fixed offset would be far too large next to the light and too small at its edge.
    const float texel = 2.0f * distanceToLight / POINT_SHADOW_SIZE;
    fromLight += normal * (texel * 1.5f);

    const float3 direction = normalize(fromLight);
    const float3 side = normalize(cross(direction, abs(direction.y) < 0.9f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f)));
    const float3 up = cross(direction, side);

    float visibility = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            const float3 tap = fromLight + (side * (float)x + up * (float)y) * texel;
            const float3 absolute = abs(tap);
            const float axisDistance = max(absolute.x, max(absolute.y, absolute.z));
            visibility += ShadowTapWeight[y + 1] * ShadowTapWeight[x + 1] *
                pointShadowTex.SampleCmpLevelZero(shadowCompareSampler, float4(tap, (float)cube),
                                                  PointShadowDepth(axisDistance - texel * 0.5f, radius));
        }
    }
    return visibility;
}

float3 ComputeSceneLighting(float3 albedo, float3 normal, float3 worldPos, MaterialLighting material)
{
    float3 viewDir = normalize(CameraPosition.xyz - worldPos);

    float ambient = LightMeta.y;
    if (material.AmbientModel > 0.5f && LightMeta.w < 0.5f)
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
        // w is the cube of this light plus one. A surface turned away or out of range has nothing
        // to lose, so it skips the lookups.
        if (LightParams[i].w > 0.5f && diffuse > 0.0f && rangeFade > 0.0f)
        {
            attenuation *= ComputePointShadow((int)LightParams[i].w - 1, worldPos, normal, LightPositions[i].xyz, radius);
        }
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
