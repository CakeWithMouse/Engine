#pragma once

#include <Engine/Runtime/SceneModuleApi.h>
#include <QPoint>
#include <QPointF>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace GizmoMath
{
    enum class AxisLock : uint32_t
    {
        All = 0,
        X = 1,
        Y = 2,
        Z = 3
    };

    enum class GizmoMode : uint32_t
    {
        Translate = 0,
        Rotate = 1,
        Scale = 2
    };

    struct CameraBasis
    {
        double forward[3];
        double right[3];
        double up[3];
    };

    CameraBasis ComputeCameraBasis(float yaw, float pitch);

    std::array<double, 3> ComputeTranslateDelta(
        const QPointF& mouseDelta,
        int viewportHeight,
        float camYaw,
        float camPitch,
        const float camPos[3],
        const float objStartPos[3],
        AxisLock activeAxis,
        bool isLocal,
        const float objStartRot[4],
        bool snapEnabled,
        double snapStep,
        float fovYRad = 1.04719755f
    );

    void NormalizeQuat(double q[4]);
    void AxisAngleQuat(double ax, double ay, double az, double angleRad, double out[4]);
    void MulQuat(const double q1[4], const double q2[4], double out[4]);
    void RotateVectorQuat(const double q[4], const double v[3], double out[3]);

    double SnapTranslation(double val, double step);
    void SnapVector(const double in[3], double step, double out[3]);
    double SnapRotation(double angleRad, double stepDeg);
    double SnapScale(double scale, double step);

    void TransformDelta(const double delta[3], const double rotQuat[4], bool isLocal, double out[3]);

    void ApplyRotation(
        const double startRot[4],
        double deltaX,
        double deltaY,
        float sensitivity,
        bool snapEnabled,
        double snapStepDeg,
        double outRot[4]
    );

    void ApplyScale(
        const double startScale[3],
        double deltaX,
        double deltaY,
        float sensitivity,
        bool snapEnabled,
        double snapStep,
        double outScale[3]
    );

    EngineSceneTransform ApplyRotateScaleGizmo(
        GizmoMode mode,
        const EngineSceneTransform& startTransform,
        const QPointF& total,
        AxisLock activeAxis,
        bool isSnapActive,
        double rotateSnapStepDeg,
        double scaleSnapStep,
        float rotateSensitivity = 0.01f,
        float scaleSensitivity = 0.01f,
        bool isLocal = false
    );

    std::array<double, 3> ComputeSpawnPosition(
        int viewportWidth,
        int viewportHeight,
        const QPoint& viewportPos,
        float camYaw,
        float camPitch,
        const float camPos[3],
        bool spawnAtOrigin
    );
} // namespace GizmoMath

namespace EditorCore
{
    using namespace GizmoMath;
} // namespace EditorCore
