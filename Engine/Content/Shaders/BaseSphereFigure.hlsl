cbuffer ConstantBuffer: register (b0)
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
    float4 LightMeta;
    float4 ReflectionData;
    float4x4 LightViewProjection[3];
    float4 CascadeSplits;
    float4 ShadowParams;
    float4 LightDirection;
    float4 DirectionalLightColorIntensity;
};

struct GBufferOut
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Material : SV_Target2;
};

struct VS_IN
{
    float4 pos : POSITION;
    float4 col : COLOR;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 objectCenter : TEXCOORD2;
};

Texture2DArray shadowMapTex : register(t4);
SamplerState shadowSampler : register(s4);

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;

    float4 worldPos = mul(input.pos, worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    float4 projectionPos = mul(viewPos, projectionMatrix);
    float3 objectCenter = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), worldMatrix).xyz;

    output.pos = projectionPos;
    output.col = ObjectColor;
    output.worldPos = worldPos.xyz;
    output.normal = normalize(worldPos.xyz - objectCenter);
    output.objectCenter = objectCenter;
    return output;
}

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

float3 ToneMapReinhard(float3 color)
{
    return color / (1.0f + color);
}

#if defined(DEFERRED_GBUFFER)
GBufferOut PSMain(PS_IN input)
{
    float3 normal = normalize(input.normal);

    GBufferOut output;
    output.Albedo = float4(saturate(input.col.rgb), 1.0f);
    output.Normal = float4(normal * 0.5f + 0.5f, 1.0f);
    output.Material = float4(input.worldPos, 1.0f);
    return output;
}
#else
float4 PSMain(PS_IN input) : SV_Target
{
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(CameraPosition.xyz - input.worldPos);

    float ambient = LightMeta.y;
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

        float3 toLight = LightPositions[i].xyz - input.worldPos;
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
    float viewDepth = length(CameraPosition.xyz - input.worldPos);
    float shadow = ComputeShadowFactor(input.worldPos, normal, directionalToLight, viewDepth);

    float3 directionalColor = DirectionalLightColorIntensity.rgb;
    float directionalIntensity = DirectionalLightColorIntensity.a;
    float directionalDiffuse = max(dot(normal, directionalToLight), 0.0f);
    float3 directionalReflect = reflect(-directionalToLight, normal);
    float directionalSpecular = pow(max(dot(viewDir, directionalReflect), 0.0f), shininess);
    float3 directionalContribution = directionalColor * directionalIntensity;

    diffuseAccum += directionalDiffuse * directionalContribution * shadow;
    specularAccum += directionalSpecular * directionalContribution * shadow;

    float4 color_out = input.col;
    float3 litColor = color_out.rgb * (ambient + diffuseAccum) + 0.35f * specularAccum;

    if (length(input.objectCenter) < 1.0f)
    {
        float3 emissive = color_out.rgb * 1.6f + directionalColor * 0.9f;
        litColor += emissive;
    }

    color_out.rgb = ToneMapReinhard(max(litColor, 0.0f));

    if (color_out.r > 0.9f && color_out.g > 0.8f && color_out.b < 0.5f)
    {
        color_out.rgb *= 1.12f;
    }

    return float4(saturate(color_out.rgb), color_out.a);
}
#endif
