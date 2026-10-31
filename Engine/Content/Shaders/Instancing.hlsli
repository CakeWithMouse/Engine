#ifndef ENGINE_INSTANCING_HLSLI
#define ENGINE_INSTANCING_HLSLI

// One element of the instance stream.
// Layout must match InstData in Engine/Include/Engine/Render/ShaderConstants.h.
struct InstData
{
    float4x4 World;
    float4 Color;
    // x = LOD transition fade: magnitude is the weight (1 = fully visible), sign says which side
    // of the transition this copy is on (negative = the level that fades in). yzw belong to
    // whatever draws the copy - the animation clip, or the per-instance data of its material.
    float4 Params;
};

/** Ordered 4x4 dither threshold for the pixel. */
float DitherThreshold(float2 pixelPosition)
{
    static const float pattern[16] =
    {
        0.0625f, 0.5625f, 0.1875f, 0.6875f,
        0.8125f, 0.3125f, 0.9375f, 0.4375f,
        0.2500f, 0.7500f, 0.1250f, 0.6250f,
        1.0000f, 0.5000f, 0.8750f, 0.3750f
    };
    const int2 cell = int2(pixelPosition) & 3;
    return pattern[cell.y * 4 + cell.x];
}

/**
 * The same pattern read along the other axis. Two tests on one pixel (coverage and cross-fade)
 * must not use the same ordering, or the pixels one keeps are the ones the other throws away.
 */
float DitherThresholdCrossed(float2 pixelPosition)
{
    return DitherThreshold(float2(pixelPosition.y, pixelPosition.x) + 2.0f);
}

/**
 * Where this copy starts reading the dither pattern. Without it every copy tests the same 4x4
 * cells at the same screen positions, and a field of small copies fades as one coarse grid
 * instead of as individual copies. Derived from the world position, which every path already
 * has in the pixel shader, so it costs no room in the instance stream.
 */
float2 DitherPhase(float3 worldPosition)
{
    const float hash = frac(dot(worldPosition, float3(0.0937f, 0.1731f, 0.2659f)) * 43758.5453f);
    return float2(floor(hash * 4.0f), floor(frac(hash * 7.0f) * 4.0f));
}

/**
 * Cross-fade between two detail levels without alpha blending: both levels are drawn and each
 * pixel keeps only one of them. Opaque geometry stays opaque, so the G-buffer and depth stay valid.
 *
 * The two levels must split the pixels, not test the same threshold independently: with the same
 * test each of them drops the pixels above its own fade, and in the middle of the band half the
 * pixels are dropped twice. On a field of small units that shows up as a bright ring of ground at
 * every switching distance. The level that fades in therefore takes exactly the pixels the other
 * one gives up, and it is marked by a negative fade.
 */
void ClipByFade(float signedFade, float2 pixelPosition)
{
    const float weight = abs(signedFade);
    if (weight >= 0.999f)
    {
        return;
    }

    const float threshold = DitherThreshold(pixelPosition);
    clip(signedFade < 0.0f ? threshold - (1.0f - weight) : weight - threshold);
}

#endif
