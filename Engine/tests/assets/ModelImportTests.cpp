#include <Engine/Runtime/EditSessionApi.h>
#include <Engine/Render/MaterialAsset.h>
#include <Engine/Scene/SceneDocument.h>
#include "../../ThirdParty/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using Json = nlohmann::json;
void Check(bool, const char*);
void Bitmap(const fs::path&);
void Write(const fs::path&, const std::string&);

#if defined(ENGINE_ENABLE_ASSIMP)
namespace
{
// Small, self-authored ASCII FBX fixtures; no DCC installation is needed by this suite.
void Fbx(const fs::path& path)
{
    fs::copy_file(fs::path(__FILE__).parent_path() / "fixtures" / path.filename(), path);
}

std::string Import(const EngineEditSessionApi& api, const fs::path& source)
{
    char path[4096]{}, error[2048]{};
    const auto status = api.importAsset(api.context, source.u8string().c_str(), 0, path, sizeof(path), error, sizeof(error));
    if (status) throw std::runtime_error(std::string("Model material import failed: ") + error);
    Check(status == EngineSceneOk, "import model and its material");
    return path;
}
std::string Place(const EngineEditSessionApi& api, const std::string& path, const std::string& name, const fs::path& content, bool texture)
{
    char error[2048]{}; uint64_t id = 0;
    Check(api.createModel(api.context, path.c_str(), name.c_str(), nullptr, &id, error, sizeof(error)) == EngineSceneOk, "place model with auto material");
    struct Result { uint64_t id; std::string material, texture; } result{id};
    EngineSceneInfo info{};
    Check(api.readActiveScene(api.context, &info, [](void* p, const EngineSceneObjectInfo* o) {
        auto& r = *static_cast<Result*>(p); if (r.id == o->id) { r.material = o->materialAsset; r.texture = o->textureAsset; }
    }, &result) == EngineSceneOk, "read imported model");
    Check(!result.material.empty() && result.texture.empty(), "material auto assigned without double multiplying the texture");
    MaterialAsset material; std::string problem;
    const auto materialPath = content / fs::u8path(result.material.substr(6));
    Check(MaterialAsset::Load(materialPath.u8string(), material, problem), "generated material is valid");
    Check(material.Shader == "Imported.hlsl" && material.Textures.size() == 1, "editable shader and BaseMap");
    if (texture)
    {
        Check(material.Textures.front().first == "BaseMap" && !material.Textures.front().second.empty(), "BaseMap contains imported texture");
        const auto image = MaterialAsset::ResolvePath(materialPath.u8string(), material.Textures.front().second);
        Check(fs::is_regular_file(fs::u8path(image)), "base texture copied/extracted");
        Check(fs::u8path(material.Textures.front().second).is_relative(), "texture remains project relative");
    }
    else Check(material.Textures.front().second.empty(), "untextured material uses a white BaseMap");
    std::string shaderErrors;
    Check(api.readMaterial(api.context, result.material.c_str(), [](void* p, const EngineMaterialInfo* m) {
        *static_cast<std::string*>(p) = m->errors;
    }, &shaderErrors, error, sizeof(error)) == EngineSceneOk && shaderErrors.empty(), "imported shader compiles and texture loads");
    return result.material;
}
}
#endif

void RunModelImportTests(const EngineEditSessionApi& api, const fs::path& content, const fs::path& sources)
{
#if defined(ENGINE_ENABLE_ASSIMP)
    fs::create_directories(sources / "maps");
    Bitmap(sources / "maps" / L"цвет.bmp");
    Fbx(sources / "auto-external.fbx");
    Fbx(sources / "auto-embedded.fbx");
    Fbx(sources / "auto-multiple.fbx");
    Fbx(sources / "auto-color.fbx");
    auto external = Import(api, sources / "auto-external.fbx");
    const auto firstMaterial = Place(api, external, "FBX external", content, true);
    const auto duplicate = Import(api, sources / "auto-external.fbx");
    Check(external != duplicate && fs::exists(content / "Models/auto-external_1/Materials/Material_0.mat"), "repeat model import is independent");
    Place(api, Import(api, sources / "auto-embedded.fbx"), "FBX embedded", content, true);
    Place(api, Import(api, sources / "auto-color.fbx"), "FBX color", content, false);
    const auto multiple = Import(api, sources / "auto-multiple.fbx");
    const auto manifestPath = content / fs::u8path(multiple.substr(6) + ".import.json");
    const auto manifest = Json::parse(std::ifstream(manifestPath));
    Check(manifest.at("materials").size() == 2 && manifest.contains("note"), "all used materials are retained with explicit one-material limitation");
    Check(std::distance(fs::directory_iterator(manifestPath.parent_path() / "Textures"), fs::directory_iterator{}) == 1, "shared image copied once");
    // A stale absolute author path is recovered from the standard FBX media folder.
    fs::create_directory(sources / "auto-fallback.fbm");
    Bitmap(sources / "auto-fallback.fbm/color.bmp");
    Fbx(sources / "auto-fallback.fbx");
    Place(api, Import(api, sources / "auto-fallback.fbx"), "FBX fallback", content, true);

    Fbx(sources / "auto-missing.fbx");
    Fbx(sources / "auto-corrupt.fbx"); Write(sources / "maps/corrupt.png", "bad image");
    char path[4096]{}, error[2048]{};
    for (const auto* name : {"auto-missing", "auto-corrupt"})
        Check(api.importAsset(api.context, (sources / (std::string(name)+".fbx")).u8string().c_str(), 0, path, sizeof(path), error, sizeof(error)) != EngineSceneOk &&
              !fs::exists(content / "Models" / name), "missing/corrupt linked image rolls back the complete import");
    Check(fs::exists(content / fs::u8path(firstMaterial.substr(6))), "failed import preserves previous assets");
    fs::remove(sources / "maps" / L"цвет.bmp");
    Place(api, external, "FBX after source deleted", content, true);

    // Optional real Blender integration fixtures generated by make-blend-fixtures.py.
    // The ordinary suite still tests FBX (external + embedded) without Blender installed.
    wchar_t fixtures[32768]{};
    if (GetEnvironmentVariableW(L"PAINENGINE_BLEND_FIXTURES", fixtures, 32768))
    {
        const auto folder = fs::path(fixtures);
        for (const auto* name : {"blend-external", "blend-packed"})
        {
            const auto blend = Import(api, folder / (std::string(name) + ".blend"));
            Check(fs::u8path(blend).extension() == ".glb", "BLEND converted to portable GLB");
            Place(api, blend, name, content, true);
        }
        for (const auto* name : {"blender-external", "blender-packed"})
            Place(api, Import(api, folder / (std::string(name) + ".fbx")), name, content, true);
        Check(api.importAsset(api.context, (folder / "blend-missing.blend").u8string().c_str(), 0, path, sizeof(path), error, sizeof(error)) != EngineSceneOk &&
              !fs::exists(content / "Models/blend-missing"), "missing BLEND image rolls back and reports Blender error");
    }
    else std::cout << "BLEND integration skipped (set PAINENGINE_BLEND_FIXTURES and PAINENGINE_BLENDER)\n";
#else
    (void)api; (void)content; (void)sources;
#endif
}
