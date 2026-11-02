#pragma once

#include <string>
#include <vector>
#include <Engine/Render/MeshData.h>

/**
 * What the engine expects from a unit model. The defaults match the contract in
 * Engine/docs/UNIT-RENDERING.md: ground vehicle, metres, pivot on the ground, nose towards +Z.
 */
struct MeshValidationSettings
{
    float MinLengthMeters = 1.0f;
    float MaxLengthMeters = 25.0f;
    int MaxTriangles = 5000;
    // How far the lowest point may sit from y = 0.
    float PivotGroundTolerance = 0.15f;
    // How far the horizontal centre may sit from the origin, as a fraction of the model size.
    float PivotCenterTolerance = 0.15f;
    // Beyond this the UV is not tiling any more, it is broken.
    float MaxTexCoord = 16.0f;
    bool bExpectNoseAlongZ = true;
};

/**
 * Problems stop the model from working; warnings are things the engine will live with but a
 * designer usually did not mean. Both carry the measured value, so the fix is obvious without
 * opening the file again.
 */
struct MeshValidationReport
{
    std::vector<std::string> Problems;
    std::vector<std::string> Warnings;

    MeshBounds Bounds;
    int Triangles = 0;
    int Vertices = 0;
    int DegenerateTriangles = 0;
    int MissingNormals = 0;
    bool bHasTexCoords = false;

    bool IsUsable() const { return Problems.empty(); }
    /** One block of text per model, ready for the log. */
    std::string ToText(const std::string& modelName) const;
};

MeshValidationReport ValidateMesh(const MeshLodData& mesh,
                                  const MeshValidationSettings& settings = MeshValidationSettings{});
