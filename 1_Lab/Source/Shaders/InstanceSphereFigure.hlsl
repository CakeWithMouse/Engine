cbuffer ConstantBuffer : register(b0)
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 invViewMatrix;
    float4x4 invProjectionMatrix;
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

struct InstData
{
    float4x4 World;
    float4 color;
};

StructuredBuffer<InstData> InstBuf : register(t0);

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

float3 NormalizeHDRColor(float3 color)
{
    float maxComp = max(max(color.r, color.g), color.b);
    if (maxComp > 1.0f)
    {
        return color / maxComp;
    }
    return color;
}

float3 ToneMapReinhard(float3 color)
{
    return color / (1.0f + color);
}

PS_IN VSMain(VS_IN input, uint ind : SV_InstanceID)
{
    PS_IN output = (PS_IN)0;
    InstData instanceData = InstBuf[ind];

    float4 worldPos = mul(input.pos, instanceData.World);
    float4 viewPos = mul(worldPos, viewMatrix);
    float4 projectionPos = mul(viewPos, projectionMatrix);
    float3 objectCenter = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), instanceData.World).xyz;

    output.pos = projectionPos;
    output.col = instanceData.color;
    output.worldPos = worldPos.xyz;
    output.normal = normalize(worldPos.xyz - objectCenter);
    output.objectCenter = objectCenter;

    return output;
}

#if defined(DEFERRED_GBUFFER)
GBufferOut PSMain(PS_IN input)
{
    float3 normal = normalize(input.normal);
    float3 baseColor = NormalizeHDRColor(input.col.rgb);

    GBufferOut output;
    output.Albedo = float4(saturate(baseColor), 1.0f);
    output.Normal = float4(normal * 0.5f + 0.5f, 1.0f);
    output.Material = float4(input.worldPos, 1.0f);
    return output;
}
#else
float4 PSMain(PS_IN input) : SV_Target
{
    float emissiveFactor = saturate(input.col.a - 1.0f);
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
        float attenuation = saturate(1.0f - dist / radius);
        attenuation *= attenuation;

        float diffuse = max(dot(normal, lightDir), 0.0f);
        float3 reflectDir = reflect(-lightDir, normal);
        float specular = pow(max(dot(viewDir, reflectDir), 0.0f), shininess);
        float3 lightContribution = LightColors[i].xyz * LightParams[i].x * attenuation;

        diffuseAccum += diffuse * lightContribution;
        specularAccum += specular * lightContribution;
    }

    float4 colorOut = input.col;
    colorOut.rgb = NormalizeHDRColor(colorOut.rgb);

    float3 sunPos = float3(0.0f, 0.0f, 0.0f);
    float3 sunColor = float3(1.0f, 0.86f, 0.62f);
    float sunIntensity = 2.2f;
    float sunRadius = 4200.0f;
    float3 toSun = sunPos - input.worldPos;
    float distToSun = length(toSun);
    float3 sunDir = toSun / max(distToSun, 0.0001f);
    float sunAttenuation = saturate(1.0f - distToSun / sunRadius);
    sunAttenuation *= sunAttenuation;
    float sunDiffuse = max(dot(normal, sunDir), 0.0f);
    float3 sunReflect = reflect(-sunDir, normal);
    float sunSpecular = pow(max(dot(viewDir, sunReflect), 0.0f), shininess);
    float3 sunContribution = sunColor * sunIntensity * sunAttenuation;
    diffuseAccum += sunDiffuse * sunContribution;
    specularAccum += sunSpecular * sunContribution;

    float3 litColor = colorOut.rgb * (ambient + diffuseAccum) + 0.15f * specularAccum;
    float3 emissiveColor = colorOut.rgb * (1.15f + 0.85f * emissiveFactor);
    litColor = lerp(litColor, emissiveColor, emissiveFactor);
    colorOut.rgb = ToneMapReinhard(max(litColor, 0.0f));

    if (colorOut.r > 0.9f && colorOut.g > 0.8f && colorOut.b < 0.5f)
    {
        colorOut.rgb *= 1.08f;
    }

    return float4(saturate(colorOut.rgb), 1.0f);
}
#endif
