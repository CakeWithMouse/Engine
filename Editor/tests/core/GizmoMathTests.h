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
    // 12. ComputeCameraBasis orthonormality
    {
        const auto b1 = ComputeCameraBasis(0.0f, 0.0f);
        CheckGizmo(std::fabs(b1.forward[0] - 1.0) < 1e-4, "Yaw 0 forward X is 1");
        CheckGizmo(std::fabs(b1.right[2] - 1.0) < 1e-4, "Yaw 0 right Z is 1");
        CheckGizmo(std::fabs(b1.up[1] - 1.0) < 1e-4, "Yaw 0 up Y is 1");

        const auto b2 = ComputeCameraBasis(-1.5707963f, 0.0f); // -pi/2: looking along -Z
        CheckGizmo(std::fabs(b2.forward[2] - (-1.0)) < 1e-4, "Yaw -pi/2 forward Z is -1");
        CheckGizmo(std::fabs(b2.right[0] - 1.0) < 1e-4, "Yaw -pi/2 right X is 1");
        CheckGizmo(std::fabs(b2.up[1] - 1.0) < 1e-4, "Yaw -pi/2 up Y is 1");
    }

    // 13. ComputeTranslateDelta follows cursor in screen plane across various camera yaw/pitch
    {
        const float camPos[3] = {0.0f, 0.0f, -5.0f};
        const float objPos[3] = {0.0f, 0.0f, 0.0f};
        const float objRot[4] = {0.0f, 0.0f, 0.0f, 1.0f};

        // Case A: Yaw = -pi/2 (looking -Z, Right is +X, Up is +Y)
        // Drag mouse right (dx = 50, dy = 0) -> delta should move in +X
        auto d1 = ComputeTranslateDelta(QPointF(50.0, 0.0), 600, -1.5707963f, 0.0f, camPos, objPos, AxisLock::All, false, objRot, false, 0.0);
        CheckGizmo(d1[0] > 0.0, "Yaw -pi/2 drag right moves along +X");
        CheckGizmo(std::fabs(d1[1]) < 1e-5, "Yaw -pi/2 drag right has 0 Y");
        CheckGizmo(std::fabs(d1[2]) < 1e-5, "Yaw -pi/2 drag right has 0 Z");

        // Drag mouse up (dx = 0, dy = -50) -> delta should move in +Y
        auto d1_up = ComputeTranslateDelta(QPointF(0.0, -50.0), 600, -1.5707963f, 0.0f, camPos, objPos, AxisLock::All, false, objRot, false, 0.0);
        CheckGizmo(d1_up[1] > 0.0, "Yaw -pi/2 drag up moves along +Y");

        // Case B: Yaw = 0 (looking +X, Right is +Z, Up is +Y)
        // Drag mouse right (dx = 50, dy = 0) -> delta should move in +Z
        auto d2 = ComputeTranslateDelta(QPointF(50.0, 0.0), 600, 0.0f, 0.0f, camPos, objPos, AxisLock::All, false, objRot, false, 0.0);
        CheckGizmo(d2[2] > 0.0, "Yaw 0 drag right moves along +Z (camera right)");
        CheckGizmo(std::fabs(d2[0]) < 1e-5, "Yaw 0 drag right has 0 X");

        // Case C: Yaw = pi (looking -X, Right is -Z, Up is +Y)
        // Drag mouse right -> delta should move along camera right (-Z in world)
        auto d3 = ComputeTranslateDelta(QPointF(50.0, 0.0), 600, 3.14159265f, 0.0f, camPos, objPos, AxisLock::All, false, objRot, false, 0.0);
        CheckGizmo(d3[2] < 0.0, "Yaw pi drag right moves along -Z (camera right)");

        // Distance proportionality: 2x distance = 2x delta
        const float farCamPos[3] = {0.0f, 0.0f, -10.0f};
        auto dFar = ComputeTranslateDelta(QPointF(50.0, 0.0), 600, -1.5707963f, 0.0f, farCamPos, objPos, AxisLock::All, false, objRot, false, 0.0);
        CheckGizmo(std::fabs(dFar[0] - 2.0 * d1[0]) < 1e-3, "Distance scaling is proportional to distance");
    }

    // 14. Axis constraints in World and Local space
    {
        const float camPos[3] = {0.0f, 0.0f, -5.0f};
        const float objPos[3] = {0.0f, 0.0f, 0.0f};
        // Object rotated 90 deg around Y: Local X points along Z
        double quat90[4];
        AxisAngleQuat(0.0, 1.0, 0.0, 3.14159265358979323846 * 0.5, quat90);
        float objRot90[4] = {static_cast<float>(quat90[0]), static_cast<float>(quat90[1]), static_cast<float>(quat90[2]), static_cast<float>(quat90[3])};

        // World X constraint
        auto dWorldX = ComputeTranslateDelta(QPointF(50.0, 50.0), 600, -1.5707963f, 0.0f, camPos, objPos, AxisLock::X, false, objRot90, false, 0.0);
        CheckGizmo(dWorldX[0] != 0.0, "World X constraint moves X");
        CheckGizmo(std::fabs(dWorldX[1]) < 1e-5, "World X constraint leaves Y at 0");
        CheckGizmo(std::fabs(dWorldX[2]) < 1e-5, "World X constraint leaves Z at 0");

        // Local X constraint (which points along Z in world)
        // Camera at yaw = 0 looks along +X, so camera right is +Z (local X axis is on screen right)
        auto dLocalX = ComputeTranslateDelta(QPointF(50.0, 0.0), 600, 0.0f, 0.0f, camPos, objPos, AxisLock::X, true, objRot90, false, 0.0);
        CheckGizmo(std::fabs(dLocalX[0]) < 1e-5, "Local X constraint leaves world X at 0");
        CheckGizmo(std::fabs(dLocalX[1]) < 1e-5, "Local X constraint leaves world Y at 0");
        CheckGizmo(std::fabs(dLocalX[2]) > 0.0, "Local X constraint (rotated 90 deg Y) moves world Z");
    }

    // 15. Rotation sign and local/world rotation
    {
        EngineSceneTransform t = identityTransform;
        // Rotate Yaw (around Y)
        EngineSceneTransform outYaw = ApplyRotateScaleGizmo(GizmoMode::Rotate, t, QPointF(50.0, 0.0), AxisLock::Y, false, 0.0, 0.0, 0.01f, 0.01f);
        CheckGizmo(outYaw.rotation[1] > 0.0f, "Positive mouse X rotates positive Yaw");

        // Rotate Pitch (around X)
        EngineSceneTransform outPitch = ApplyRotateScaleGizmo(GizmoMode::Rotate, t, QPointF(0.0, 50.0), AxisLock::X, false, 0.0, 0.0, 0.01f, 0.01f);
        CheckGizmo(outPitch.rotation[0] > 0.0f, "Positive mouse Y produces X rotation");
    }

}

} // namespace
