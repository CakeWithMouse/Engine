#pragma once

#include <Engine/Runtime/SceneModuleApi.h>
#include <QPoint>
#include <QPointF>
#include <array>
#include <cstdint>

namespace EditorCore
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

void NormalizeQuat(double q[4]);
void MulQuat(const double a[4], const double b[4], double out[4]);
void AxisAngleQuat(double x, double y, double z, double angle, double out[4]);

EngineSceneTransform ApplyRotateScaleGizmo(
    GizmoMode mode,
    const EngineSceneTransform& startTransform,
    const QPointF& total,
    AxisLock activeAxis,
    bool isSnapActive,
    double rotateSnapStepDeg,
    double scaleSnapStep,
    float rotateSensitivity = 0.01f,
    float scaleSensitivity = 0.01f
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

} // namespace EditorCore
