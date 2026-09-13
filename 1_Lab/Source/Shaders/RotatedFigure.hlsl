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

#if defined(DEFERRED_LIGHTING)
struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D gAlbedoTex : register(t0);
Texture2D gNormalTex : register(t1);
Texture2D gMaterialTex : register(t2);
Texture2D gDepthTex : register(t3);
Texture2DArray shadowMapTex : register(t4);
SamplerState gBufferSampler : register(s0);
SamplerState shadowSampler : register(s4);

VS_OUT VSMain(uint vertexID : SV_VertexID)
{
    VS_OUT output = (VS_OUT)0;

    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    float2 uv[3] =
    {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };

    output.pos = float4(positions[vertexID], 0.0f, 1.0f);
    output.uv = uv[vertexID];
    return output;
}
#else
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
};

Texture2DArray shadowMapTex : register(t4);
SamplerState shadowSampler : register(s4);

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;

    float4 worldPos = mul(input.pos, worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    output.pos = mul(viewPos, projectionMatrix);

    output.col = ObjectColor;
    output.worldPos = worldPos.xyz;
    output.normal = float3(0.0f, 1.0f, 0.0f);
    return output;
}
#endif

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

float3 CalculateLighting(float3 albedo, float3 normal, float3 worldPos)
{
    float3 viewDir = normalize(CameraPosition.xyz - worldPos);

    float ambient = max(LightMeta.y, 0.18f);
    float hemiAmbient = lerp(0.10f, 0.32f, saturate(normal.y * 0.5f + 0.5f));
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

    return albedo * (ambient + hemiAmbient + diffuseAccum) + 0.12f * specularAccum;
}

#if defined(DEFERRED_LIGHTING)
float3 ReconstructWorldPosition(float2 uv, float depth)
{
    // Direct3D stores post-projection depth in [0, 1], so z must stay in that range
    // during reconstruction. Mapping it to [-1, 1] shifts the whole scene in view space.
    float4 clipPos = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f);
    float4 viewPos = mul(clipPos, invProjectionMatrix);
    viewPos /= max(viewPos.w, 0.0001f);
    float4 worldPos = mul(viewPos, invViewMatrix);
    return worldPos.xyz / max(worldPos.w, 0.0001f);
}

float3 ReconstructViewPosition(float2 uv, float depth)
{
    float4 clipPos = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f);
    float4 viewPos = mul(clipPos, invProjectionMatrix);
    viewPos /= max(viewPos.w, 0.0001f);
    return viewPos.xyz;
}

float4 PSMain(VS_OUT input) : SV_Target
{
    // Fullscreen triangle UVs intentionally go outside [0, 1] at the vertices.
    // Clamping them per-pixel distorts interpolation and makes the G-buffer look
    // as if it is glued to a screen corner.
    float2 uv = input.uv;
    uint gBufferWidth = 0;
    uint gBufferHeight = 0;
    gDepthTex.GetDimensions(gBufferWidth, gBufferHeight);
    float2 clampedUv = saturate(uv);
    int2 pixelCoord = int2(clampedUv * float2(max((int)gBufferWidth - 1, 0), max((int)gBufferHeight - 1, 0)));

    float4 albedoSample = gAlbedoTex.Load(int3(pixelCoord, 0));
    float4 normalSample = gNormalTex.Load(int3(pixelCoord, 0));
    float4 worldPosSample = gMaterialTex.Load(int3(pixelCoord, 0));
    float depth = gDepthTex.Load(int3(pixelCoord, 0)).r;
    float2 resolvedUv = (float2(pixelCoord) + 0.5f) / float2(max(gBufferWidth, 1), max(gBufferHeight, 1));

    const int debugMode = (int)(ObjectColor.x + 0.5f);
    if (debugMode == 1)
    {
        return float4(albedoSample.rgb, 1.0f);
    }
    if (debugMode == 2)
    {
        return float4(normalSample.rgb, 1.0f);
    }
    if (debugMode == 3)
    {
        if (depth >= 0.99999f)
        {
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        float viewDepth = max(ReconstructViewPosition(resolvedUv, depth).z, 0.0f);
        float visualDepth = saturate(log2(1.0f + viewDepth) / log2(10001.0f));
        return float4(visualDepth.xxx, 1.0f);
    }
    if (debugMode == 4)
    {
        return float4(abs(worldPosSample.xyz) * 0.01f, 1.0f);
    }

    if (depth >= 1.0f)
    {
        return float4(albedoSample.rgb, 1.0f);
    }

    float3 normal = normalize(normalSample.xyz * 2.0f - 1.0f);
    float3 worldPos = worldPosSample.xyz;
    if (abs(worldPos.x) + abs(worldPos.y) + abs(worldPos.z) < 0.0001f)
    {
        worldPos = ReconstructWorldPosition(resolvedUv, depth);
    }
    float3 litColor = CalculateLighting(albedoSample.rgb, normal, worldPos);
    return float4(saturate(litColor), 1.0f);
}
#elif defined(DEFERRED_GBUFFER)
GBufferOut PSMain(PS_IN input)
{
    float3 viewDir = normalize(CameraPosition.xyz - input.worldPos);
    float3 normal = normalize(cross(ddx(input.worldPos), ddy(input.worldPos)));
    if (dot(normal, viewDir) < 0.0f)
    {
        normal = -normal;
    }

    GBufferOut output;
    output.Albedo = float4(saturate(input.col.rgb), 1.0f);
    output.Normal = float4(normalize(normal) * 0.5f + 0.5f, 1.0f);
    output.Material = float4(input.worldPos, 1.0f);
    return output;
}
#else
float4 PSMain(PS_IN input) : SV_Target
{
    float3 viewDir = normalize(CameraPosition.xyz - input.worldPos);
    float3 normal = normalize(cross(ddx(input.worldPos), ddy(input.worldPos)));
    if (dot(normal, viewDir) < 0.0f)
    {
        normal = -normal;
    }

    float3 litColor = CalculateLighting(input.col.rgb, normal, input.worldPos);
    return float4(saturate(litColor), input.col.a);
}
#endif
