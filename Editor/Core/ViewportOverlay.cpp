#include "ViewportOverlay.h"

namespace ViewportOverlay
{
    static uint32_t FadeColor(uint32_t color, float factor)
    {
        const uint32_t a = (color >> 24) & 0xFF;
        const uint32_t newA = static_cast<uint32_t>(static_cast<float>(a) * std::clamp(factor, 0.0f, 1.0f));
        return (color & 0x00FFFFFF) | (newA << 24);
    }

    std::vector<EngineOverlayLine> GenerateGridLines(
        float camX,
        float camZ,
        const Palette& palette,
        float step,
        int majorInterval,
        int halfCount,
        float maxDistance)
    {
        std::vector<EngineOverlayLine> lines;
        lines.reserve(static_cast<size_t>(halfCount * 4 + 4));

        const float centerX = std::floor(camX / step) * step;
        const float centerZ = std::floor(camZ / step) * step;
        const float extent = static_cast<float>(halfCount) * step;

        // Lines parallel to Z
        for (int i = -halfCount; i <= halfCount; ++i)
        {
            const float x = centerX + static_cast<float>(i) * step;
            const float dist = std::abs(x - camX);
            if (dist > maxDistance) continue;

            float fade = 1.0f - (dist / maxDistance);
            fade = fade * fade;

            const bool isOriginZ = std::abs(x) < 0.001f;
            const int rounded = static_cast<int>(std::round(x / step));
            const bool isMajor = (rounded % majorInterval) == 0;

            uint32_t color;
            if (isOriginZ)
                color = FadeColor(palette.axisZ, fade * 1.5f);
            else if (isMajor)
                color = FadeColor(palette.gridMajor, fade);
            else
                color = FadeColor(palette.gridNormal, fade);

            if (((color >> 24) & 0xFF) > 2)
            {
                lines.push_back({x, 0.0f, centerZ - extent, x, 0.0f, centerZ + extent, color, 0});
            }
        }

        // Lines parallel to X
        for (int i = -halfCount; i <= halfCount; ++i)
        {
            const float z = centerZ + static_cast<float>(i) * step;
            const float dist = std::abs(z - camZ);
            if (dist > maxDistance) continue;

            float fade = 1.0f - (dist / maxDistance);
            fade = fade * fade;

            const bool isOriginX = std::abs(z) < 0.001f;
            const int rounded = static_cast<int>(std::round(z / step));
            const bool isMajor = (rounded % majorInterval) == 0;

            uint32_t color;
            if (isOriginX)
                color = FadeColor(palette.axisX, fade * 1.5f);
            else if (isMajor)
                color = FadeColor(palette.gridMajor, fade);
            else
                color = FadeColor(palette.gridNormal, fade);

            if (((color >> 24) & 0xFF) > 2)
            {
                lines.push_back({centerX - extent, 0.0f, z, centerX + extent, 0.0f, z, color, 0});
            }
        }

        // Short Y axis at origin
        lines.push_back({0.0f, 0.0f, 0.0f, 0.0f, 5.0f, 0.0f, palette.axisY, 0});

        return lines;
    }

    std::vector<EngineOverlayLine> GenerateLightRadiusLines(
        float posX, float posY, float posZ,
        float radius,
        uint32_t colorRgba,
        int segments)
    {
        std::vector<EngineOverlayLine> lines;
        if (radius <= 0.001f || segments < 8) return lines;
        lines.reserve(static_cast<size_t>(segments * 3));

        constexpr float twoPi = 6.28318530718f;
        const float step = twoPi / static_cast<float>(segments);

        for (int i = 0; i < segments; ++i)
        {
            const float a0 = static_cast<float>(i) * step;
            const float a1 = static_cast<float>(i + 1) * step;
            const float c0 = std::cos(a0) * radius;
            const float s0 = std::sin(a0) * radius;
            const float c1 = std::cos(a1) * radius;
            const float s1 = std::sin(a1) * radius;

            // XZ circle
            lines.push_back({posX + c0, posY, posZ + s0, posX + c1, posY, posZ + s1, colorRgba, 0});
            // XY circle
            lines.push_back({posX + c0, posY + s0, posZ, posX + c1, posY + s1, posZ, colorRgba, 0});
            // YZ circle
            lines.push_back({posX, posY + c0, posZ + s0, posX, posY + c1, posZ + s1, colorRgba, 0});
        }

        return lines;
    }

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
        float lightRadius)
    {
        uint64_t h = 14695981039346656037ull;
        auto hashBytes = [&](const void* ptr, size_t size) {
            const auto* b = static_cast<const uint8_t*>(ptr);
            for (size_t i = 0; i < size; ++i)
            {
                h ^= b[i];
                h *= 1099511628211ull;
            }
        };

        if (camPos) hashBytes(camPos, sizeof(float) * 3);
        if (camView) hashBytes(camView, sizeof(float) * 16);
        hashBytes(&gridEnabled, sizeof(bool));
        hashBytes(&gizmoEnabled, sizeof(bool));
        hashBytes(&selectedId, sizeof(uint64_t));
        hashBytes(&gizmoMode, sizeof(uint32_t));
        if (objPos) hashBytes(objPos, sizeof(float) * 3);
        if (objRot) hashBytes(objRot, sizeof(float) * 4);
        hashBytes(&hoverHandle, sizeof(int));
        hashBytes(&activeHandle, sizeof(int));
        hashBytes(&lightRadius, sizeof(float));
        return h;
    }
}

