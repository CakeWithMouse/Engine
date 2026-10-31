#include "GizmoGeometry.h"

namespace GizmoGeometry
{
    float ComputeGizmoWorldScale(
        const float objPos[3],
        const EngineCameraInfo& cam,
        float screenPixelSize)
    {
        const float dx = objPos[0] - cam.position[0];
        const float dy = objPos[1] - cam.position[1];
        const float dz = objPos[2] - cam.position[2];
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float fovY = (cam.fov > 0.01f) ? cam.fov : 1.04719755f;
        const float viewH = (cam.height > 10) ? static_cast<float>(cam.height) : 600.0f;
        const float scale = dist * std::tan(fovY * 0.5f) * (screenPixelSize / (viewH * 0.5f));
        return std::max(0.001f, scale);
    }

    bool ProjectPointToScreen(
        const float world[3],
        const float view[16],
        const float proj[16],
        int screenWidth,
        int screenHeight,
        float screenOut[2])
    {
        // View transform (row-major DirectX XMFLOAT4X4)
        // v = [x, y, z, 1] * View
        const float vx = world[0] * view[0] + world[1] * view[4] + world[2] * view[8] + view[12];
        const float vy = world[0] * view[1] + world[1] * view[5] + world[2] * view[9] + view[13];
        const float vz = world[0] * view[2] + world[1] * view[6] + world[2] * view[10] + view[14];
        const float vw = world[0] * view[3] + world[1] * view[7] + world[2] * view[11] + view[15];

        // Clip transform
        const float cx = vx * proj[0] + vy * proj[4] + vz * proj[8] + vw * proj[12];
        const float cy = vx * proj[1] + vy * proj[5] + vz * proj[9] + vw * proj[13];
        const float cw = vx * proj[3] + vy * proj[7] + vz * proj[11] + vw * proj[15];

        if (cw <= 0.0001f) return false;

        const float ndcX = cx / cw;
        const float ndcY = cy / cw;

        screenOut[0] = (ndcX + 1.0f) * 0.5f * static_cast<float>(screenWidth);
        screenOut[1] = (1.0f - ndcY) * 0.5f * static_cast<float>(screenHeight);
        return true;
    }

    float DistancePointToSegment(
        float px, float py,
        float x0, float y0,
        float x1, float y1)
    {
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-6f)
        {
            const float ex = px - x0;
            const float ey = py - y0;
            return std::sqrt(ex * ex + ey * ey);
        }

        const float t = std::clamp(((px - x0) * dx + (py - y0) * dy) / lenSq, 0.0f, 1.0f);
        const float projX = x0 + t * dx;
        const float projY = y0 + t * dy;
        const float ex = px - projX;
        const float ey = py - projY;
        return std::sqrt(ex * ex + ey * ey);
    }

    bool RayPlaneIntersection(
        const float rayOrigin[3],
        const float rayDir[3],
        const float planePoint[3],
        const float planeNormal[3],
        float hitPoint[3])
    {
        const float denom = rayDir[0] * planeNormal[0] + rayDir[1] * planeNormal[1] + rayDir[2] * planeNormal[2];
        if (std::abs(denom) < 1e-6f) return false;

        const float dx = planePoint[0] - rayOrigin[0];
        const float dy = planePoint[1] - rayOrigin[1];
        const float dz = planePoint[2] - rayOrigin[2];
        const float t = (dx * planeNormal[0] + dy * planeNormal[1] + dz * planeNormal[2]) / denom;
        if (t < 0.0f) return false;

        hitPoint[0] = rayOrigin[0] + t * rayDir[0];
        hitPoint[1] = rayOrigin[1] + t * rayDir[1];
        hitPoint[2] = rayOrigin[2] + t * rayDir[2];
        return true;
    }

    float ComputeRingAngle(
        const float center[3],
        const float axisNormal[3],
        const float refVector[3],
        const float worldPoint[3])
    {
        float vx = worldPoint[0] - center[0];
        float vy = worldPoint[1] - center[1];
        float vz = worldPoint[2] - center[2];

        // Project onto plane: v - (v . n) * n
        const float dotN = vx * axisNormal[0] + vy * axisNormal[1] + vz * axisNormal[2];
        vx -= dotN * axisNormal[0];
        vy -= dotN * axisNormal[1];
        vz -= dotN * axisNormal[2];

        const float len = std::sqrt(vx * vx + vy * vy + vz * vz);
        if (len < 1e-6f) return 0.0f;
        vx /= len; vy /= len; vz /= len;

        const float dotRef = vx * refVector[0] + vy * refVector[1] + vz * refVector[2];
        // Cross product: refVector x v
        const float cx = refVector[1] * vz - refVector[2] * vy;
        const float cy = refVector[2] * vx - refVector[0] * vz;
        const float cz = refVector[0] * vy - refVector[1] * vx;
        const float sign = (cx * axisNormal[0] + cy * axisNormal[1] + cz * axisNormal[2]) >= 0.0f ? 1.0f : -1.0f;

        const float clampedCos = std::clamp(dotRef, -1.0f, 1.0f);
        return sign * std::acos(clampedCos);
    }

    static void GetGizmoAxes(
        const float objRot[4],
        bool isLocal,
        float axisX[3],
        float axisY[3],
        float axisZ[3])
    {
        if (isLocal && objRot)
        {
            const double rot[4] = {objRot[0], objRot[1], objRot[2], objRot[3]};
            const double ux[3] = {1.0, 0.0, 0.0};
            const double uy[3] = {0.0, 1.0, 0.0};
            const double uz[3] = {0.0, 0.0, 1.0};
            double rx[3], ry[3], rz[3];
            GizmoMath::RotateVectorQuat(rot, ux, rx);
            GizmoMath::RotateVectorQuat(rot, uy, ry);
            GizmoMath::RotateVectorQuat(rot, uz, rz);
            axisX[0] = static_cast<float>(rx[0]); axisX[1] = static_cast<float>(rx[1]); axisX[2] = static_cast<float>(rx[2]);
            axisY[0] = static_cast<float>(ry[0]); axisY[1] = static_cast<float>(ry[1]); axisY[2] = static_cast<float>(ry[2]);
            axisZ[0] = static_cast<float>(rz[0]); axisZ[1] = static_cast<float>(rz[1]); axisZ[2] = static_cast<float>(rz[2]);
        }
        else
        {
            axisX[0] = 1.0f; axisX[1] = 0.0f; axisX[2] = 0.0f;
            axisY[0] = 0.0f; axisY[1] = 1.0f; axisY[2] = 0.0f;
            axisZ[0] = 0.0f; axisZ[1] = 0.0f; axisZ[2] = 1.0f;
        }
    }

    Handle PickHandle(
        GizmoMath::GizmoMode mode,
        const float objPos[3],
        const float objRot[4],
        bool isLocal,
        float mouseX,
        float mouseY,
        const EngineCameraInfo& cam,
        float thresholdPx)
    {
        if (!objPos) return Handle::None;

        const float scale = ComputeGizmoWorldScale(objPos, cam);
        const int w = static_cast<int>(cam.width);
        const int h = static_cast<int>(cam.height);

        float centerScreen[2];
        if (!ProjectPointToScreen(objPos, cam.viewMatrix, cam.projMatrix, w, h, centerScreen))
            return Handle::None;

        // Center handle check
        const float centerDist = std::hypot(mouseX - centerScreen[0], mouseY - centerScreen[1]);
        if (centerDist < thresholdPx * 1.5f)
            return Handle::Center;

        float ax[3], ay[3], az[3];
        GetGizmoAxes(objRot, isLocal, ax, ay, az);

        float closestDist = thresholdPx;
        Handle bestHandle = Handle::None;

        if (mode == GizmoMath::GizmoMode::Translate)
        {
            // Test planes (XY, XZ, YZ)
            const float planeSize = scale * 0.35f;
            auto testPlane = [&](const float a1[3], const float a2[3], Handle hnd) {
                float corner[3] = {objPos[0] + (a1[0] + a2[0]) * planeSize,
                                   objPos[1] + (a1[1] + a2[1]) * planeSize,
                                   objPos[2] + (a1[2] + a2[2]) * planeSize};
                float p1[3] = {objPos[0] + a1[0] * planeSize, objPos[1] + a1[1] * planeSize, objPos[2] + a1[2] * planeSize};
                float p2[3] = {objPos[0] + a2[0] * planeSize, objPos[1] + a2[1] * planeSize, objPos[2] + a2[2] * planeSize};

                float sc[2], sp1[2], sp2[2];
                if (ProjectPointToScreen(corner, cam.viewMatrix, cam.projMatrix, w, h, sc) &&
                    ProjectPointToScreen(p1, cam.viewMatrix, cam.projMatrix, w, h, sp1) &&
                    ProjectPointToScreen(p2, cam.viewMatrix, cam.projMatrix, w, h, sp2))
                {
                    const float d1 = DistancePointToSegment(mouseX, mouseY, sp1[0], sp1[1], sc[0], sc[1]);
                    const float d2 = DistancePointToSegment(mouseX, mouseY, sp2[0], sp2[1], sc[0], sc[1]);
                    const float minD = std::min(d1, d2);
                    if (minD < closestDist)
                    {
                        closestDist = minD;
                        bestHandle = hnd;
                    }
                }
            };

            testPlane(ax, ay, Handle::PlaneXY);
            testPlane(ax, az, Handle::PlaneXZ);
            testPlane(ay, az, Handle::PlaneYZ);

            // Test axis lines
            auto testAxis = [&](const float a[3], Handle hnd) {
                float tip[3] = {objPos[0] + a[0] * scale, objPos[1] + a[1] * scale, objPos[2] + a[2] * scale};
                float tipScreen[2];
                if (ProjectPointToScreen(tip, cam.viewMatrix, cam.projMatrix, w, h, tipScreen))
                {
                    const float d = DistancePointToSegment(mouseX, mouseY, centerScreen[0], centerScreen[1], tipScreen[0], tipScreen[1]);
                    if (d < closestDist)
                    {
                        closestDist = d;
                        bestHandle = hnd;
                    }
                }
            };

            testAxis(ax, Handle::AxisX);
            testAxis(ay, Handle::AxisY);
            testAxis(az, Handle::AxisZ);
        }
        else if (mode == GizmoMath::GizmoMode::Rotate)
        {
            // Test 3 rotation rings (sampled as 48 segments)
            auto testRing = [&](const float a1[3], const float a2[3], Handle hnd) {
                constexpr int segs = 32;
                constexpr float step = 6.28318530718f / static_cast<float>(segs);
                float prevScreen[2];
                bool hasPrev = false;

                for (int i = 0; i <= segs; ++i)
                {
                    const float angle = static_cast<float>(i) * step;
                    const float p[3] = {
                        objPos[0] + (a1[0] * std::cos(angle) + a2[0] * std::sin(angle)) * scale,
                        objPos[1] + (a1[1] * std::cos(angle) + a2[1] * std::sin(angle)) * scale,
                        objPos[2] + (a1[2] * std::cos(angle) + a2[2] * std::sin(angle)) * scale
                    };
                    float scr[2];
                    if (ProjectPointToScreen(p, cam.viewMatrix, cam.projMatrix, w, h, scr))
                    {
                        if (hasPrev)
                        {
                            const float d = DistancePointToSegment(mouseX, mouseY, prevScreen[0], prevScreen[1], scr[0], scr[1]);
                            if (d < closestDist)
                            {
                                closestDist = d;
                                bestHandle = hnd;
                            }
                        }
                        prevScreen[0] = scr[0]; prevScreen[1] = scr[1];
                        hasPrev = true;
                    }
                    else hasPrev = false;
                }
            };

            testRing(ay, az, Handle::AxisX);
            testRing(ax, az, Handle::AxisY);
            testRing(ax, ay, Handle::AxisZ);
        }
        else if (mode == GizmoMath::GizmoMode::Scale)
        {
            auto testAxis = [&](const float a[3], Handle hnd) {
                float tip[3] = {objPos[0] + a[0] * scale, objPos[1] + a[1] * scale, objPos[2] + a[2] * scale};
                float tipScreen[2];
                if (ProjectPointToScreen(tip, cam.viewMatrix, cam.projMatrix, w, h, tipScreen))
                {
                    const float d = DistancePointToSegment(mouseX, mouseY, centerScreen[0], centerScreen[1], tipScreen[0], tipScreen[1]);
                    if (d < closestDist)
                    {
                        closestDist = d;
                        bestHandle = hnd;
                    }
                }
            };

            testAxis(ax, Handle::AxisX);
            testAxis(ay, Handle::AxisY);
            testAxis(az, Handle::AxisZ);
        }

        return bestHandle;
    }

    std::vector<EngineOverlayLine> BuildGizmoLines(
        GizmoMath::GizmoMode mode,
        const float objPos[3],
        const float objRot[4],
        bool isLocal,
        const EngineCameraInfo& cam,
        Handle hoverHandle,
        Handle activeHandle,
        const Palette& palette)
    {
        std::vector<EngineOverlayLine> lines;
        if (!objPos) return lines;

        const float scale = ComputeGizmoWorldScale(objPos, cam);
        float ax[3], ay[3], az[3];
        GetGizmoAxes(objRot, isLocal, ax, ay, az);

        const Handle effective = (activeHandle != Handle::None) ? activeHandle : hoverHandle;
        auto pickColor = [&](Handle h, uint32_t normalColor) -> uint32_t {
            if (effective == h) return palette.highlight;
            if (effective != Handle::None)
            {
                // Dim other handles slightly
                const uint32_t a = ((normalColor >> 24) & 0xFF) * 2 / 3;
                return (normalColor & 0x00FFFFFF) | (a << 24);
            }
            return normalColor;
        };

        constexpr uint32_t flags = 2; // alwaysOnTop

        if (mode == GizmoMath::GizmoMode::Translate)
        {
            // Move axes with arrows
            auto addAxis = [&](const float a[3], const float u1[3], const float u2[3], Handle hnd, uint32_t col) {
                const uint32_t c = pickColor(hnd, col);
                const float tip[3] = {objPos[0] + a[0] * scale, objPos[1] + a[1] * scale, objPos[2] + a[2] * scale};
                lines.push_back({objPos[0], objPos[1], objPos[2], tip[0], tip[1], tip[2], c, flags});

                // Arrow head cone (4 side lines from tip to base)
                const float coneLen = scale * 0.18f;
                const float coneR = scale * 0.05f;
                const float base[3] = {tip[0] - a[0] * coneLen, tip[1] - a[1] * coneLen, tip[2] - a[2] * coneLen};

                const float b1[3] = {base[0] + u1[0] * coneR, base[1] + u1[1] * coneR, base[2] + u1[2] * coneR};
                const float b2[3] = {base[0] - u1[0] * coneR, base[1] - u1[1] * coneR, base[2] - u1[2] * coneR};
                const float b3[3] = {base[0] + u2[0] * coneR, base[1] + u2[1] * coneR, base[2] + u2[2] * coneR};
                const float b4[3] = {base[0] - u2[0] * coneR, base[1] - u2[1] * coneR, base[2] - u2[2] * coneR};

                lines.push_back({tip[0], tip[1], tip[2], b1[0], b1[1], b1[2], c, flags});
                lines.push_back({tip[0], tip[1], tip[2], b2[0], b2[1], b2[2], c, flags});
                lines.push_back({tip[0], tip[1], tip[2], b3[0], b3[1], b3[2], c, flags});
                lines.push_back({tip[0], tip[1], tip[2], b4[0], b4[1], b4[2], c, flags});
                lines.push_back({b1[0], b1[1], b1[2], b3[0], b3[1], b3[2], c, flags});
                lines.push_back({b3[0], b3[1], b3[2], b2[0], b2[1], b2[2], c, flags});
                lines.push_back({b2[0], b2[1], b2[2], b4[0], b4[1], b4[2], c, flags});
                lines.push_back({b4[0], b4[1], b4[2], b1[0], b1[1], b1[2], c, flags});
            };

            addAxis(ax, ay, az, Handle::AxisX, palette.axisX);
            addAxis(ay, ax, az, Handle::AxisY, palette.axisY);
            addAxis(az, ax, ay, Handle::AxisZ, palette.axisZ);

            // Plane quads
            auto addPlane = [&](const float a1[3], const float a2[3], Handle hnd, uint32_t col) {
                const uint32_t c = pickColor(hnd, col);
                const float pSize = scale * 0.3f;
                const float p1[3] = {objPos[0] + a1[0] * pSize, objPos[1] + a1[1] * pSize, objPos[2] + a1[2] * pSize};
                const float p2[3] = {objPos[0] + a2[0] * pSize, objPos[1] + a2[1] * pSize, objPos[2] + a2[2] * pSize};
                const float pc[3] = {objPos[0] + (a1[0] + a2[0]) * pSize, objPos[1] + (a1[1] + a2[1]) * pSize, objPos[2] + (a1[2] + a2[2]) * pSize};
                lines.push_back({p1[0], p1[1], p1[2], pc[0], pc[1], pc[2], c, flags});
                lines.push_back({p2[0], p2[1], p2[2], pc[0], pc[1], pc[2], c, flags});
            };

            addPlane(ax, ay, Handle::PlaneXY, palette.planeXY);
            addPlane(ax, az, Handle::PlaneXZ, palette.planeXZ);
            addPlane(ay, az, Handle::PlaneYZ, palette.planeYZ);
        }
        else if (mode == GizmoMath::GizmoMode::Rotate)
        {
            auto addRing = [&](const float a1[3], const float a2[3], Handle hnd, uint32_t col) {
                const uint32_t c = pickColor(hnd, col);
                constexpr int segs = 48;
                constexpr float step = 6.28318530718f / static_cast<float>(segs);
                for (int i = 0; i < segs; ++i)
                {
                    const float a0 = static_cast<float>(i) * step;
                    const float a1_ang = static_cast<float>(i + 1) * step;
                    const float p0[3] = {
                        objPos[0] + (a1[0] * std::cos(a0) + a2[0] * std::sin(a0)) * scale,
                        objPos[1] + (a1[1] * std::cos(a0) + a2[1] * std::sin(a0)) * scale,
                        objPos[2] + (a1[2] * std::cos(a0) + a2[2] * std::sin(a0)) * scale
                    };
                    const float p1[3] = {
                        objPos[0] + (a1[0] * std::cos(a1_ang) + a2[0] * std::sin(a1_ang)) * scale,
                        objPos[1] + (a1[1] * std::cos(a1_ang) + a2[1] * std::sin(a1_ang)) * scale,
                        objPos[2] + (a1[2] * std::cos(a1_ang) + a2[2] * std::sin(a1_ang)) * scale
                    };
                    lines.push_back({p0[0], p0[1], p0[2], p1[0], p1[1], p1[2], c, flags});
                }
            };

            addRing(ay, az, Handle::AxisX, palette.axisX);
            addRing(ax, az, Handle::AxisY, palette.axisY);
            addRing(ax, ay, Handle::AxisZ, palette.axisZ);
        }
        else if (mode == GizmoMath::GizmoMode::Scale)
        {
            auto addScaleAxis = [&](const float a[3], const float u1[3], const float u2[3], Handle hnd, uint32_t col) {
                const uint32_t c = pickColor(hnd, col);
                const float tip[3] = {objPos[0] + a[0] * scale, objPos[1] + a[1] * scale, objPos[2] + a[2] * scale};
                lines.push_back({objPos[0], objPos[1], objPos[2], tip[0], tip[1], tip[2], c, flags});

                // End box (12 edges)
                const float bSize = scale * 0.05f;
                for (int dx = -1; dx <= 1; dx += 2)
                for (int dy = -1; dy <= 1; dy += 2)
                {
                    const float p1[3] = {tip[0] + (u1[0] * dx + u2[0] * dy) * bSize - a[0] * bSize,
                                         tip[1] + (u1[1] * dx + u2[1] * dy) * bSize - a[1] * bSize,
                                         tip[2] + (u1[2] * dx + u2[2] * dy) * bSize - a[2] * bSize};
                    const float p2[3] = {tip[0] + (u1[0] * dx + u2[0] * dy) * bSize + a[0] * bSize,
                                         tip[1] + (u1[1] * dx + u2[1] * dy) * bSize + a[1] * bSize,
                                         tip[2] + (u1[2] * dx + u2[2] * dy) * bSize + a[2] * bSize};
                    lines.push_back({p1[0], p1[1], p1[2], p2[0], p2[1], p2[2], c, flags});
                }
            };

            addScaleAxis(ax, ay, az, Handle::AxisX, palette.axisX);
            addScaleAxis(ay, ax, az, Handle::AxisY, palette.axisY);
            addScaleAxis(az, ax, ay, Handle::AxisZ, palette.axisZ);

            // Center uniform scale box
            const uint32_t cc = pickColor(Handle::Center, palette.center);
            const float cs = scale * 0.06f;
            lines.push_back({objPos[0] - cs, objPos[1] - cs, objPos[2] - cs, objPos[0] + cs, objPos[1] - cs, objPos[2] - cs, cc, flags});
            lines.push_back({objPos[0] + cs, objPos[1] - cs, objPos[2] - cs, objPos[0] + cs, objPos[1] + cs, objPos[2] - cs, cc, flags});
            lines.push_back({objPos[0] + cs, objPos[1] + cs, objPos[2] - cs, objPos[0] - cs, objPos[1] + cs, objPos[2] - cs, cc, flags});
            lines.push_back({objPos[0] - cs, objPos[1] + cs, objPos[2] - cs, objPos[0] - cs, objPos[1] - cs, objPos[2] - cs, cc, flags});

            lines.push_back({objPos[0] - cs, objPos[1] - cs, objPos[2] + cs, objPos[0] + cs, objPos[1] - cs, objPos[2] + cs, cc, flags});
            lines.push_back({objPos[0] + cs, objPos[1] - cs, objPos[2] + cs, objPos[0] + cs, objPos[1] + cs, objPos[2] + cs, cc, flags});
            lines.push_back({objPos[0] + cs, objPos[1] + cs, objPos[2] + cs, objPos[0] - cs, objPos[1] + cs, objPos[2] + cs, cc, flags});
            lines.push_back({objPos[0] - cs, objPos[1] + cs, objPos[2] + cs, objPos[0] - cs, objPos[1] - cs, objPos[2] + cs, cc, flags});

            lines.push_back({objPos[0] - cs, objPos[1] - cs, objPos[2] - cs, objPos[0] - cs, objPos[1] - cs, objPos[2] + cs, cc, flags});
            lines.push_back({objPos[0] + cs, objPos[1] - cs, objPos[2] - cs, objPos[0] + cs, objPos[1] - cs, objPos[2] + cs, cc, flags});
            lines.push_back({objPos[0] + cs, objPos[1] + cs, objPos[2] - cs, objPos[0] + cs, objPos[1] + cs, objPos[2] + cs, cc, flags});
            lines.push_back({objPos[0] - cs, objPos[1] + cs, objPos[2] - cs, objPos[0] - cs, objPos[1] + cs, objPos[2] + cs, cc, flags});
        }

        return lines;
    }
}

