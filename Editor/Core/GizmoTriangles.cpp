#include "GizmoTriangles.h"
#include <algorithm>

namespace GizmoTriangles
{
    using Handle = GizmoGeometry::Handle;

    static void AddQuad(std::vector<EngineOverlayTriangle>& tris,
                        const float p0[3], const float p1[3], const float p2[3], const float p3[3],
                        uint32_t rgba, uint32_t flags)
    {
        tris.push_back({p0[0], p0[1], p0[2], p1[0], p1[1], p1[2], p2[0], p2[1], p2[2], rgba, flags});
        tris.push_back({p0[0], p0[1], p0[2], p2[0], p2[1], p2[2], p3[0], p3[1], p3[2], rgba, flags});
    }

    static void AddCube(std::vector<EngineOverlayTriangle>& tris,
                        const float c[3], float sz, uint32_t rgba, uint32_t flags)
    {
        const float x0 = c[0] - sz, x1 = c[0] + sz;
        const float y0 = c[1] - sz, y1 = c[1] + sz;
        const float z0 = c[2] - sz, z1 = c[2] + sz;

        const float v[8][3] = {
            {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
            {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}
        };

        // Front, Back, Left, Right, Top, Bottom
        AddQuad(tris, v[0], v[1], v[2], v[3], rgba, flags);
        AddQuad(tris, v[5], v[4], v[7], v[6], rgba, flags);
        AddQuad(tris, v[4], v[0], v[3], v[7], rgba, flags);
        AddQuad(tris, v[1], v[5], v[6], v[2], rgba, flags);
        AddQuad(tris, v[3], v[2], v[6], v[7], rgba, flags);
        AddQuad(tris, v[4], v[5], v[1], v[0], rgba, flags);
    }

    static void AddPyramid(std::vector<EngineOverlayTriangle>& tris,
                           const float apex[3], const float baseCenter[3],
                           const float u1[3], const float u2[3],
                           float radius, uint32_t rgba, uint32_t flags)
    {
        const float b0[3] = {baseCenter[0] + u1[0] * radius, baseCenter[1] + u1[1] * radius, baseCenter[2] + u1[2] * radius};
        const float b1[3] = {baseCenter[0] + u2[0] * radius, baseCenter[1] + u2[1] * radius, baseCenter[2] + u2[2] * radius};
        const float b2[3] = {baseCenter[0] - u1[0] * radius, baseCenter[1] - u1[1] * radius, baseCenter[2] - u1[2] * radius};
        const float b3[3] = {baseCenter[0] - u2[0] * radius, baseCenter[1] - u2[1] * radius, baseCenter[2] - u2[2] * radius};

        // 4 side faces
        tris.push_back({apex[0], apex[1], apex[2], b0[0], b0[1], b0[2], b1[0], b1[1], b1[2], rgba, flags});
        tris.push_back({apex[0], apex[1], apex[2], b1[0], b1[1], b1[2], b2[0], b2[1], b2[2], rgba, flags});
        tris.push_back({apex[0], apex[1], apex[2], b2[0], b2[1], b2[2], b3[0], b3[1], b3[2], rgba, flags});
        tris.push_back({apex[0], apex[1], apex[2], b3[0], b3[1], b3[2], b0[0], b0[1], b0[2], rgba, flags});

        // Base quad
        AddQuad(tris, b0, b3, b2, b1, rgba, flags);
    }

    std::vector<EngineOverlayTriangle> BuildGizmoTriangles(
        GizmoMath::GizmoMode mode,
        const float objPos[3],
        const float camPos[3],
        const float camFwd[3],
        float fovRad,
        float screenHeight,
        Handle hoverHandle,
        const GizmoGeometry::Palette& palette)
    {
        std::vector<EngineOverlayTriangle> tris;
        const float dx = objPos[0] - camPos[0];
        const float dy = objPos[1] - camPos[1];
        const float dz = objPos[2] - camPos[2];
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float fovY = (fovRad > 0.01f) ? fovRad : 1.04719755f;
        const float viewH = (screenHeight > 10.0f) ? screenHeight : 600.0f;
        const float scale = std::max(0.001f, dist * std::tan(fovY * 0.5f) * (100.0f / (viewH * 0.5f)));

        const float ax[3] = {1.0f, 0.0f, 0.0f};
        const float ay[3] = {0.0f, 1.0f, 0.0f};
        const float az[3] = {0.0f, 0.0f, 1.0f};

        auto getStyle = [&](Handle h, uint32_t normalColor) -> uint32_t {
            if (hoverHandle == Handle::None) return normalColor;
            if (hoverHandle == h) return palette.highlight;
            const uint32_t a = ((normalColor >> 24) & 0xFF) / 2;
            return (normalColor & 0x00FFFFFF) | (a << 24);
        };

        const uint32_t flags = OverlayFlags::AlwaysOnTop;

        if (mode == GizmoMath::GizmoMode::Translate)
        {
            // Pyramid arrowheads for each axis
            auto addArrow = [&](const float a[3], const float u1[3], const float u2[3], Handle hnd, uint32_t col) {
                const uint32_t c = getStyle(hnd, col);
                const float tip[3] = {objPos[0] + a[0] * scale, objPos[1] + a[1] * scale, objPos[2] + a[2] * scale};
                const float coneLen = scale * 0.22f;
                const float coneR = scale * 0.07f;
                const float base[3] = {tip[0] - a[0] * coneLen, tip[1] - a[1] * coneLen, tip[2] - a[2] * coneLen};
                AddPyramid(tris, tip, base, u1, u2, coneR, c, flags);
            };

            addArrow(ax, ay, az, Handle::AxisX, palette.axisX);
            addArrow(ay, ax, az, Handle::AxisY, palette.axisY);
            addArrow(az, ax, ay, Handle::AxisZ, palette.axisZ);

            // Semi-transparent plane handles
            auto addPlane = [&](const float a1[3], const float a2[3], Handle hnd, uint32_t col) {
                uint32_t c = getStyle(hnd, col);
                const uint32_t planeAlpha = (hoverHandle == hnd) ? 0x88000000 : 0x44000000;
                c = (c & 0x00FFFFFF) | planeAlpha;

                const float pSize = scale * 0.28f;
                const float p0[3] = {objPos[0], objPos[1], objPos[2]};
                const float p1[3] = {objPos[0] + a1[0] * pSize, objPos[1] + a1[1] * pSize, objPos[2] + a1[2] * pSize};
                const float p2[3] = {objPos[0] + a2[0] * pSize, objPos[1] + a2[1] * pSize, objPos[2] + a2[2] * pSize};
                const float pc[3] = {objPos[0] + (a1[0] + a2[0]) * pSize, objPos[1] + (a1[1] + a2[1]) * pSize, objPos[2] + (a1[2] + a2[2]) * pSize};
                AddQuad(tris, p0, p1, pc, p2, c, flags);
            };

            addPlane(ax, ay, Handle::PlaneXY, palette.planeXY);
            addPlane(ax, az, Handle::PlaneXZ, palette.planeXZ);
            addPlane(ay, az, Handle::PlaneYZ, palette.planeYZ);

            // Solid center cube
            const uint32_t cc = getStyle(Handle::Center, palette.center);
            AddCube(tris, objPos, scale * 0.07f, cc, flags);
        }
        else if (mode == GizmoMath::GizmoMode::Scale)
        {
            // Solid scale cubes
            auto addScaleHandle = [&](const float a[3], Handle hnd, uint32_t col) {
                const uint32_t c = getStyle(hnd, col);
                const float tip[3] = {objPos[0] + a[0] * scale, objPos[1] + a[1] * scale, objPos[2] + a[2] * scale};
                AddCube(tris, tip, scale * 0.07f, c, flags);
            };

            addScaleHandle(ax, Handle::AxisX, palette.axisX);
            addScaleHandle(ay, Handle::AxisY, palette.axisY);
            addScaleHandle(az, Handle::AxisZ, palette.axisZ);

            // Solid center cube
            const uint32_t cc = getStyle(Handle::Center, palette.center);
            AddCube(tris, objPos, scale * 0.08f, cc, flags);
        }

        return tris;
    }
}
