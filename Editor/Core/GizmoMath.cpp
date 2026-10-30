#include "GizmoMath.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace EditorCore
{

void NormalizeQuat(double q[4])
{
    const double len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);

    if (len < 1e-6)
    {
        q[0] = 0.0;
        q[1] = 0.0;
        q[2] = 0.0;
        q[3] = 1.0;
        return;
    }

    const double inv = 1.0 / len;
    q[0] *= inv;
    q[1] *= inv;
    q[2] *= inv;
    q[3] *= inv;
}

void MulQuat(const double a[4], const double b[4], double out[4])
{
    const double x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    const double y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    const double z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    const double w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];

    out[0] = x;
    out[1] = y;
    out[2] = z;
    out[3] = w;
}

void AxisAngleQuat(double x, double y, double z, double angle, double out[4])
{
    const double len = std::sqrt(x * x + y * y + z * z);

    if (len < 1e-6)
    {
        out[0] = 0.0;
        out[1] = 0.0;
        out[2] = 0.0;
        out[3] = 1.0;
        return;
    }

    const double inv = 1.0 / len;
    const double half = angle * 0.5;
    const double s = std::sin(half);

    out[0] = x * inv * s;
    out[1] = y * inv * s;
    out[2] = z * inv * s;
    out[3] = std::cos(half);
}

EngineSceneTransform ApplyRotateScaleGizmo(
    GizmoMode mode,
    const EngineSceneTransform& startTransform,
    const QPointF& total,
    AxisLock activeAxis,
    bool isSnapActive,
    double rotateSnapStepDeg,
    double scaleSnapStep,
    float rotateSensitivity,
    float scaleSensitivity)
{
    if (mode == GizmoMode::Translate)
        return startTransform;

    EngineSceneTransform t = startTransform;

    if (mode == GizmoMode::Rotate)
    {
        double yaw = 0.0;
        double pitch = 0.0;
        double roll = 0.0;

        if (activeAxis == AxisLock::X)
        {
            pitch = -total.y() * static_cast<double>(rotateSensitivity);
        }
        else if (activeAxis == AxisLock::Y)
        {
            yaw = total.x() * static_cast<double>(rotateSensitivity);
        }
        else if (activeAxis == AxisLock::Z)
        {
            roll = total.x() * static_cast<double>(rotateSensitivity);
        }
        else
        {
            yaw = total.x() * static_cast<double>(rotateSensitivity);
            pitch = -total.y() * static_cast<double>(rotateSensitivity);
        }

        if (isSnapActive && rotateSnapStepDeg > 1e-4)
        {
            const double deg2rad = 3.14159265358979323846 / 180.0;
            const double stepRad = rotateSnapStepDeg * deg2rad;
            yaw = std::round(yaw / stepRad) * stepRad;
            pitch = std::round(pitch / stepRad) * stepRad;
            roll = std::round(roll / stepRad) * stepRad;
        }

        double startRot[4] = {
            static_cast<double>(startTransform.rotation[0]),
            static_cast<double>(startTransform.rotation[1]),
            static_cast<double>(startTransform.rotation[2]),
            static_cast<double>(startTransform.rotation[3])
        };

        NormalizeQuat(startRot);

        double qYaw[4];
        double qPitch[4];
        double qRoll[4];
        double qTmp1[4];
        double qTmp2[4];
        double qNew[4];

        AxisAngleQuat(0.0, 1.0, 0.0, yaw, qYaw);
        AxisAngleQuat(1.0, 0.0, 0.0, pitch, qPitch);
        AxisAngleQuat(0.0, 0.0, 1.0, roll, qRoll);

        MulQuat(qYaw, qPitch, qTmp1);
        MulQuat(qTmp1, qRoll, qTmp2);
        MulQuat(qTmp2, startRot, qNew);
        NormalizeQuat(qNew);

        using RotT = std::remove_reference_t<decltype(t.rotation[0])>;

        for (int i = 0; i < 4; ++i)
            t.rotation[i] = static_cast<RotT>(qNew[i]);
    }
    else if (mode == GizmoMode::Scale)
    {
        double factor = std::exp((total.x() - total.y()) * static_cast<double>(scaleSensitivity));
        factor = std::max(0.01, std::min(100.0, factor));

        if (isSnapActive && scaleSnapStep > 1e-4)
        {
            factor = std::max(scaleSnapStep, std::round(factor / scaleSnapStep) * scaleSnapStep);
        }

        using ScaleT = std::remove_reference_t<decltype(t.scale[0])>;

        for (int i = 0; i < 3; ++i)
        {
            bool modifyAxis = true;
            if (activeAxis == AxisLock::X && i != 0) modifyAxis = false;
            if (activeAxis == AxisLock::Y && i != 1) modifyAxis = false;
            if (activeAxis == AxisLock::Z && i != 2) modifyAxis = false;

            if (modifyAxis)
            {
                const double base = startTransform.scale[i];
                double newScale = base * factor;
                if (isSnapActive && scaleSnapStep > 1e-4)
                {
                    newScale = std::max(scaleSnapStep, std::round(newScale / scaleSnapStep) * scaleSnapStep);
                }
                t.scale[i] = static_cast<ScaleT>(newScale);
            }
            else
            {
                t.scale[i] = startTransform.scale[i];
            }
        }
    }

    return t;
}

std::array<double, 3> ComputeSpawnPosition(
    int viewportWidth,
    int viewportHeight,
    const QPoint& viewportPos,
    float camYaw,
    float camPitch,
    const float camPos[3],
    bool spawnAtOrigin)
{
    if (spawnAtOrigin)
    {
        return {0.0, 0.0, 0.0};
    }

    const float w = viewportWidth > 0 ? static_cast<float>(viewportWidth) : 800.0f;
    const float h = viewportHeight > 0 ? static_cast<float>(viewportHeight) : 600.0f;

    const float ndcX = 2.0f * static_cast<float>(viewportPos.x()) / w - 1.0f;
    const float ndcY = 1.0f - 2.0f * static_cast<float>(viewportPos.y()) / h;

    const float aspect = w / h;
    const float tanHalfFov = 0.577f;

    const float cy = std::cos(camYaw);
    const float sy = std::sin(camYaw);
    const float cp = std::cos(camPitch);
    const float sp = std::sin(camPitch);

    const float fwdX = cy * cp;
    const float fwdY = sp;
    const float fwdZ = sy * cp;

    const float rightX = sy;
    const float rightY = 0.0f;
    const float rightZ = -cy;

    const float upX = -fwdY * rightZ;
    const float upY = fwdZ * rightX - fwdX * rightZ;
    const float upZ = fwdY * rightX;

    float rx = fwdX + ndcX * aspect * tanHalfFov * rightX + ndcY * tanHalfFov * upX;
    float ry = fwdY + ndcX * aspect * tanHalfFov * rightY + ndcY * tanHalfFov * upY;
    float rz = fwdZ + ndcX * aspect * tanHalfFov * rightZ + ndcY * tanHalfFov * upZ;

    const float rLen = std::sqrt(rx * rx + ry * ry + rz * rz);
    if (rLen > 1e-6f)
    {
        rx /= rLen;
        ry /= rLen;
        rz /= rLen;
    }

    if (ry < -0.01f && camPos[1] > 0.0f)
    {
        const float t = -camPos[1] / ry;
        if (t > 0.1f && t < 200.0f)
        {
            return {
                static_cast<double>(camPos[0] + rx * t),
                0.0,
                static_cast<double>(camPos[2] + rz * t)
            };
        }
    }

    return {
        static_cast<double>(camPos[0] + fwdX * 10.0f),
        0.0,
        static_cast<double>(camPos[2] + fwdZ * 10.0f)
    };
}

} // namespace EditorCore
