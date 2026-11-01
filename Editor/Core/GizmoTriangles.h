#pragma once

#include <vector>
#include <cmath>
#include <Engine/Runtime/ViewportOverlayApi.h>
#include "GizmoMath.h"
#include "GizmoGeometry.h"

namespace GizmoTriangles
{
    std::vector<EngineOverlayTriangle> BuildGizmoTriangles(
        GizmoMath::GizmoMode mode,
        const float objPos[3],
        const float camPos[3],
        const float camFwd[3],
        float fovRad,
        float screenHeight,
        GizmoGeometry::Handle hoverHandle = GizmoGeometry::Handle::None,
        const GizmoGeometry::Palette& palette = GizmoGeometry::Palette{}
    );
}

