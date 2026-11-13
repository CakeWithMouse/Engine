#pragma once

#include "ModelGeometryScaleTests.h"
#include "TestSkipped.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace ModelFlatNormalsTests
{

namespace fs = std::filesystem;
using ModelGeometryScaleTests::Check;
using ModelGeometryScaleTests::Load;
using ModelGeometryScaleTests::Near;
using ModelGeometryScaleTests::WriteText;

inline bool NormalEqual(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
    return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z);
}

inline size_t TotalVertices(const SkeletalModel& model)
{
    size_t total = 0;
    for (const auto& mesh : model.Meshes)
    {
        total += mesh.Vertices.size();
    }
    return total;
}

struct FaceStats
{
    size_t faces = 0;
    size_t flat = 0;
    std::vector<DirectX::XMFLOAT3> normals;
};

// A face is flat when all three corner normals agree; the per-face normal is kept for comparison.
inline FaceStats Faces(const SkeletalModel& model)
{
    FaceStats stats;
    for (const auto& mesh : model.Meshes)
    {
        for (size_t i = 0; i + 2 < mesh.Indices.size(); i += 3)
        {
            const auto& a = mesh.Vertices[mesh.Indices[i]].Normal;
            const auto& b = mesh.Vertices[mesh.Indices[i + 1]].Normal;
            const auto& c = mesh.Vertices[mesh.Indices[i + 2]].Normal;
            ++stats.faces;
            if (NormalEqual(a, b) && NormalEqual(a, c))
            {
                ++stats.flat;
            }
            stats.normals.push_back(a);
        }
    }
    return stats;
}

// P3 regression: a folded pair of triangles carrying smooth vn must split under "flat"
// (more vertices than the file, every face flat) while "smooth" keeps the crease unwelded
// at the default angle. The fixture lives in the repository, not in TEMP.
inline void Run()
{
#if !defined(ENGINE_ENABLE_ASSIMP)
    throw TestSkipped("flat normals checks require -EnableAssimp");
#else
    const auto fixture = fs::path(__FILE__).parent_path() / "fixtures" / "folded-smooth.obj";
    Check(fs::exists(fixture), "folded-smooth.obj fixture exists in Engine/tests/fixtures");

    std::ifstream input(fixture, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    Check(text.find("\nvn ") != std::string::npos, "fixture keeps its smooth vertex normals (vn)");

    size_t fileVertices = 0;
    size_t fileFaces = 0;
    std::istringstream lines(text);
    for (std::string line; std::getline(lines, line);)
    {
        if (line.rfind("v ", 0) == 0) ++fileVertices;
        else if (line.rfind("f ", 0) == 0) ++fileFaces;
    }
    Check(fileVertices == 4 && fileFaces == 2, "fixture is 4 positions and 2 faces");

    const auto stamp = std::to_string(static_cast<unsigned long long>(std::time(nullptr))) + "_" + std::to_string(std::clock());
    const auto tempRoot = fs::temp_directory_path() / ("FlatNormals_" + stamp);
    fs::create_directories(tempRoot);
    try
    {
        VfsRead::GetVFS().Mount("/FlatNormals", tempRoot);
        fs::copy_file(fixture, tempRoot / "folded.obj", fs::copy_options::overwrite_existing);
        const std::string vpath = "/FlatNormals/folded.obj";
        const auto sidecar = tempRoot / "folded.obj.importsettings.json";

        // "file": the smooth vn survive and weld the shared edge into 4 vertices.
        // scale is a required field of the model object; without it Parse fails and the loader
        // silently falls back to defaults, so every sidecar here carries scale:1.0.
        WriteText(sidecar, R"({"version":1,"kind":"model","model":{"scale":1.0,"normals":"file"}})");
        const auto fileMode = Load(vpath);
        Check(TotalVertices(*fileMode.Model) == fileVertices, "file mode keeps the 4 welded vertices");
        const auto fileFacesStats = Faces(*fileMode.Model);
        Check(fileFacesStats.faces == fileFaces, "file mode keeps both faces");
        Check(NormalEqual(fileFacesStats.normals[0], fileFacesStats.normals[1]),
              "file mode: both faces share the same smooth vn");

        // "flat": drop the vn, split the shared edge, every face carries its own geometric normal.
        WriteText(sidecar, R"({"version":1,"kind":"model","model":{"scale":1.0,"normals":"flat"}})");
        const auto flat = Load(vpath);
        Check(TotalVertices(*flat.Model) > fileVertices, "flat splits the shared edge: more vertices than the file");
        const auto flatStats = Faces(*flat.Model);
        Check(flatStats.faces == fileFaces, "flat keeps both faces");
        Check(flatStats.flat == flatStats.faces, "flat: 100% of faces are flat");
        Check(!NormalEqual(flatStats.normals[0], flatStats.normals[1]),
              "flat: the two faces keep distinct normals, the fold stays visible");

        // "flat" without scale field: scale is optional, must not fall back to defaults ("file")
        WriteText(sidecar, R"({"version":1,"kind":"model","model":{"normals":"flat"}})");
        const auto flatNoScale = Load(vpath);
        Check(TotalVertices(*flatNoScale.Model) > fileVertices, "flat without scale still splits shared edge");

        // "smooth" at the default 60 degrees: the 90-degree crease is above the angle, so it is not welded.
        WriteText(sidecar, R"({"version":1,"kind":"model","model":{"scale":1.0,"normals":"smooth"}})");
        const auto smooth = Load(vpath);
        Check(TotalVertices(*smooth.Model) > fileVertices, "smooth at default angle does not weld the crease");
        const auto smoothStats = Faces(*smooth.Model);
        Check(!NormalEqual(smoothStats.normals[0], smoothStats.normals[1]),
              "smooth at default angle keeps distinct normals across the crease");

        // "smooth" at 175 degrees: the crease is averaged away and the shared edge welds back to 4.
        WriteText(sidecar, R"({"version":1,"kind":"model","model":{"scale":1.0,"normals":"smooth","smoothingAngle":175.0}})");
        const auto wide = Load(vpath);
        Check(TotalVertices(*wide.Model) == fileVertices, "smooth at 175 welds the crease back to 4 vertices");
        const auto wideStats = Faces(*wide.Model);
        Check(wideStats.flat == 0, "smooth at 175: no face is flat, the corner normals differ");

        VfsRead::GetVFS().Unmount("/FlatNormals");
        fs::remove_all(tempRoot);
    }
    catch (...)
    {
        VfsRead::GetVFS().Unmount("/FlatNormals");
        fs::remove_all(tempRoot);
        throw;
    }
#endif
}

} // namespace ModelFlatNormalsTests