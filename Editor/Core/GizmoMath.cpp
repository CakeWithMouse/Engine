#include "GizmoMath.h"

namespace GizmoMath
{
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
        double yaw = deltaX * static_cast<double>(sensitivity);
        double pitch = -deltaY * static_cast<double>(sensitivity);

        if (snapEnabled)
        {
            yaw = SnapRotation(yaw, snapStepDeg);
            pitch = SnapRotation(pitch, snapStepDeg);
        }

        double normStart[4] = {startRot[0], startRot[1], startRot[2], startRot[3]};
        NormalizeQuat(normStart);

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
}

