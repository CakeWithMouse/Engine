#pragma once

#include <Engine/Runtime/ViewportOverlayApi.h>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace ViewportOverlay
{
    struct Palette
    {
        uint32_t gridNormal = 0x20808080;
        uint32_t gridMajor  = 0x40A0A0A0;
        uint32_t axisX      = 0xFF3030E8; // Red
        uint32_t axisY      = 0xFF30E830; // Green
        uint32_t axisZ      = 0xFFE88030; // Blue
    };

    inline uint32_t MakeRgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
    {
        return static_cast<uint32_t>(r) |
              (static_cast<uint32_t>(g) << 8) |
              (static_cast<uint32_t>(b) << 16) |
              (static_cast<uint32_t>(a) << 24);
    }

    std::vector<EngineOverlayLine> GenerateGridLines(
        float camX,
        float camZ,
        const Palette& palette,
        float step = 1.0f,
        int majorInterval = 10,
        int halfCount = 50,
        float maxDistance = 50.0f
    );

    std::vector<EngineOverlayLine> GenerateLightRadiusLines(
        float posX, float posY, float posZ,
        float radius,
        uint32_t colorRgba,
        int segments = 48
    );

    uint64_t ComputeOverlayHash(
        const float camPos[3],
        const float camView[16],
        bool gridEnabled,
        bool gizmoEnabled,
        uint64_t selectedId,
        uint32_t gizmoMode,
        const float objPos[3],
        const float objRot[4],
        int hoverHandle,
        int activeHandle,
        float lightRadius
    );
}

