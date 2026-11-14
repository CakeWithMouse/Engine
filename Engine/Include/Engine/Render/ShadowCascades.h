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

struct ShadowAxisBounds
{
    float Min = 0.0f;
    float Max = 0.0f;
    float Extent = 0.0f;
    float TexelSize = 0.0f;
    float OriginTexel = 0.0f;
};

struct ShadowCascadeProjectionBounds
{
    ShadowAxisBounds X;
    ShadowAxisBounds Y;
    float SphereRadius = 0.0f;
};

/**
 * Circumscribed sphere radius for a perspective frustum slice centered halfway between its
 * near and far planes. Unlike an AABB measured from rotated world-space corners, this radius
 * depends only on the camera projection and slice depths, so yaw cannot change its XY extent.
 */
inline float CascadeFrustumSphereRadius(float nearDepth, float farDepth,
                                        float tanHalfFovY, float aspect)
{
    if (!std::isfinite(nearDepth) || !std::isfinite(farDepth) ||
        !std::isfinite(tanHalfFovY) || !std::isfinite(aspect) ||
        nearDepth < 0.0f || farDepth <= nearDepth || tanHalfFovY <= 0.0f || aspect <= 0.0f)
    {
        return 0.0f;
    }

    const float halfDepth = (farDepth - nearDepth) * 0.5f;
    const float farHalfHeight = farDepth * tanHalfFovY;
    const float farHalfWidth = farHalfHeight * aspect;
    return std::sqrt(halfDepth * halfDepth +
                     farHalfHeight * farHalfHeight +
                     farHalfWidth * farHalfWidth);
}

/**
 * Hysteresis for the sphere-fit radius when dolly or area-based splits change near/far depths.
 * Small variations retain the prior projection; growth is immediate and reserves 10%, while
 * shrink occurs only after the required radius drops below 75% of the prior one. This depends
 * only on the required and previous radii, not elapsed time or frame count.
 */
inline float StabilizeShadowCascadeRadius(float requiredRadius, float previousRadius)
{
    if (!std::isfinite(requiredRadius) || requiredRadius <= 0.0f)
    {
        return 0.0f;
    }

    if (std::isfinite(previousRadius) && previousRadius > 0.0f &&
        requiredRadius <= previousRadius && requiredRadius >= previousRadius * 0.75f)
    {
        return previousRadius;
    }

    const float step = std::exp2(std::floor(std::log2(requiredRadius)) - 4.0f);
    return std::ceil((requiredRadius * 1.1f) / step) * step;
}

/**
 * Places one axis of a sphere-fit projection on the fixed light-space texel grid. The extent
 * is calculated from the invariant sphere radius, not from center +/- radius (whose subtraction
 * can lose precision at large world coordinates). Two texels of padding leave room for snapping
 * the low edge down while keeping the complete sphere in the map.
 */
inline ShadowAxisBounds SnapSphereFitAxis(float center, float sphereRadius, int mapSize)
{
    ShadowAxisBounds result;
    if (!std::isfinite(center) || !std::isfinite(sphereRadius) ||
        !(sphereRadius > 0.0f) || mapSize <= 2)
    {
        return result;
    }

    const float diameter = sphereRadius * 2.0f;
    result.Extent = diameter * static_cast<float>(mapSize) / static_cast<float>(mapSize - 2);
    result.TexelSize = result.Extent / static_cast<float>(mapSize);
    result.OriginTexel = std::floor((center - sphereRadius) / result.TexelSize);
    result.Min = result.OriginTexel * result.TexelSize;
    result.Max = result.Min + result.Extent;
    return result;
}

inline ShadowCascadeProjectionBounds FitShadowCascadeProjectionBoundsForRadius(
    float centerLightX, float centerLightY, float sphereRadius, int mapSize)
{
    ShadowCascadeProjectionBounds result;
    result.SphereRadius = sphereRadius;
    result.X = SnapSphereFitAxis(centerLightX, sphereRadius, mapSize);
    result.Y = SnapSphereFitAxis(centerLightY, sphereRadius, mapSize);
    return result;
}

/**
 * Stable square XY projection for a perspective cascade slice. `centerLightX/Y` are the
 * frustum-slice center transformed into the world-anchored light view. Rotation changes only
 * the snapped grid cell, never the projection extent; caller still uses the actual corners for
 * depth bounds and caster culling.
 */
inline ShadowCascadeProjectionBounds FitShadowCascadeProjectionBounds(
    float centerLightX, float centerLightY, float nearDepth, float farDepth,
    float tanHalfFovY, float aspect, int mapSize)
{
    const float sphereRadius = CascadeFrustumSphereRadius(nearDepth, farDepth, tanHalfFovY, aspect);
    return FitShadowCascadeProjectionBoundsForRadius(centerLightX, centerLightY, sphereRadius, mapSize);
}
