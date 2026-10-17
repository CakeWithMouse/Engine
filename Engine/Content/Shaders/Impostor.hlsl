#include "Common.hlsli"
#include "Instancing.hlsli"

// Farthest detail level: a camera-facing quad that samples the pre-rendered atlas of the model.
// Colour and object-space normal come from the atlas, so impostors are lit by the same light as
// the meshes and keep working in the deferred pass.
static const float UnitSpecularWeight = 0.35f;
static const float UnitAmbientModel = 1.0f;

cbuffer ImpostorBuffer : register(b1)
{
    float4 AtlasParams; // x yaw tiles, y pitch tiles, z min pitch (rad), w pitch step (rad)
    float4 ModelParams; // x radius, y centre height, z texels per tile, w coarsest mip level
};

StructuredBuffer<InstData> InstBuf : register(t0);
Texture2D impostorAlbedo : register(t0);
Texture2D impostorNormal : register(t1);
SamplerState impostorSampler : register(s0);

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 tileUv : TEXCOORD0;   // 0..1 inside the tile
    float2 tileIndex : TEXCOORD1; // y is fractional: the two neighbouring rows are blended
    float3 worldPos : TEXCOORD2;
    float3 axisRight : TEXCOORD3;
    float3 axisForward : TEXCOORD4;
    // xy transition weight and direction, zw this unit's own phase in the dither pattern.
    float4 fade : TEXCOORD5;
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
    output.fade = instance.Params;

    // Tile that matches the direction the camera looks from, in the model's own frame.
    const float localX = dot(viewDir, axisRight);
    const float localZ = dot(viewDir, axisForward);
    const float yawStep = 6.2831853f / max(AtlasParams.x, 1.0f);
    float yawIndex = round(atan2(localX, localZ) / yawStep);
    yawIndex = fmod(yawIndex + AtlasParams.x, AtlasParams.x);

    // The elevation of the view changes smoothly with the distance to the camera, so snapping it
    // to a row would paint rings of equal distance on flat ground. The index is kept fractional
    // and the pixel shader mixes the two rows around it.
    const float pitchAngle = asin(clamp(viewDir.y, -1.0f, 1.0f));
    const float pitchIndex = clamp((pitchAngle - AtlasParams.z) / max(AtlasParams.w, 1e-4f),
                                   0.0f, AtlasParams.y - 1.0f);

    output.tileUv = float2(corner.x * 0.5f + 0.5f, 0.5f - corner.y * 0.5f);
    output.tileIndex = float2(yawIndex, pitchIndex);
    return output;
}

/** What the atlas says about this pixel: colour, coverage and the object-space normal. */
struct ImpostorSample
{
    float3 Albedo;
    float Coverage;
    float3 Normal;
};

float2 AtlasUv(float2 tileUv, float2 tileIndex, float inset)
{
    // Stay inside the tile: a neighbouring view must not bleed in through filtering. The margin
    // is half a texel of the mip in use, so it grows as the sprite gets smaller.
    const float2 clamped = clamp(tileUv, inset, 1.0f - inset);
    return (tileIndex + clamped) / float2(AtlasParams.x, AtlasParams.y);
}

ImpostorSample SampleRow(PS_IN input, float row, float mip, float inset)
{
    const float2 uv = AtlasUv(input.tileUv, float2(input.tileIndex.x, row), inset);
    const float4 albedo = impostorAlbedo.SampleLevel(impostorSampler, uv, mip);
    ImpostorSample result;
    // Mips average the model with the transparent background, so colour comes back scaled by
    // coverage; dividing it out keeps the sprite from darkening towards its edges.
    result.Coverage = albedo.a;
    result.Albedo = albedo.rgb / max(albedo.a, 1e-3f);
    result.Normal = impostorNormal.SampleLevel(impostorSampler, uv, mip).xyz * 2.0f - 1.0f;
    return result;
}

/** Colour, coverage and normal for this pixel, mixed across the two neighbouring elevation rows. */
ImpostorSample SampleImpostor(PS_IN input)
{
    const float texelsPerTile = max(ModelParams.z, 1.0f);
    const float2 atlasUv = AtlasUv(input.tileUv, input.tileIndex, 0.0f);
    const float mip = clamp(impostorAlbedo.CalculateLevelOfDetail(impostorSampler, atlasUv),
                            0.0f, ModelParams.w);
    const float inset = 0.5f * exp2(ceil(mip)) / texelsPerTile;

    const float lowRow = floor(input.tileIndex.y);
    const float blend = input.tileIndex.y - lowRow;
    const ImpostorSample low = SampleRow(input, lowRow, mip, inset);
    if (blend <= 0.001f)
    {
        return low;
    }

    const ImpostorSample high = SampleRow(input, min(lowRow + 1.0f, AtlasParams.y - 1.0f), mip, inset);
    ImpostorSample result;
    result.Albedo = lerp(low.Albedo, high.Albedo, blend);
    result.Coverage = lerp(low.Coverage, high.Coverage, blend);
    result.Normal = lerp(low.Normal, high.Normal, blend);
    return result;
}

/**
 * A sprite is a few pixels wide, so its coverage has to be spent, not rounded off. Testing it
 * against a fixed value eats the silhouette as the unit gets smaller, and because the amount
 * eaten follows the distance to the camera, a dense field thins out in rings around the camera
 * and in stripes along the rows of a formation. Keeping the pixel with the probability of its
 * coverage makes the sprite occupy the area the model really has, at any size.
 */
void ClipByCoverage(float coverage, float2 pixelPosition)
{
    clip(coverage - DitherThresholdCrossed(pixelPosition));
}

float3 ImpostorNormal(PS_IN input, float3 objectNormal)
{
    const float3 worldNormal = input.axisRight * objectNormal.x +
                               float3(0.0f, 1.0f, 0.0f) * objectNormal.y +
                               input.axisForward * objectNormal.z;
    return normalize(worldNormal);
}

#if defined(DEFERRED_GBUFFER)
GBufferOut PSMain(PS_IN input)
{
    const ImpostorSample atlas = SampleImpostor(input);
    ClipByCoverage(atlas.Coverage, input.pos.xy + input.fade.zw);
    ClipByFade(input.fade.x, input.fade.y, input.pos.xy + input.fade.zw);

    MaterialLighting material = MakeMaterialLighting(UnitSpecularWeight, UnitAmbientModel);
    return EncodeGBuffer(atlas.Albedo * input.color.rgb, ImpostorNormal(input, atlas.Normal),
                         input.worldPos, material);
}
#else
float4 PSMain(PS_IN input) : SV_Target
{
    const ImpostorSample atlas = SampleImpostor(input);
    ClipByCoverage(atlas.Coverage, input.pos.xy + input.fade.zw);
    ClipByFade(input.fade.x, input.fade.y, input.pos.xy + input.fade.zw);

    MaterialLighting material = MakeMaterialLighting(UnitSpecularWeight, UnitAmbientModel);
    const float3 litColor = ComputeSceneLighting(atlas.Albedo * input.color.rgb,
                                                 ImpostorNormal(input, atlas.Normal),
                                                 input.worldPos, material);
    return float4(saturate(litColor), 1.0f);
}
#endif
