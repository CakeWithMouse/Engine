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
}
