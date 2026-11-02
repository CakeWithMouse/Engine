#include <Engine/Render/MeshValidation.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
    std::string Number(float value, int digits = 2)
    {
        std::ostringstream stream;
        stream.precision(digits);
        stream << std::fixed << value;
        return stream.str();
    }

    float TriangleArea(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, const DirectX::XMFLOAT4& c)
    {
        const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        const float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        const float cx = uy * vz - uz * vy;
        const float cy = uz * vx - ux * vz;
        const float cz = ux * vy - uy * vx;
        return 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);
    }
}

MeshValidationReport ValidateMesh(const MeshLodData& mesh, const MeshValidationSettings& settings)
{
    MeshValidationReport report;
    report.Vertices = static_cast<int>(mesh.Vertices.size());
    report.Triangles = static_cast<int>(mesh.Indices.size() / 3);

    if (mesh.Vertices.empty() || mesh.Indices.empty())
    {
        report.Problems.push_back("Mesh is empty: no vertices or no indices.");
        return report;
    }
    if (mesh.Indices.size() % 3 != 0)
    {
        report.Problems.push_back("Index count " + std::to_string(mesh.Indices.size()) +
                                  " is not a whole number of triangles; export as triangles.");
        return report;
    }

    for (uint32_t index : mesh.Indices)
    {
        if (index >= mesh.Vertices.size())
        {
            report.Problems.push_back("An index points past the end of the vertex array; the file is broken.");
            return report;
        }
    }

    report.Bounds = ComputeMeshBounds(mesh);
    const float sizeX = report.Bounds.Max.x - report.Bounds.Min.x;
    const float sizeY = report.Bounds.Max.y - report.Bounds.Min.y;
    const float sizeZ = report.Bounds.Max.z - report.Bounds.Min.z;
    const float longest = std::max(sizeX, std::max(sizeY, sizeZ));

    // Scale: one unit is one metre, so a vehicle that comes in centimetres or inches is obvious.
    if (longest < settings.MinLengthMeters || longest > settings.MaxLengthMeters)
    {
        report.Problems.push_back(
            "Size " + Number(sizeX) + " x " + Number(sizeY) + " x " + Number(sizeZ) +
            " m is outside the expected " + Number(settings.MinLengthMeters, 0) + "-" +
            Number(settings.MaxLengthMeters, 0) + " m. One unit must be one metre" +
            (longest > settings.MaxLengthMeters ? " (exported in centimetres?)." : " (exported in kilometres?)."));
    }

    // Pivot: on the ground by Y, in the middle by X and Z.
    if (std::abs(report.Bounds.Min.y) > settings.PivotGroundTolerance)
    {
        report.Problems.push_back(
            "Pivot is not on the ground: the lowest point is at y = " + Number(report.Bounds.Min.y) +
            " m. Move the model so that its tracks or wheels touch y = 0.");
    }

    const float horizontalSize = std::max(sizeX, sizeZ);
    const float centerTolerance = std::max(0.05f, horizontalSize * settings.PivotCenterTolerance);
    if (std::abs(report.Bounds.Center.x) > centerTolerance || std::abs(report.Bounds.Center.z) > centerTolerance)
    {
        report.Problems.push_back(
            "Pivot is off centre: the middle of the model is at x = " + Number(report.Bounds.Center.x) +
            ", z = " + Number(report.Bounds.Center.z) +
            " m. The engine turns instances around the origin, so the model would swing around it.");
    }

    // Orientation: the long axis of a ground vehicle is its nose, and the nose points at +Z.
    if (settings.bExpectNoseAlongZ && sizeX > sizeZ * 1.2f)
    {
        report.Warnings.push_back(
            "The model is longer across (x = " + Number(sizeX) + " m) than along (z = " + Number(sizeZ) +
            " m). The nose must point towards +Z; this one looks rotated by 90 degrees.");
    }
    if (sizeY > horizontalSize * 1.5f)
    {
        report.Warnings.push_back(
            "The model is taller (" + Number(sizeY) + " m) than it is wide or long. Up must be +Y.");
    }

    if (report.Triangles > settings.MaxTriangles)
    {
        report.Warnings.push_back(
            std::to_string(report.Triangles) + " triangles against a budget of " +
            std::to_string(settings.MaxTriangles) + " for the most detailed level.");
    }

    // Geometry quality: zero-area triangles and normals that are missing or not unit length.
    const float areaEpsilon = std::max(1e-7f, longest * longest * 1e-8f);
    for (size_t triangle = 0; triangle + 2 < mesh.Indices.size(); triangle += 3)
    {
        const float area = TriangleArea(mesh.Vertices[mesh.Indices[triangle]].Position,
                                        mesh.Vertices[mesh.Indices[triangle + 1]].Position,
                                        mesh.Vertices[mesh.Indices[triangle + 2]].Position);
        if (area <= areaEpsilon)
        {
            ++report.DegenerateTriangles;
        }
    }
    if (report.DegenerateTriangles > 0)
    {
        report.Warnings.push_back(
            std::to_string(report.DegenerateTriangles) + " triangles have no area. They cost vertices and "
            "draw nothing; they also confuse the LOD builder.");
    }

    float maxTexCoord = 0.0f;
    bool anyTexCoord = false;
    for (const MeshVertex& vertex : mesh.Vertices)
    {
        const float length = std::sqrt(vertex.Normal.x * vertex.Normal.x +
                                       vertex.Normal.y * vertex.Normal.y +
                                       vertex.Normal.z * vertex.Normal.z);
        if (length < 0.5f || length > 1.5f)
        {
            ++report.MissingNormals;
        }
        maxTexCoord = std::max(maxTexCoord, std::max(std::abs(vertex.TexCoord.x), std::abs(vertex.TexCoord.y)));
        anyTexCoord = anyTexCoord || vertex.TexCoord.x != 0.0f || vertex.TexCoord.y != 0.0f;
    }
    report.bHasTexCoords = anyTexCoord;

    if (report.MissingNormals > 0)
    {
        report.Problems.push_back(
            std::to_string(report.MissingNormals) + " vertices have no usable normal (zero or not unit length). "
            "Export normals or let the importer generate them.");
    }
    if (!anyTexCoord)
    {
        report.Warnings.push_back("No UV set: the model will be drawn in its team colour, without a texture.");
    }
    else if (maxTexCoord > settings.MaxTexCoord)
    {
        report.Warnings.push_back(
            "UV reaches " + Number(maxTexCoord, 1) + ", far past the 0-1 range. Tiling is fine, this is not.");
    }

    return report;
}

std::string MeshValidationReport::ToText(const std::string& modelName) const
{
    std::ostringstream text;
    const float sizeX = Bounds.Max.x - Bounds.Min.x;
    const float sizeY = Bounds.Max.y - Bounds.Min.y;
    const float sizeZ = Bounds.Max.z - Bounds.Min.z;

    text << "asset-check " << modelName << ": " << Triangles << " triangles, " << Vertices << " vertices, "
         << "size " << Number(sizeX) << " x " << Number(sizeY) << " x " << Number(sizeZ) << " m, "
         << "pivot y " << Number(Bounds.Min.y) << ", uv " << (bHasTexCoords ? "yes" : "no") << ", "
         << (Problems.empty() ? (Warnings.empty() ? "ok" : "ok with warnings") : "NOT USABLE");

    for (const std::string& problem : Problems)
    {
        text << "\n  problem: " << problem;
    }
    for (const std::string& warning : Warnings)
    {
        text << "\n  warning: " << warning;
    }
    return text.str();
}
