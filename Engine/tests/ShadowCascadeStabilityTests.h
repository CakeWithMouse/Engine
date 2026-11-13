#pragma once

#include <Engine/Render/ShadowCascades.h>

#include <glm/glm.hpp>

#include <array>
#include <cmath>
#include <stdexcept>

namespace ShadowCascadeStabilityTests
{
    inline void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    /**
     * Full camera-yaw sweep for one perspective slice. The XY light projection extent must
     * remain bit-identical because its sphere radius depends only on fixed camera parameters.
     * Tiny angular steps also cross many texel-grid boundaries: only snapped center may step,
     * never projection scale. Every frustum corner must remain inside the square projection.
     */
    inline void SphereFitProjectionStableUnderYaw()
    {
        constexpr float nearDepth = 100.0f;
        constexpr float farDepth = 200.0f;
        constexpr float tanHalfFovY = 0.57735026919f;
        constexpr float aspect = 16.0f / 9.0f;
        constexpr int mapSize = 2048;
        constexpr float angularStep = 0.001f; // about 6284 samples across 2*pi
        constexpr float epsilon = 0.002f;

        const glm::vec3 lightForward = glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f));
        const glm::vec3 lightRight = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), lightForward));
        const glm::vec3 lightUp = glm::cross(lightForward, lightRight);

        const float radius = CascadeFrustumSphereRadius(nearDepth, farDepth, tanHalfFovY, aspect);
        const float expectedExtent = (2.0f * radius) *
            static_cast<float>(mapSize) / static_cast<float>(mapSize - 2);
        Require(radius > 0.0f && std::isfinite(radius), "Sphere fit returned an invalid radius");

        const float farHalfHeight = farDepth * tanHalfFovY;
        const float farHalfWidth = farHalfHeight * aspect;
        const float halfDepth = (farDepth - nearDepth) * 0.5f;
        const float farCornerDistance = std::sqrt(
            halfDepth * halfDepth + farHalfHeight * farHalfHeight + farHalfWidth * farHalfWidth);
        Require(std::abs(radius - farCornerDistance) <= epsilon,
                "Analytical radius does not reach the far frustum corners");

        ShadowCascadeProjectionBounds previous{};
        glm::vec3 previousCenter(0.0f);
        int gridBoundarySteps = 0;
        const int sweepSteps = static_cast<int>(std::ceil(6.28318530718f / angularStep));
        for (int step = 0; step <= sweepSteps; ++step)
        {
            const float yaw = static_cast<float>(step) * angularStep;
            const glm::vec3 cameraForward(std::sin(yaw), 0.0f, std::cos(yaw));
            const glm::vec3 cameraRight(std::cos(yaw), 0.0f, -std::sin(yaw));
            const glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
            const float centerDepth = (nearDepth + farDepth) * 0.5f;
            const glm::vec3 center = cameraForward * centerDepth;
            const float centerLightX = glm::dot(center, lightRight);
            const float centerLightY = glm::dot(center, lightUp);
            const auto bounds = FitShadowCascadeProjectionBounds(
                centerLightX, centerLightY, nearDepth, farDepth, tanHalfFovY, aspect, mapSize);

            Require(bounds.SphereRadius == radius && bounds.X.Extent == expectedExtent &&
                    bounds.Y.Extent == expectedExtent,
                    "Yaw changed the analytical sphere-fit projection extent");
            Require(std::abs((bounds.X.Max - bounds.X.Min) - bounds.X.Extent) <= epsilon &&
                    std::abs((bounds.Y.Max - bounds.Y.Min) - bounds.Y.Extent) <= epsilon,
                    "Grid-aligned bounds do not preserve their reported extent");

            for (const float depth : std::array<float, 2>{nearDepth, farDepth})
            {
                const float halfHeight = depth * tanHalfFovY;
                const float halfWidth = halfHeight * aspect;
                const glm::vec3 sliceCenter = cameraForward * depth;
                const std::array<glm::vec3, 4> offsets = {{
                    -cameraRight * halfWidth + cameraUp * halfHeight,
                    cameraRight * halfWidth + cameraUp * halfHeight,
                    -cameraRight * halfWidth - cameraUp * halfHeight,
                    cameraRight * halfWidth - cameraUp * halfHeight
                }};
                for (const glm::vec3& offset : offsets)
                {
                    const glm::vec3 corner = sliceCenter + offset;
                    const float x = glm::dot(corner, lightRight);
                    const float y = glm::dot(corner, lightUp);
                    Require(x >= bounds.X.Min - epsilon && x <= bounds.X.Max + epsilon &&
                            y >= bounds.Y.Min - epsilon && y <= bounds.Y.Max + epsilon,
                            "Sphere-fit projection clipped a frustum corner");
                }
            }

            if (step > 0)
            {
                const float previousXCell = std::floor(
                    (glm::dot(previousCenter, lightRight) - radius) / previous.X.TexelSize);
                const float currentXCell = std::floor((centerLightX - radius) / bounds.X.TexelSize);
                const float previousYCell = std::floor(
                    (glm::dot(previousCenter, lightUp) - radius) / previous.Y.TexelSize);
                const float currentYCell = std::floor((centerLightY - radius) / bounds.Y.TexelSize);
                if (previousXCell != currentXCell || previousYCell != currentYCell)
                {
                    ++gridBoundarySteps;
                    Require(bounds.X.Extent == previous.X.Extent && bounds.Y.Extent == previous.Y.Extent,
                            "Texel-grid boundary changed projection scale");
                }
            }
            previous = bounds;
            previousCenter = center;
        }
        Require(gridBoundarySteps > 100, "Yaw sweep did not exercise enough texel-grid boundaries");

        // Explicitly sample just either side of one texel boundary with a tiny center change.
        const float texel = SnapSphereFitAxis(0.0f, radius, mapSize).TexelSize;
        const float boundaryCenter = radius + 137.0f * texel;
        const float edgeOffset = 0.01f * texel;
        const auto justBefore = SnapSphereFitAxis(boundaryCenter - edgeOffset, radius, mapSize);
        const auto justAfter = SnapSphereFitAxis(boundaryCenter + edgeOffset, radius, mapSize);
        Require(justBefore.Extent == justAfter.Extent,
                "An explicit sub-texel boundary step changed projection scale");
        Require(std::abs((justAfter.Min - justBefore.Min) - texel) <= 0.02f * texel,
                "The light-space grid did not move by exactly one texel at its boundary");
    }

    /** Small depth changes hold scale; exceeding coverage grows immediately; large zoom/FOV
     * changes intentionally resize the projection. This is value hysteresis, not time inertia. */
    inline void AdaptiveSphereRadiusHysteresisPreservesCoverage()
    {
        constexpr float tanHalfFovY = 0.57735026919f;
        constexpr float aspect = 16.0f / 9.0f;
        constexpr int mapSize = 2048;

        const float initialRequired = CascadeFrustumSphereRadius(100.0f, 200.0f, tanHalfFovY, aspect);
        float stableRadius = StabilizeShadowCascadeRadius(initialRequired, 0.0f);
        Require(stableRadius >= initialRequired * 1.1f,
                "Initial stable radius lacks its 10 percent coverage reserve");

        for (int i = 1; i <= 100; ++i)
        {
            const float nearDepth = 100.0f + static_cast<float>(i) * 0.001f;
            const float farDepth = 200.0f + static_cast<float>(i) * 0.001f;
            const float required = CascadeFrustumSphereRadius(nearDepth, farDepth, tanHalfFovY, aspect);
            const float next = StabilizeShadowCascadeRadius(required, stableRadius);
            Require(next == stableRadius && next >= required,
                    "Small adaptive split motion changed or outgrew the stable radius");
        }

        const float coverageExceeded = stableRadius * 1.05f;
        const float grownRadius = StabilizeShadowCascadeRadius(coverageExceeded, stableRadius);
        Require(grownRadius > stableRadius && grownRadius >= coverageExceeded * 1.1f,
                "Radius did not grow immediately when current coverage was exceeded");
        const auto grownBounds = FitShadowCascadeProjectionBoundsForRadius(3.0f, -7.0f, grownRadius, mapSize);
        Require(grownBounds.X.Min <= 3.0f - coverageExceeded &&
                grownBounds.X.Max >= 3.0f + coverageExceeded &&
                grownBounds.Y.Min <= -7.0f - coverageExceeded &&
                grownBounds.Y.Max >= -7.0f + coverageExceeded,
                "Grown projection does not contain the newly required sphere");

        const float atShrinkThreshold = StabilizeShadowCascadeRadius(stableRadius * 0.75f, stableRadius);
        const float withinHysteresis = StabilizeShadowCascadeRadius(stableRadius * 0.80f, stableRadius);
        Require(atShrinkThreshold == stableRadius && withinHysteresis == stableRadius,
                "Radius shrank before the 25 percent threshold");

        const float largeShrinkRequired = stableRadius * 0.70f;
        const float shrunkRadius = StabilizeShadowCascadeRadius(largeShrinkRequired, stableRadius);
        Require(shrunkRadius < stableRadius && shrunkRadius >= largeShrinkRequired * 1.1f,
                "Large zoom-out did not shrink while retaining coverage reserve");

        const float narrowFovRequired = CascadeFrustumSphereRadius(
            100.0f, 200.0f, tanHalfFovY * 0.5f, aspect);
        const float narrowFovRadius = StabilizeShadowCascadeRadius(narrowFovRequired, grownRadius);
        Require(narrowFovRadius != grownRadius && narrowFovRadius >= narrowFovRequired,
                "A large FOV change did not intentionally resize the projection");
    }
}
