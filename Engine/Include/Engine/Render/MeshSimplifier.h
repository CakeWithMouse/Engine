#pragma once

#include <Engine/Render/MeshData.h>

/**
 * How far to take one generated level.
 *
 * The simplifier works on positions, so vertices are first welded by position: a model exported
 * with split normals has every edge as a seam, and without welding there is nothing to collapse.
 * Welding loses the authored normals, so they are rebuilt afterwards with a crease angle - flat
 * where the surface turns sharply, smooth where it does not.
 */
struct MeshSimplifySettings
{
    // Target triangle count as a fraction of the source.
    float TargetRatio = 0.25f;
    // Allowed deviation, relative to the size of the model. Reached before the ratio is.
    float TargetError = 0.05f;
    // Edges sharper than this keep their hard shading.
    float CreaseAngleDegrees = 50.0f;
    /**
     * When the exact simplifier cannot reach the target (a model full of seams and tiny parts),
     * fall back to the sloppy one, which ignores topology. It ruins close-up shapes, so it is
     * only worth it for the farthest levels.
     */
    bool bAllowSloppy = true;
    /**
     * How much of the silhouette the sloppy path may cost, as a fraction of the radius of the
     * model. Left to itself it eats thin parts whole - a gun barrel vanishes and the vehicle
     * shrinks - so it is run again with a tighter error until the silhouette holds. Raising this
     * buys fewer triangles at the far level, lowering it buys shape.
     */
    float MaxSilhouetteDrift = 0.08f;
};

/** What the simplifier did, so a failed target is visible instead of silent. */
struct MeshSimplifyResult
{
    int SourceTriangles = 0;
    int ResultTriangles = 0;
    int TargetTriangles = 0;
    // Deviation reported by the simplifier, relative to the size of the model.
    float Error = 0.0f;
    // How much the radius of the model changed: what a distant level is actually judged on.
    float SilhouetteDrift = 0.0f;
    bool bUsedSloppy = false;
};

/**
 * Builds a coarser version of the mesh. Returns an empty mesh if the source is unusable;
 * the caller decides whether that is fatal.
 */
MeshLodData SimplifyMesh(const MeshLodData& source,
                         const MeshSimplifySettings& settings,
                         MeshSimplifyResult* outResult = nullptr);
