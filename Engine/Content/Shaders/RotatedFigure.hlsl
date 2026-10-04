#include "Common.hlsli"

// Default material (flat colour, faceted normals) and the deferred lighting pass.
static const float RotatedSpecularWeight = 0.12f;
static const float RotatedAmbientModel = 1.0f; // hemispherical ambient

#if defined(DEFERRED_LIGHTING)
struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D gAlbedoTex : register(t0);
Texture2D gNormalTex : register(t1);
Texture2D gWorldPositionTex : register(t2);
Texture2D gDepthTex : register(t3);
SamplerState gBufferSampler : register(s0);

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

float3 FacetNormal(float3 worldPos)
{
    float3 viewDir = normalize(CameraPosition.xyz - worldPos);
    float3 normal = normalize(cross(ddx(worldPos), ddy(worldPos)));
    return dot(normal, viewDir) < 0.0f ? -normal : normal;
}
#endif

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
    float4 worldPosSample = gWorldPositionTex.Load(int3(pixelCoord, 0));
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

    // Nothing was written by the geometry pass: keep the clear colour (skybox draws later).
    if (depth >= 1.0f)
    {
        discard;
    }

    float3 normal = normalize(normalSample.xyz * 2.0f - 1.0f);
    float3 worldPos = worldPosSample.xyz;
    if (worldPosSample.w < 0.5f)
    {
        worldPos = ReconstructWorldPosition(resolvedUv, depth);
    }

    MaterialLighting material = MakeMaterialLighting(albedoSample.a, normalSample.a);
    float3 litColor = ComputeSceneLighting(albedoSample.rgb, normal, worldPos, material);
    return float4(saturate(litColor), 1.0f);
}
#elif defined(DEFERRED_GBUFFER)
GBufferOut PSMain(PS_IN input)
{
    MaterialLighting material = MakeMaterialLighting(RotatedSpecularWeight, RotatedAmbientModel);
    return EncodeGBuffer(input.col.rgb, FacetNormal(input.worldPos), input.worldPos, material);
}
#else
float4 PSMain(PS_IN input) : SV_Target
{
    MaterialLighting material = MakeMaterialLighting(RotatedSpecularWeight, RotatedAmbientModel);
    float3 litColor = ComputeSceneLighting(input.col.rgb, FacetNormal(input.worldPos), input.worldPos, material);
    return float4(saturate(litColor), input.col.a);
}
#endif
