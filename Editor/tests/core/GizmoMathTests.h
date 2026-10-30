#pragma once

#include "../../Core/GizmoMath.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{

void CheckGizmo(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (GizmoMath): %s\n", msg);
        std::exit(1);
    }
}

inline void RunGizmoMathTests()
{
    using namespace EditorCore;

    EngineSceneTransform identityTransform{};
    identityTransform.position[0] = 0.0f;
    identityTransform.position[1] = 0.0f;
    identityTransform.position[2] = 0.0f;
    identityTransform.rotation[0] = 0.0f;
    identityTransform.rotation[1] = 0.0f;
    identityTransform.rotation[2] = 0.0f;
    identityTransform.rotation[3] = 1.0f;
    identityTransform.scale[0] = 1.0f;
    identityTransform.scale[1] = 1.0f;
    identityTransform.scale[2] = 1.0f;

    // 1. Translate mode does not alter rotation or scale
    {
        EngineSceneTransform out = ApplyRotateScaleGizmo(
            GizmoMode::Translate, identityTransform, QPointF(50.0, 50.0),
            AxisLock::All, false, 0.0, 0.0);
        CheckGizmo(out.rotation[3] == 1.0f, "Translate mode preserves rotation");
        CheckGizmo(out.scale[0] == 1.0f, "Translate mode preserves scale");
    }

    // 2. Rotate with AxisLock::Y (Yaw only)
    {
        EngineSceneTransform out = ApplyRotateScaleGizmo(
            GizmoMode::Rotate, identityTransform, QPointF(100.0, 0.0),
            AxisLock::Y, false, 0.0, 0.0, 0.01f, 0.01f);
        // Yaw rotates around Y axis (q[1] is non-zero, q[0] and q[2] are zero)
        CheckGizmo(std::fabs(out.rotation[1]) > 0.01f, "AxisLock::Y produces Y rotation");
        CheckGizmo(std::fabs(out.rotation[0]) < 1e-5f, "AxisLock::Y leaves X rotation at 0");
        CheckGizmo(std::fabs(out.rotation[2]) < 1e-5f, "AxisLock::Y leaves Z rotation at 0");
    }

    // 3. Rotate with AxisLock::X (Pitch only)
    {
        EngineSceneTransform out = ApplyRotateScaleGizmo(
            GizmoMode::Rotate, identityTransform, QPointF(0.0, 100.0),
            AxisLock::X, false, 0.0, 0.0, 0.01f, 0.01f);
        CheckGizmo(std::fabs(out.rotation[0]) > 0.01f, "AxisLock::X produces X rotation");
        CheckGizmo(std::fabs(out.rotation[1]) < 1e-5f, "AxisLock::X leaves Y rotation at 0");
        CheckGizmo(std::fabs(out.rotation[2]) < 1e-5f, "AxisLock::X leaves Z rotation at 0");
    }

    // 4. Rotate with AxisLock::Z (Roll only)
    {
        EngineSceneTransform out = ApplyRotateScaleGizmo(
            GizmoMode::Rotate, identityTransform, QPointF(100.0, 0.0),
            AxisLock::Z, false, 0.0, 0.0, 0.01f, 0.01f);
        CheckGizmo(std::fabs(out.rotation[2]) > 0.01f, "AxisLock::Z produces Z rotation");
        CheckGizmo(std::fabs(out.rotation[0]) < 1e-5f, "AxisLock::Z leaves X rotation at 0");
        CheckGizmo(std::fabs(out.rotation[1]) < 1e-5f, "AxisLock::Z leaves Y rotation at 0");
    }

    // 5. Rotate snap: 15 degree step
    {
        // 100 pixels * 0.01 = 1.0 radian (~57.29 degrees)
        // Snapped to 15 deg (0.261799 rad): 57.29 / 15 = 3.819 -> 4 steps = 60 deg
        EngineSceneTransform snapped = ApplyRotateScaleGizmo(
            GizmoMode::Rotate, identityTransform, QPointF(100.0, 0.0),
            AxisLock::Y, true, 15.0, 0.0, 0.01f, 0.01f);
        const double deg2rad = 3.14159265358979323846 / 180.0;
        const double expectedYaw = 4.0 * 15.0 * deg2rad;
        const double expectedQy = std::sin(expectedYaw * 0.5);
        CheckGizmo(std::fabs(snapped.rotation[1] - expectedQy) < 1e-4, "Rotation snap matches expected 60 deg");
    }

    // 6. Scale with AxisLock::All
    {
        EngineSceneTransform out = ApplyRotateScaleGizmo(
            GizmoMode::Scale, identityTransform, QPointF(50.0, 0.0),
            AxisLock::All, false, 0.0, 0.0, 0.01f, 0.01f);
        CheckGizmo(out.scale[0] > 1.0f, "Scale increases with positive drag");
        CheckGizmo(std::fabs(out.scale[0] - out.scale[1]) < 1e-5f, "Scale X and Y equal for AxisLock::All");
        CheckGizmo(std::fabs(out.scale[0] - out.scale[2]) < 1e-5f, "Scale X and Z equal for AxisLock::All");
    }

    // 7. Scale with AxisLock::X
    {
        EngineSceneTransform out = ApplyRotateScaleGizmo(
            GizmoMode::Scale, identityTransform, QPointF(50.0, 0.0),
            AxisLock::X, false, 0.0, 0.0, 0.01f, 0.01f);
        CheckGizmo(out.scale[0] > 1.0f, "Scale X increased");
        CheckGizmo(out.scale[1] == 1.0f, "Scale Y unchanged with AxisLock::X");
        CheckGizmo(out.scale[2] == 1.0f, "Scale Z unchanged with AxisLock::X");
    }

    // 8. Scale with AxisLock::Y
    {
        EngineSceneTransform out = ApplyRotateScaleGizmo(
            GizmoMode::Scale, identityTransform, QPointF(50.0, 0.0),
            AxisLock::Y, false, 0.0, 0.0, 0.01f, 0.01f);
        CheckGizmo(out.scale[0] == 1.0f, "Scale X unchanged with AxisLock::Y");
        CheckGizmo(out.scale[1] > 1.0f, "Scale Y increased");
        CheckGizmo(out.scale[2] == 1.0f, "Scale Z unchanged with AxisLock::Y");
    }

    // 9. Scale with AxisLock::Z
    {
        EngineSceneTransform out = ApplyRotateScaleGizmo(
            GizmoMode::Scale, identityTransform, QPointF(50.0, 0.0),
            AxisLock::Z, false, 0.0, 0.0, 0.01f, 0.01f);
        CheckGizmo(out.scale[0] == 1.0f, "Scale X unchanged with AxisLock::Z");
        CheckGizmo(out.scale[1] == 1.0f, "Scale Y unchanged with AxisLock::Z");
        CheckGizmo(out.scale[2] > 1.0f, "Scale Z increased");
    }

    // 10. Scale snap: 0.25 step
    {
        EngineSceneTransform snapped = ApplyRotateScaleGizmo(
            GizmoMode::Scale, identityTransform, QPointF(30.0, 0.0),
            AxisLock::All, true, 0.0, 0.25, 0.01f, 0.01f);
        // exp(0.3) = 1.3498... -> snapped to 0.25 step: 1.25 or 1.50
        double val = snapped.scale[0];
        double rem = std::fmod(val, 0.25);
        CheckGizmo(rem < 1e-4 || std::fabs(rem - 0.25) < 1e-4, "Scale snapped to 0.25 step");
    }

    // 11. ComputeSpawnPosition
    {
        float camPos[3] = {0.0f, 10.0f, 0.0f};
        auto spawnOrigin = ComputeSpawnPosition(800, 600, QPoint(400, 300), 0.0f, 0.0f, camPos, true);
        CheckGizmo(spawnOrigin[0] == 0.0 && spawnOrigin[1] == 0.0 && spawnOrigin[2] == 0.0, "Spawn at origin returns {0,0,0}");

        // Looking straight down: pitch -1.5707 rad (-90 deg), camPos y=10
        auto spawnGround = ComputeSpawnPosition(800, 600, QPoint(400, 300), 0.0f, -1.57f, camPos, false);
        CheckGizmo(spawnGround[1] == 0.0, "Ground raycast lands at Y=0");
    }
}

} // namespace
