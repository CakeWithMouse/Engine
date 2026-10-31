#pragma once

#include <Engine/Runtime/ViewportOverlayApi.h>
#include "GizmoGeometry.h"
#include <cstdint>

class IEditorHost;

class ViewportOverlayHelper
{
public:
    static void UpdateOverlays(
        IEditorHost* host,
        const EngineCameraInfo& cam,
        bool hasCamera,
        GizmoGeometry::Handle hoveredHandle,
        int gizmoMode,
        bool localSpace,
        uint64_t& inOutHash
    );

    static uint64_t HitTestBillboard(
        IEditorHost* host,
        const EngineCameraInfo& cam,
        float screenX,
        float screenY,
        float tolerancePixels = 16.0f
    );
};

