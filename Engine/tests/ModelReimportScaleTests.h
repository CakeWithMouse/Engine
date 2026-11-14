#pragma once

#include "ModelGeometryScaleTests.h"
#include "TestSkipped.h"
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Scene/SceneAssets.h>
#include <Engine/Render/SkeletalLoader.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ModelReimportScaleTests
{

namespace fs = std::filesystem;
using ModelGeometryScaleTests::Box;
using ModelGeometryScaleTests::Check;
using ModelGeometryScaleTests::ModelBox;
using ModelGeometryScaleTests::Near;
using ModelGeometryScaleTests::Size;
using ModelGeometryScaleTests::WriteText;

struct Row
{
    uint64_t id = 0;
    std::string name;
    std::string model;
    float scale[3] = {0, 0, 0};
    float position[3] = {0, 0, 0};
};

inline std::vector<Row> Rows(const EngineEditSessionApi& api)
{
    std::vector<Row> rows;
    EngineSceneInfo info{};
    Check(api.readActiveScene(api.context, &info, [](void* p, const EngineSceneObjectInfo* o) {
        Row r;
        r.id = o->id;
        r.name = o->name;
        r.model = o->modelAsset;
        for (int k = 0; k < 3; ++k)
        {
            r.scale[k] = o->transform.scale[k];
            r.position[k] = o->transform.position[k];
        }
        static_cast<std::vector<Row>*>(p)->push_back(r);
    }, &rows) == EngineSceneOk, "readActiveScene succeeds");
    return rows;
}

inline std::string ReadText(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

inline void Run()
{
#if !defined(ENGINE_ENABLE_ASSIMP)
    throw TestSkipped("reimport scale checks require -EnableAssimp");
#else
    const auto stamp = std::to_string(static_cast<unsigned long long>(std::time(nullptr))) + "_" + std::to_string(std::clock());
    const auto root = fs::temp_directory_path() / ("ReimportScale_" + stamp);
    const auto content = root / "Content";
    fs::create_directories(content / "Scenes");
    try
    {
        {
            SceneEditSession session({nullptr, 0});
            auto api = session.GetApi();
            auto author = session.GetAuthoringApi();
            char error[2048]{};
            Check(api.setContentRoot(api.context, content.u8string().c_str(), error, sizeof(error)) == EngineSceneOk,
                  "setContentRoot mounts /Game");

            WriteText(root / "mesh.obj", "v 0 0 0\nv 10 0 0\nv 10 10 0\nv 0 10 0\nf 1 2 3\nf 1 3 4\n");
            const auto model = SceneAssets::Import(content, root / "mesh.obj", SceneAssets::Model);
            const auto modelFile = content / fs::u8path(model.substr(6));
            const auto sidecar = fs::u8path(modelFile.u8string() + ".importsettings.json");

            // Start at version 2 scale 1: geometry is the raw 10x10 quad.
            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":1.0}})");
            Check(author.asset(author.context, 0, model.c_str(), "", error, sizeof(error)) == EngineSceneOk,
                  "initial reimport succeeds");

            // Place an object with a non-unit Transform to prove reimport preserves it.
            EngineSceneTransform t{{3, 0, 0}, {0, 0, 0, 1}, {2, 2, 2}};
            uint64_t mesh = 0;
            Check(api.createModel(api.context, model.c_str(), "Mesh", &t, &mesh, error, sizeof(error)) == EngineSceneOk,
                  "createModel succeeds");
            const auto saved = content / "Scenes/Main.scene.json";
            Check(api.saveScene(api.context, saved.u8string().c_str(), error, sizeof(error)) == EngineSceneOk,
                  "saveScene succeeds");

            const Box beforeBox = ModelBox(*ModelGeometryScaleTests::Load(model).Model);
            Check(Near(Size(beforeBox, 0), 10.0f) && Near(Size(beforeBox, 1), 10.0f),
                  "geometry bbox is 10x10 at version 2 scale 1");
            bool found = false;
            for (const auto& row : Rows(api))
            {
                if (row.name != "Mesh") continue;
                Check(Near(row.scale[0], 2.0f) && Near(row.scale[1], 2.0f) && Near(row.scale[2], 2.0f),
                      "Transform scale is {2,2,2} before reimport");
                found = true;
            }
            Check(found, "Mesh object present before reimport");

            // Edit the sidecar to scale 0.01 and reimport from the recorded source.
            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":0.01}})");
            Check(author.asset(author.context, 0, model.c_str(), "", error, sizeof(error)) == EngineSceneOk,
                  "reimport after scale edit succeeds");

            const Box afterBox = ModelBox(*ModelGeometryScaleTests::Load(model).Model);
            Check(Near(Size(afterBox, 0), 0.1f) && Near(Size(afterBox, 1), 0.1f),
                  "geometry bbox shrinks 100x after reimport to scale 0.01");
            Check(Near(Size(afterBox, 0), Size(beforeBox, 0) * 0.01f, 1e-3f),
                  "after bbox equals before bbox times the new scale");

            found = false;
            for (const auto& row : Rows(api))
            {
                if (row.name != "Mesh") continue;
                Check(Near(row.scale[0], 2.0f) && Near(row.scale[1], 2.0f) && Near(row.scale[2], 2.0f),
                      "Transform scale preserved {2,2,2} after reimport");
                Check(Near(row.position[0], 3.0f), "Transform position preserved after reimport");
                Check(row.model == model, "model asset path unchanged after reimport");
                found = true;
            }
            Check(found, "Mesh object present after reimport");

            const auto sidecarText = ReadText(sidecar);
            Check(sidecarText.find("0.01") != std::string::npos, "edited sidecar scale survives reimport");
            Check(sidecarText.find("\"version\": 2") != std::string::npos || sidecarText.find("\"version\":2") != std::string::npos,
                  "sidecar stays version 2 after reimport");
        }
        fs::remove_all(root);
    }
    catch (...)
    {
        fs::remove_all(root);
        throw;
    }
#endif
}

} // namespace ModelReimportScaleTests