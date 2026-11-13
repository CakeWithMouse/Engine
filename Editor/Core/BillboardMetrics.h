#pragma once

#include <algorithm>

namespace BillboardMetrics
{
    // Icon diameter as a fraction of the physical viewport height.
    inline constexpr float kHeightFraction = 0.024f;
    // Diameter clamp in physical pixels, keeping icons readable on small/large viewports.
    inline constexpr float kMinPixels = 16.0f;
    inline constexpr float kMaxPixels = 80.0f;

    // viewportHeightPhysical is the framebuffer height in physical pixels,
    // i.e. it already incorporates devicePixelRatio.
    inline float SizePixels(float viewportHeightPhysical)
    {
        return std::clamp(viewportHeightPhysical * kHeightFraction, kMinPixels, kMaxPixels);
    }
}