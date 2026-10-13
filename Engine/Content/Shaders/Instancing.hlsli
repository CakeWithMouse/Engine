#ifndef ENGINE_INSTANCING_HLSLI
#define ENGINE_INSTANCING_HLSLI

// One element of the instance stream.
// Layout must match InstData in Engine/Include/Engine/Render/ShaderConstants.h.
struct InstData
{
    float4x4 World;
    float4 Color;
    float4 Params; // x = LOD transition fade, 1 = fully visible
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
 * Cross-fade between two detail levels without alpha blending: both levels are drawn and each
 * pixel keeps only one of them. Opaque geometry stays opaque, so the G-buffer and depth stay valid.
 */
void ClipByFade(float fade, float2 pixelPosition)
{
    if (fade < 0.999f)
    {
        clip(fade - DitherThreshold(pixelPosition));
    }
}

#endif
