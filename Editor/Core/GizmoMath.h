#pragma once

#include <cmath>
#include <algorithm>

namespace GizmoMath
{
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
}

