#pragma once

#include "GizmoGeometry.h"
#include "ViewportOverlay.h"
#include <cassert>
#include <iostream>
#include <cmath>

template <typename F>
inline void RunGizmoGeometryTests(F check)
{
    using namespace GizmoGeometry;

    // 1. ComputeGizmoWorldScale proportional to distance
    {
        EngineCameraInfo cam{};
        cam.position[0] = 0.0f; cam.position[1] = 0.0f; cam.position[2] = -10.0f;
        cam.fov = 1.04719755f; // 60 deg
        cam.width = 800; cam.height = 600;

        float obj1[3] = {0.0f, 0.0f, 0.0f}; // dist 10
        float scale1 = ComputeGizmoWorldScale(obj1, cam, 90.0f);

        float obj2[3] = {0.0f, 0.0f, 10.0f}; // dist 20
        float scale2 = ComputeGizmoWorldScale(obj2, cam, 90.0f);

        check(scale1 > 0.0f, "Gizmo scale positive");
        check(std::abs(scale2 - scale1 * 2.0f) < 0.01f, "Gizmo scale doubles when distance doubles");
    }

    // 2. ProjectPointToScreen with simple view/proj
    {
        // Simple camera looking down +Z from (0,0,0)
        float view[16] = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };
        float proj[16] = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,1,
            0,0,0,0
        };
        float pt[3] = {0.0f, 0.0f, 5.0f};
        float screen[2]{};
        bool ok = ProjectPointToScreen(pt, view, proj, 800, 600, screen);
        check(ok, "ProjectPointToScreen succeeds for point in front of camera");
        check(std::abs(screen[0] - 400.0f) < 1.0f && std::abs(screen[1] - 300.0f) < 1.0f,
              "Origin projects to screen center");

        float behind[3] = {0.0f, 0.0f, -5.0f};
        bool behindOk = ProjectPointToScreen(behind, view, proj, 800, 600, screen);
        check(!behindOk, "ProjectPointToScreen rejects points behind camera");
    }

    // 3. DistancePointToSegment
    {
        // Segment from (0, 0) to (10, 0)
        float dOn = DistancePointToSegment(5.0f, 0.0f, 0.0f, 0.0f, 10.0f, 0.0f);
        check(dOn < 1e-5f, "Point on segment has distance 0");

        float dAbove = DistancePointToSegment(5.0f, 3.0f, 0.0f, 0.0f, 10.0f, 0.0f);
        check(std::abs(dAbove - 3.0f) < 1e-4f, "Point perpendicular to segment");

        float dPast = DistancePointToSegment(14.0f, 3.0f, 0.0f, 0.0f, 10.0f, 0.0f);
        // Dist to endpoint (10, 0) is sqrt(4^2 + 3^2) = 5
        check(std::abs(dPast - 5.0f) < 1e-4f, "Point beyond endpoint clamps to endpoint");
    }

    // 4. RayPlaneIntersection
    {
        float origin[3] = {0.0f, 10.0f, 0.0f};
        float dir[3] = {0.0f, -1.0f, 0.0f};
        float planePt[3] = {0.0f, 0.0f, 0.0f};
        float planeN[3] = {0.0f, 1.0f, 0.0f};
        float hit[3]{};

        bool ok = RayPlaneIntersection(origin, dir, planePt, planeN, hit);
        check(ok, "Ray intersects ground plane");
        check(std::abs(hit[0]) < 1e-4f && std::abs(hit[1]) < 1e-4f && std::abs(hit[2]) < 1e-4f,
              "Hit point is (0, 0, 0)");

        float upDir[3] = {0.0f, 1.0f, 0.0f};
        bool upOk = RayPlaneIntersection(origin, upDir, planePt, planeN, hit);
        check(!upOk, "Ray pointing away does not intersect plane");
    }

    // 5. ComputeRingAngle
    {
        float center[3] = {0.0f, 0.0f, 0.0f};
        float axisN[3] = {0.0f, 1.0f, 0.0f}; // Y axis
        float refV[3] = {1.0f, 0.0f, 0.0f};  // X axis

        float pt0[3] = {5.0f, 0.0f, 0.0f};
        float a0 = ComputeRingAngle(center, axisN, refV, pt0);
        check(std::abs(a0) < 1e-4f, "Angle at refVector is 0");

        float pt90[3] = {0.0f, 0.0f, -5.0f};
        float a90 = ComputeRingAngle(center, axisN, refV, pt90);
        check(std::abs(std::abs(a90) - 1.570796f) < 1e-3f, "Angle at 90 deg is pi/2");
    }

    // 6. Handle picking
    {
        EngineCameraInfo cam{};
        cam.position[0] = 0.0f; cam.position[1] = 0.0f; cam.position[2] = -10.0f;
        cam.fov = 1.04719755f;
        cam.width = 800; cam.height = 600;

        // View looking at (0,0,0) from (0,0,-10)
        // [X, Y, Z, 1] * View = [X, Y, Z + 10, 1]
        cam.viewMatrix[0] = 1; cam.viewMatrix[5] = 1; cam.viewMatrix[10] = 1; cam.viewMatrix[15] = 1;
        cam.viewMatrix[14] = 10.0f; // offset in Z
        cam.projMatrix[0] = 1.3f; cam.projMatrix[5] = 1.73f; cam.projMatrix[10] = 1.0f; cam.projMatrix[11] = 1.0f;

        float obj[3] = {0.0f, 0.0f, 0.0f};
        float rot[4] = {0.0f, 0.0f, 0.0f, 1.0f};

        // Center click
        Handle hCenter = PickHandle(GizmoMath::GizmoMode::Translate, obj, rot, false, 400.0f, 300.0f, cam, 15.0f);
        check(hCenter == Handle::Center, "Click at screen center picks Handle::Center");

        // Far away click
        Handle hNone = PickHandle(GizmoMath::GizmoMode::Translate, obj, rot, false, 100.0f, 100.0f, cam, 8.0f);
        check(hNone == Handle::None, "Click far away picks Handle::None");
    }

    // 7. ViewportOverlay::GenerateGridLines & LightRadius
    {
        ViewportOverlay::Palette pal;
        auto grid = ViewportOverlay::GenerateGridLines(0.0f, 0.0f, pal, 1.0f, 10, 20, 20.0f);
        check(!grid.empty(), "Grid lines generated");
        check(grid.size() < 300, "Grid line count within budget");

        auto lightSphere = ViewportOverlay::GenerateLightRadiusLines(0.0f, 5.0f, 0.0f, 10.0f, 0xFFFFFFFF, 48);
        check(lightSphere.size() == 48 * 3, "Light sphere generates 3 circles of 48 segments");
    }

    // 8. ClosestPointOnAxis ray projection
    {
        float rayOrig[3] = {2.0f, 5.0f, -10.0f};
        float rayDir[3] = {0.0f, 0.0f, 1.0f};
        float axisOrig[3] = {0.0f, 0.0f, 0.0f};
        float axisDir[3] = {1.0f, 0.0f, 0.0f};
        float hitPt[3]{};
        float s = 0.0f;

        bool ok = ClosestPointOnAxis(rayOrig, rayDir, axisOrig, axisDir, hitPt, &s);
        check(ok, "ClosestPointOnAxis succeeds for non-parallel lines");
        check(std::abs(s - 2.0f) < 1e-4f, "Axis coordinate s is 2.0");
        check(std::abs(hitPt[0] - 2.0f) < 1e-4f && std::abs(hitPt[1]) < 1e-4f && std::abs(hitPt[2]) < 1e-4f,
              "Hit point on axis is (2, 0, 0)");
    }

    // 9. ScreenToWorldRay camera unprojection
    {
        EngineCameraInfo cam{};
        cam.position[0] = 0.0f; cam.position[1] = 0.0f; cam.position[2] = -10.0f;
        cam.width = 800; cam.height = 600;
        // View matrix identity (camera at (0,0,-10) looking down +Z)
        cam.viewMatrix[0] = 1; cam.viewMatrix[5] = 1; cam.viewMatrix[10] = 1; cam.viewMatrix[15] = 1;
        cam.projMatrix[0] = 1.0f; cam.projMatrix[5] = 1.0f; cam.projMatrix[10] = 1.0f; cam.projMatrix[11] = 1.0f;

        float rOrig[3]{}, rDir[3]{};
        bool ok = ScreenToWorldRay(400.0f, 300.0f, cam, rOrig, rDir);
        check(ok, "ScreenToWorldRay succeeds at screen center");
        check(std::abs(rDir[0]) < 1e-4f && std::abs(rDir[1]) < 1e-4f && std::abs(rDir[2] - 1.0f) < 1e-4f,
              "Center ray points forward (0, 0, 1)");

        float rDirRight[3]{};
        ScreenToWorldRay(800.0f, 300.0f, cam, rOrig, rDirRight);
        check(rDirRight[0] > 0.1f, "Right edge ray has positive X direction");
    }

    // 10. Signed ring angle verification
    {
        float center[3] = {0.0f, 0.0f, 0.0f};
        float axisN[3] = {0.0f, 1.0f, 0.0f};
        float refV[3] = {1.0f, 0.0f, 0.0f};

        float ptPos[3] = {0.0f, 0.0f, 5.0f};
        float aPos = ComputeRingAngle(center, axisN, refV, ptPos);

        float ptNeg[3] = {0.0f, 0.0f, -5.0f};
        float aNeg = ComputeRingAngle(center, axisN, refV, ptNeg);

        check((aPos * aNeg) < 0.0f, "Opposite rotation directions have opposite signs");
        check(std::abs(std::abs(aPos) - 1.570796f) < 1e-3f, "Magnitude is pi/2");
    }

    // 11. Accumulated gesture snap calculation
    {
        const double step = 0.5;
        const float dragStartS = 1.0f;

        auto snap = [&](float currentS) -> float {
            float delta = currentS - dragStartS;
            return static_cast<float>(std::round(delta / step) * step);
        };

        check(std::abs(snap(1.24f) - 0.0f) < 1e-4f, "Delta 0.24 snaps to 0.0");
        check(std::abs(snap(1.26f) - 0.5f) < 1e-4f, "Delta 0.26 snaps to 0.5");
        check(std::abs(snap(1.49f) - 0.5f) < 1e-4f, "Delta 0.49 snaps to 0.5");
        check(std::abs(snap(1.76f) - 1.0f) < 1e-4f, "Delta 0.76 snaps to 1.0");
    }

    // 12. Multi-angle unit tests for X, Y, Z axes and XY, XZ, YZ planes (World & Local)
    {
        auto makeCam = [](float px, float py, float pz, float tx, float ty, float tz, float upx, float upy, float upz) -> EngineCameraInfo {
            EngineCameraInfo c{};
            c.position[0] = px; c.position[1] = py; c.position[2] = pz;
            c.width = 800; c.height = 600;
            c.fov = 1.04719755f;

            float fx = tx - px, fy = ty - py, fz = tz - pz;
            float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
            fx /= flen; fy /= flen; fz /= flen;

            float rx = upy * fz - upz * fy;
            float ry = upz * fx - upx * fz;
            float rz = upx * fy - upy * fx;
            float rlen = std::sqrt(rx*rx + ry*ry + rz*rz);
            rx /= rlen; ry /= rlen; rz /= rlen;

            float ux = fy * rz - fz * ry;
            float uy = fz * rx - fx * rz;
            float uz = fx * ry - fy * rx;

            c.viewMatrix[0] = rx; c.viewMatrix[1] = ux; c.viewMatrix[2] = fx; c.viewMatrix[3] = 0.0f;
            c.viewMatrix[4] = ry; c.viewMatrix[5] = uy; c.viewMatrix[6] = fy; c.viewMatrix[7] = 0.0f;
            c.viewMatrix[8] = rz; c.viewMatrix[9] = uz; c.viewMatrix[10] = fz; c.viewMatrix[11] = 0.0f;
            c.viewMatrix[12] = -(px*rx + py*ry + pz*rz);
            c.viewMatrix[13] = -(px*ux + py*uy + pz*uz);
            c.viewMatrix[14] = -(px*fx + py*fy + pz*fz);
            c.viewMatrix[15] = 1.0f;

            const float aspect = 800.0f / 600.0f;
            const float sy = 1.0f / std::tan(c.fov * 0.5f);
            const float sx = sy / aspect;
            c.projMatrix[0] = sx; c.projMatrix[5] = sy; c.projMatrix[10] = 1.0f; c.projMatrix[11] = 1.0f;
            c.projMatrix[14] = -0.1f;
            return c;
        };

        const EngineCameraInfo cameras[5] = {
            makeCam(0.0f, 0.0f, -10.0f, 0,0,0, 0,1,0),     // 1. Front view (looking +Z)
            makeCam(0.0f, 10.0f, -0.01f, 0,0,0, 0,0,1),    // 2. Top-down view (looking down Y)
            makeCam(7.07f, 5.0f, -7.07f, 0,0,0, 0,1,0),    // 3. Isometric 45-deg view
            makeCam(10.0f, 0.0f, 0.0f, 0,0,0, 0,1,0),      // 4. Side view
            makeCam(-6.0f, -4.0f, -8.0f, 0,0,0, 0,1,0)     // 5. Negative pitch/yaw view
        };

        // Test World axes across all camera angles
        for (int camIdx = 0; camIdx < 5; ++camIdx)
        {
            const auto& cam = cameras[camIdx];
            float p0[3] = {0.0f, 0.0f, 0.0f};
            float s0[2]{};
            check(ProjectPointToScreen(p0, cam.viewMatrix, cam.projMatrix, 800, 600, s0), "Origin projects to screen");

            const float axes[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
            for (int a = 0; a < 3; ++a)
            {
                float p1[3] = {axes[a][0] * 2.0f, axes[a][1] * 2.0f, axes[a][2] * 2.0f};
                float s1[2]{};
                if (!ProjectPointToScreen(p1, cam.viewMatrix, cam.projMatrix, 800, 600, s1)) continue;

                const float screenDist = std::hypot(s1[0] - s0[0], s1[1] - s0[1]);
                if (screenDist < 10.0f) continue; // Axis nearly parallel to view ray

                // Drag midway along screen projection of arrow
                const float mx = (s0[0] + s1[0]) * 0.5f;
                const float my = (s0[1] + s1[1]) * 0.5f;

                float rOrig[3], rDir[3];
                check(ScreenToWorldRay(mx, my, cam, rOrig, rDir), "ScreenToWorldRay succeeds on arrow drag");

                float hitPt[3]{}, s = 0.0f;
                check(ClosestPointOnAxis(rOrig, rDir, p0, axes[a], hitPt, &s), "ClosestPointOnAxis succeeds");
                check(s > 0.05f, "Drag along arrow gives strictly positive displacement");

                // Check no parasitic motion along orthogonal axes
                const int o1 = (a + 1) % 3, o2 = (a + 2) % 3;
                check(std::abs(hitPt[o1]) < 1e-4f && std::abs(hitPt[o2]) < 1e-4f, "No parasitic motion along other axes");
            }
        }

        // Test Local axes on rotated object (yaw 45 deg around Y)
        const float rot45[4] = {0.0f, 0.3826834f, 0.0f, 0.9238795f};
        float lax[3], lay[3], laz[3];
        GetGizmoAxes(rot45, true, lax, lay, laz);

        for (int camIdx = 0; camIdx < 5; ++camIdx)
        {
            const auto& cam = cameras[camIdx];
            float p0[3] = {0.0f, 0.0f, 0.0f};
            float s0[2]{};
            if (!ProjectPointToScreen(p0, cam.viewMatrix, cam.projMatrix, 800, 600, s0)) continue;

            const float* localAxes[3] = {lax, lay, laz};
            for (int a = 0; a < 3; ++a)
            {
                float p1[3] = {localAxes[a][0] * 2.0f, localAxes[a][1] * 2.0f, localAxes[a][2] * 2.0f};
                float s1[2]{};
                if (!ProjectPointToScreen(p1, cam.viewMatrix, cam.projMatrix, 800, 600, s1)) continue;

                const float screenDist = std::hypot(s1[0] - s0[0], s1[1] - s0[1]);
                if (screenDist < 10.0f) continue;

                const float mx = (s0[0] + s1[0]) * 0.5f;
                const float my = (s0[1] + s1[1]) * 0.5f;

                float rOrig[3], rDir[3];
                check(ScreenToWorldRay(mx, my, cam, rOrig, rDir), "ScreenToWorldRay on local arrow drag");

                float hitPt[3]{}, s = 0.0f;
                check(ClosestPointOnAxis(rOrig, rDir, p0, localAxes[a], hitPt, &s), "ClosestPointOnAxis for local axis");
                check(s > 0.05f, "Local axis drag gives positive displacement along local axis");
            }
        }

        // Test plane drags (XY, XZ, YZ) in isometric view
        const auto& isoCam = cameras[2];
        const float pPlanes[3][3] = {{0,0,1}, {0,1,0}, {1,0,0}}; // normals for XY, XZ, YZ
        const float pAxes1[3][3] = {{1,0,0}, {1,0,0}, {0,1,0}};
        const float pAxes2[3][3] = {{0,1,0}, {0,0,1}, {0,0,1}};

        for (int p = 0; p < 3; ++p)
        {
            float pCorner[3] = {pAxes1[p][0] + pAxes2[p][0], pAxes1[p][1] + pAxes2[p][1], pAxes1[p][2] + pAxes2[p][2]};
            float sCorner[2]{};
            if (ProjectPointToScreen(pCorner, isoCam.viewMatrix, isoCam.projMatrix, 800, 600, sCorner))
            {
                float rO[3], rD[3];
                check(ScreenToWorldRay(sCorner[0], sCorner[1], isoCam, rO, rD), "ScreenToWorldRay on plane corner");

                float hit[3]{};
                float p0[3] = {0,0,0};
                check(RayPlaneIntersection(rO, rD, p0, pPlanes[p], hit), "RayPlaneIntersection succeeds");

                const float s1 = hit[0]*pAxes1[p][0] + hit[1]*pAxes1[p][1] + hit[2]*pAxes1[p][2];
                const float s2 = hit[0]*pAxes2[p][0] + hit[1]*pAxes2[p][1] + hit[2]*pAxes2[p][2];
                const float sn = hit[0]*pPlanes[p][0] + hit[1]*pPlanes[p][1] + hit[2]*pPlanes[p][2];

                check(s1 > 0.1f && s2 > 0.1f, "Plane drag gives positive motion along both axes");
                check(std::abs(sn) < 1e-4f, "Plane drag has zero drift along normal");
            }
        }
    }
}
