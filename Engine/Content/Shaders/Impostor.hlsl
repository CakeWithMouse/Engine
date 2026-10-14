#include "Common.hlsli"
#include "Instancing.hlsli"

// Farthest detail level: a camera-facing quad that samples the pre-rendered atlas of the model.
// Colour and object-space normal come from the atlas, so impostors are lit by the same light as
// the meshes and keep working in the deferred pass.
static const float UnitSpecularWeight = 0.35f;
static const float UnitAmbientModel = 1.0f;
static const float CoverageThreshold = 0.35f;

cbuffer ImpostorBuffer : register(b1)
{
    float4 AtlasParams; // x yaw tiles, y pitch tiles, z min pitch (rad), w pitch step (rad)
    float4 ModelParams; // x radius, y centre height, z tile inset in uv, w unused
};

StructuredBuffer<InstData> InstBuf : register(t0);
Texture2D impostorAlbedo : register(t0);
Texture2D impostorNormal : register(t1);
SamplerState impostorSampler : register(s0);

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 tileUv : TEXCOORD0;   // 0..1 inside the tile
    float2 tileIndex : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
    float3 axisRight : TEXCOORD3;
    float3 axisForward : TEXCOORD4;
    float fade : TEXCOORD5;
    float4 color : COLOR;
};

PS_IN VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    PS_IN output = (PS_IN)0;

    InstData instance = InstBuf[instanceId];
    const float3 worldPos = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), instance.World).xyz;
    const float3 scaledRight = mul(float4(1.0f, 0.0f, 0.0f, 0.0f), instance.World).xyz;
    const float3 scaledForward = mul(float4(0.0f, 0.0f, 1.0f, 0.0f), instance.World).xyz;
    const float scale = max(length(scaledRight), 0.0001f);
    const float3 axisRight = scaledRight / scale;
    const float3 axisForward = normalize(scaledForward);

    const float3 centre = worldPos + float3(0.0f, ModelParams.y * scale, 0.0f);
    const float3 viewDir = normalize(CameraPosition.xyz - centre);

    // Camera-facing quad with its roll locked to world up; the pitch rows of the atlas carry
    // the foreshortening of a top-down view.
    float3 quadRight = cross(float3(0.0f, 1.0f, 0.0f), viewDir);
    if (dot(quadRight, quadRight) < 1e-6f)
    {
        quadRight = float3(1.0f, 0.0f, 0.0f);
    }
    quadRight = normalize(quadRight);
    const float3 quadUp = normalize(cross(viewDir, quadRight));

    const float2 corners[4] = {float2(-1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f)};
    const float2 corner = corners[vertexId];
    const float radius = ModelParams.x * scale;
    const float3 positionWS = centre + quadRight * corner.x * radius + quadUp * corner.y * radius;

    const float4 viewPos = mul(float4(positionWS, 1.0f), viewMatrix);
    output.pos = mul(viewPos, projectionMatrix);
    output.worldPos = positionWS;
    output.axisRight = axisRight;
    output.axisForward = axisForward;
    output.color = ObjectColor * instance.Color;
    output.fade = instance.Params.x;

    // Tile that matches the direction the camera looks from, in the model's own frame.
    const float localX = dot(viewDir, axisRight);
    const float localZ = dot(viewDir, axisForward);
    const float yawStep = 6.2831853f / max(AtlasParams.x, 1.0f);
    float yawIndex = round(atan2(localX, localZ) / yawStep);
    yawIndex = fmod(yawIndex + AtlasParams.x, AtlasParams.x);

    const float pitchAngle = asin(clamp(viewDir.y, -1.0f, 1.0f));
    const float pitchIndex = clamp(round((pitchAngle - AtlasParams.z) / max(AtlasParams.w, 1e-4f)),
                                   0.0f, AtlasParams.y - 1.0f);

    output.tileUv = float2(corner.x * 0.5f + 0.5f, 0.5f - corner.y * 0.5f);
    output.tileIndex = float2(yawIndex, pitchIndex);
    return output;
}

float2 AtlasUv(PS_IN input)
{
    // Stay inside the tile: neighbouring tiles must not bleed in through filtering.
    const float2 clamped = clamp(input.tileUv, ModelParams.z, 1.0f - ModelParams.z);
    return (input.tileIndex + clamped) / float2(AtlasParams.x, AtlasParams.y);
}

float3 ImpostorNormal(PS_IN input, float2 uv)
{
    const float3 objectNormal = impostorNormal.Sample(impostorSampler, uv).xyz * 2.0f - 1.0f;
    const float3 worldNormal = input.axisRight * objectNormal.x +
                               float3(0.0f, 1.0f, 0.0f) * objectNormal.y +
                               input.axisForward * objectNormal.z;
    return normalize(worldNormal);
}

#if defined(DEFERRED_GBUFFER)
GBufferOut PSMain(PS_IN input)
{
    const float2 uv = AtlasUv(input);
    const float4 sampled = impostorAlbedo.Sample(impostorSampler, uv);
    clip(sampled.a - CoverageThreshold);
    ClipByFade(input.fade, input.pos.xy);

    MaterialLighting material = MakeMaterialLighting(UnitSpecularWeight, UnitAmbientModel);
    return EncodeGBuffer(sampled.rgb * input.color.rgb, ImpostorNormal(input, uv), input.worldPos, material);
}
#else
float4 PSMain(PS_IN input) : SV_Target
{
    const float2 uv = AtlasUv(input);
    const float4 sampled = impostorAlbedo.Sample(impostorSampler, uv);
    clip(sampled.a - CoverageThreshold);
    ClipByFade(input.fade, input.pos.xy);

    MaterialLighting material = MakeMaterialLighting(UnitSpecularWeight, UnitAmbientModel);
    const float3 litColor = ComputeSceneLighting(sampled.rgb * input.color.rgb,
                                                 ImpostorNormal(input, uv), input.worldPos, material);
    return float4(saturate(litColor), 1.0f);
}
#endif
