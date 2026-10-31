#pragma once

#include <Engine/Runtime/ViewportOverlayApi.h>
#include "GizmoMath.h"
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace GizmoGeometry
{
    enum class Handle : int
    {
        None = 0,
        AxisX,
        AxisY,
        AxisZ,
        PlaneXY,
        PlaneXZ,
        PlaneYZ,
        Center
    };

    struct Palette
    {
        uint32_t axisX      = 0xFF3030E8; // Red
        uint32_t axisY      = 0xFF30E830; // Green
        uint32_t axisZ      = 0xFFE88030; // Blue
        uint32_t highlight  = 0xFF20FFFF; // Yellow / Gold
        uint32_t planeXY    = 0x8030E8E8;
        uint32_t planeXZ    = 0x80E830E8;
        uint32_t planeYZ    = 0x80E8E830;
        uint32_t center     = 0xFFE0E0E0;
    };

    float ComputeGizmoWorldScale(
        const float objPos[3],
        const EngineCameraInfo& cam,
        float screenPixelSize = 90.0f
    );

    bool ProjectPointToScreen(
        const float world[3],
        const float view[16],
        const float proj[16],
        int screenWidth,
        int screenHeight,
        float screenOut[2]
    );

    float DistancePointToSegment(
        float px, float py,
        float x0, float y0,
        float x1, float y1
    );

    bool RayPlaneIntersection(
        const float rayOrigin[3],
        const float rayDir[3],
        const float planePoint[3],
        const float planeNormal[3],
        float hitPoint[3]
    );

    float ComputeRingAngle(
        const float center[3],
        const float axisNormal[3],
        const float refVector[3],
        const float worldPoint[3]
    );

    Handle PickHandle(
        GizmoMath::GizmoMode mode,
        const float objPos[3],
        const float objRot[4],
        bool isLocal,
        float mouseX,
        float mouseY,
        const EngineCameraInfo& cam,
        float thresholdPx = 8.0f
    );

    std::vector<EngineOverlayLine> BuildGizmoLines(
        GizmoMath::GizmoMode mode,
        const float objPos[3],
        const float objRot[4],
        bool isLocal,
        const EngineCameraInfo& cam,
        Handle hoverHandle,
        Handle activeHandle,
        const Palette& palette = Palette()
    );
}
