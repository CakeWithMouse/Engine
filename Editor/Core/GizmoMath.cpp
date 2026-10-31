#include "GizmoMath.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace GizmoMath
{
    CameraBasis ComputeCameraBasis(float yaw, float pitch)
    {
        const double cy = std::cos(static_cast<double>(yaw));
        const double sy = std::sin(static_cast<double>(yaw));
        const double cp = std::cos(static_cast<double>(pitch));
        const double sp = std::sin(static_cast<double>(pitch));

        CameraBasis b{};
        // Forward vector in engine DirectX LH coordinates (Player.cpp:295-297)
        b.forward[0] = cy * cp;
        b.forward[1] = sp;
        b.forward[2] = sy * cp;

        // Camera right vector: points towards screen-right
        b.right[0] = -sy;
        b.right[1] = 0.0;
        b.right[2] = cy;

        // Camera up vector: cross(right, forward)
        b.up[0] = -cy * sp;
        b.up[1] = cp;
        b.up[2] = -sy * sp;

        return b;
    }

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
        float fovYRad)
    {
        const double screenDx = mouseDelta.x();
        const double screenDy = -mouseDelta.y(); // Qt mouse Y is downwards; screen Y is upwards

        const CameraBasis basis = ComputeCameraBasis(camYaw, camPitch);

        const double toObj[3] = {
            static_cast<double>(objStartPos[0] - camPos[0]),
            static_cast<double>(objStartPos[1] - camPos[1]),
            static_cast<double>(objStartPos[2] - camPos[2])
        };

        double dist = toObj[0] * basis.forward[0] + toObj[1] * basis.forward[1] + toObj[2] * basis.forward[2];
        if (dist < 0.1)
        {
            const double lenSq = toObj[0] * toObj[0] + toObj[1] * toObj[1] + toObj[2] * toObj[2];
            dist = std::sqrt(lenSq);
        }
        if (dist < 0.5)
            dist = 5.0;

        const double tanHalfFov = std::tan(static_cast<double>(fovYRad) * 0.5);
        const double vh = viewportHeight > 0 ? static_cast<double>(viewportHeight) : 600.0;
        const double worldPerPixel = (2.0 * dist * tanHalfFov) / vh;

        std::array<double, 3> delta{0.0, 0.0, 0.0};

        if (activeAxis == AxisLock::All)
        {
            delta[0] = (screenDx * basis.right[0] + screenDy * basis.up[0]) * worldPerPixel;
            delta[1] = (screenDx * basis.right[1] + screenDy * basis.up[1]) * worldPerPixel;
            delta[2] = (screenDx * basis.right[2] + screenDy * basis.up[2]) * worldPerPixel;

            if (snapEnabled && snapStep > 1e-6)
            {
                for (int i = 0; i < 3; ++i)
                {
                    const double targetPos = static_cast<double>(objStartPos[i]) + delta[i];
                    const double snappedPos = SnapTranslation(targetPos, snapStep);
                    delta[i] = snappedPos - static_cast<double>(objStartPos[i]);
                }
            }
        }
        else
        {
            double axisWorld[3]{0.0, 0.0, 0.0};
            const double axisLocal[3] = {
                activeAxis == AxisLock::X ? 1.0 : 0.0,
                activeAxis == AxisLock::Y ? 1.0 : 0.0,
                activeAxis == AxisLock::Z ? 1.0 : 0.0
            };

            if (isLocal && objStartRot != nullptr)
            {
                const double rot[4] = {
                    static_cast<double>(objStartRot[0]),
                    static_cast<double>(objStartRot[1]),
                    static_cast<double>(objStartRot[2]),
                    static_cast<double>(objStartRot[3])
                };
                RotateVectorQuat(rot, axisLocal, axisWorld);
            }
            else
            {
                axisWorld[0] = axisLocal[0];
                axisWorld[1] = axisLocal[1];
                axisWorld[2] = axisLocal[2];
            }

            const double axisScreenX = axisWorld[0] * basis.right[0] + axisWorld[1] * basis.right[1] + axisWorld[2] * basis.right[2];
            const double axisScreenY = axisWorld[0] * basis.up[0] + axisWorld[1] * basis.up[1] + axisWorld[2] * basis.up[2];
            const double screenLenSq = axisScreenX * axisScreenX + axisScreenY * axisScreenY;

            double s = 0.0;
            if (screenLenSq < 1e-3)
            {
                s = screenDy * worldPerPixel;
            }
            else
            {
                s = ((screenDx * axisScreenX + screenDy * axisScreenY) / screenLenSq) * worldPerPixel;
            }

            if (snapEnabled && snapStep > 1e-6)
            {
                s = SnapTranslation(s, snapStep);
            }

            delta[0] = s * axisWorld[0];
            delta[1] = s * axisWorld[1];
            delta[2] = s * axisWorld[2];
        }

        return delta;
    }

    constexpr double kPi = 3.14159265358979323846;

    void NormalizeQuat(double q[4])
    {
        const double lenSq = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
        if (lenSq > 1e-12)
        {
            const double invLen = 1.0 / std::sqrt(lenSq);
            q[0] *= invLen;
            q[1] *= invLen;
            q[2] *= invLen;
            q[3] *= invLen;
        }
        else
        {
            q[0] = 0.0;
            q[1] = 0.0;
            q[2] = 0.0;
            q[3] = 1.0;
        }
    }

    void AxisAngleQuat(double ax, double ay, double az, double angleRad, double out[4])
    {
        const double axisLenSq = ax * ax + ay * ay + az * az;
        if (axisLenSq > 1e-12)
        {
            const double invAxisLen = 1.0 / std::sqrt(axisLenSq);
            ax *= invAxisLen;
            ay *= invAxisLen;
            az *= invAxisLen;
        }
        else
        {
            ax = 0.0;
            ay = 1.0;
            az = 0.0;
        }

        const double halfAngle = angleRad * 0.5;
        const double s = std::sin(halfAngle);
        out[0] = ax * s;
        out[1] = ay * s;
        out[2] = az * s;
        out[3] = std::cos(halfAngle);
    }

    void MulQuat(const double q1[4], const double q2[4], double out[4])
    {
        out[0] = q1[3] * q2[0] + q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1];
        out[1] = q1[3] * q2[1] - q1[0] * q2[2] + q1[1] * q2[3] + q1[2] * q2[0];
        out[2] = q1[3] * q2[2] + q1[0] * q2[1] - q1[1] * q2[0] + q1[2] * q2[3];
        out[3] = q1[3] * q2[3] - q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2];
    }

    void RotateVectorQuat(const double q[4], const double v[3], double out[3])
    {
        // t = 2 * cross(q.xyz, v)
        const double tx = 2.0 * (q[1] * v[2] - q[2] * v[1]);
        const double ty = 2.0 * (q[2] * v[0] - q[0] * v[2]);
        const double tz = 2.0 * (q[0] * v[1] - q[1] * v[0]);

        // v' = v + q.w * t + cross(q.xyz, t)
        out[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
        out[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
        out[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
    }

    double SnapTranslation(double val, double step)
    {
        if (step <= 1e-6)
            return val;
        return std::round(val / step) * step;
    }

    void SnapVector(const double in[3], double step, double out[3])
    {
        out[0] = SnapTranslation(in[0], step);
        out[1] = SnapTranslation(in[1], step);
        out[2] = SnapTranslation(in[2], step);
    }

    double SnapRotation(double angleRad, double stepDeg)
    {
        if (stepDeg <= 1e-6)
            return angleRad;
        const double stepRad = stepDeg * (kPi / 180.0);
        return std::round(angleRad / stepRad) * stepRad;
    }

    double SnapScale(double scale, double step)
    {
        if (step <= 1e-6)
            return scale;
        const double s = std::round(scale / step) * step;
        return s < 0.001 ? 0.001 : s;
    }

    void TransformDelta(const double delta[3], const double rotQuat[4], bool isLocal, double out[3])
    {
        if (!isLocal)
        {
            out[0] = delta[0];
            out[1] = delta[1];
            out[2] = delta[2];
            return;
        }

        RotateVectorQuat(rotQuat, delta, out);
    }

    void ApplyRotation(
        const double startRot[4],
        double deltaX,
        double deltaY,
        float sensitivity,
        bool snapEnabled,
        double snapStepDeg,
        double outRot[4]
    )
    {
        double normStart[4] = {startRot[0], startRot[1], startRot[2], startRot[3]};
        NormalizeQuat(normStart);

        double yaw = deltaX * static_cast<double>(sensitivity);
        double pitch = deltaY * static_cast<double>(sensitivity);

        if (snapEnabled)
        {
            yaw = SnapRotation(yaw, snapStepDeg);
            pitch = SnapRotation(pitch, snapStepDeg);
        }

        double qYaw[4];
        double qPitch[4];
        double qTmp[4];

        AxisAngleQuat(0.0, 1.0, 0.0, yaw, qYaw);
        AxisAngleQuat(1.0, 0.0, 0.0, pitch, qPitch);

        MulQuat(qYaw, qPitch, qTmp);
        MulQuat(qTmp, normStart, outRot);
        NormalizeQuat(outRot);
    }

    void ApplyScale(
        const double startScale[3],
        double deltaX,
        double deltaY,
        float sensitivity,
        bool snapEnabled,
        double snapStep,
        double outScale[3]
    )
    {
        double factor = std::exp((deltaX - deltaY) * static_cast<double>(sensitivity));
        factor = std::max(0.01, std::min(100.0, factor));

        if (snapEnabled)
            factor = SnapScale(factor, snapStep);

        for (int i = 0; i < 3; ++i)
            outScale[i] = startScale[i] * factor;
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
        float scaleSensitivity,
        bool isLocal)
    {
        EngineSceneTransform t = startTransform;

        if (mode == GizmoMode::Rotate)
        {
            double yaw = 0.0;
            double pitch = 0.0;
            double roll = 0.0;

            if (activeAxis == AxisLock::Y)
            {
                yaw = total.x() * static_cast<double>(rotateSensitivity);
            }
            else if (activeAxis == AxisLock::X)
            {
                pitch = total.y() * static_cast<double>(rotateSensitivity);
            }
            else if (activeAxis == AxisLock::Z)
            {
                roll = total.x() * static_cast<double>(rotateSensitivity);
            }
            else
            {
                yaw = total.x() * static_cast<double>(rotateSensitivity);
                pitch = total.y() * static_cast<double>(rotateSensitivity);
            }

            if (isSnapActive && rotateSnapStepDeg > 1e-4)
            {
                const double deg2rad = kPi / 180.0;
                const double stepRad = rotateSnapStepDeg * deg2rad;
                if (activeAxis == AxisLock::Y || activeAxis == AxisLock::All)
                    yaw = std::round(yaw / stepRad) * stepRad;
                if (activeAxis == AxisLock::X || activeAxis == AxisLock::All)
                    pitch = std::round(pitch / stepRad) * stepRad;
                if (activeAxis == AxisLock::Z)
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
            if (isLocal)
                MulQuat(startRot, qTmp2, qNew);
            else
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
} // namespace GizmoMath
