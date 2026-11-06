#pragma once

#include <Engine/Render/ShaderConstants.h>

#include <algorithm>
#include <cmath>

/** Stretch of the view axis the shadow cascades divide between them. */
struct ShadowCascadeRange
{
    float Near = 0.0f;
    float Far = 0.0f;
};

/**
 * The part of a shadow area the camera can see, measured along the view axis. The result is never
 * inverted: an area that lies entirely past the shadow distance, or behind the camera, collapses
 * to a thin slab at the matching end of the range instead of handing the splits a negative length.
 */
inline ShadowCascadeRange FitShadowCascadeRange(float nearPlane, float shadowDistance,
                                                float areaCenterDepth, float areaExtent)
{
    const float limit = std::max(shadowDistance, nearPlane + 1.0f);
    ShadowCascadeRange range;
    range.Near = std::min(std::max(areaCenterDepth - areaExtent, nearPlane), limit - 1.0f);
    range.Far = std::min(std::max(areaCenterDepth + areaExtent, range.Near + 1.0f), limit);
    return range;
}

/**
 * Practical split scheme: the logarithmic term keeps the near cascades tight, the uniform one
 * keeps the far cascades from collapsing. lambda 1 is purely logarithmic, 0 purely uniform.
 */
inline void SplitShadowCascadeRange(const ShadowCascadeRange& range, float lambda,
                                    float (&outSplits)[MaxShadowCascades + 1])
{
    outSplits[0] = range.Near;
    outSplits[MaxShadowCascades] = range.Far;
    for (int split = 1; split < MaxShadowCascades; ++split)
    {
        const float fraction = static_cast<float>(split) / static_cast<float>(MaxShadowCascades);
        const float logarithmic = range.Near * std::pow(range.Far / std::max(range.Near, 0.001f), fraction);
        const float uniform = range.Near + (range.Far - range.Near) * fraction;
        outSplits[split] = lambda * logarithmic + (1.0f - lambda) * uniform;
    }
}

/**
 * Aligns one axis of a shadow projection to its own texel grid. The size moves in steps of a
 * sixteenth of an octave and the start in whole texels, so a small camera movement leaves every
 * texel where it was instead of sliding the shadow edges along the rows of the map. The result
 * always contains the original range.
 */
inline void SnapShadowBounds(float& minValue, float& maxValue, int mapSize)
{
    const float size = maxValue - minValue;
    if (!(size > 0.0f) || mapSize <= 2)
    {
        return;
    }
    const float step = std::exp2(std::floor(std::log2(size)) - 4.0f);
    const float padded = size * static_cast<float>(mapSize) / static_cast<float>(mapSize - 2);
    const float snappedSize = std::ceil(padded / step) * step;
    const float texel = snappedSize / static_cast<float>(mapSize);
    minValue = std::floor(minValue / texel) * texel;
    maxValue = minValue + snappedSize;
}
