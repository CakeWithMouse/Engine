#define NOMINMAX
#include <Windows.h>
#include <Engine/Runtime/GameEditModule.h>
#include <Engine/Base/Base.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/Scene/SceneAssets.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/SkeletalLoader.h>
#include <Engine/Resources/Paths.h>
#include "../../Source/Resources/VfsRead.h"
#include "../../ThirdParty/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
using Json = nlohmann::json;
int checks = 0;
void RunAssetRenderTests(HWND, const fs::path&, const fs::path&, const std::string&, const std::string&);
void RunModelImportTests(const EngineEditSessionApi&, const fs::path&, const fs::path&);
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); ++checks; }
void Write(const fs::path& path, const std::string& bytes)
{ std::ofstream out(path, std::ios::binary); out.write(bytes.data(), bytes.size()); if (!out) throw std::runtime_error("Write fixture"); }
void Bitmap(const fs::path& path)
{
    std::string bytes(58, '\0'); bytes[0] = 'B'; bytes[1] = 'M'; bytes[2] = 58;
    bytes[10] = 54; bytes[14] = 40; bytes[18] = 1; bytes[22] = 1; bytes[26] = 1; bytes[28] = 24;
    bytes[34] = 4; bytes[54] = 20; bytes[55] = char(220); bytes[56] = 40;
    Write(path, bytes);
}
struct Object { uint64_t id; std::string name, model, texture, material; };
std::vector<Object> Objects(const EngineEditSessionApi& api)
{
    std::vector<Object> out; EngineSceneInfo info{};
    Check(api.readActiveScene(api.context, &info, [](void* p, const EngineSceneObjectInfo* o) {
        static_cast<std::vector<Object>*>(p)->push_back({o->id, o->name, o->modelAsset, o->textureAsset, o->materialAsset});
    }, &out) == EngineSceneOk, "read asset snapshot");
    return out;
}
struct MaterialView
{
    std::string shader, errors;
    EngineMaterialRenderState render{};
    std::vector<std::pair<std::string, std::vector<float>>> parameters; // value, then default
    std::vector<std::pair<std::string, std::string>> textures;
    bool modified = false;
};
MaterialView ReadMaterial(const EngineEditSessionApi& api, const char* material)
{
    MaterialView view; char error[1024]{};
    Check(api.readMaterial(api.context, material, [](void* p, const EngineMaterialInfo* info) {
        auto& v = *static_cast<MaterialView*>(p);
        v.shader = info->shader; v.errors = info->errors; v.render = info->render; v.modified = info->modified != 0;
        for (uint32_t i = 0; i < info->parameterCount; ++i)
        {
            const auto& parameter = info->parameters[i];
            std::vector<float> values(parameter.value, parameter.value + 4);
            values.insert(values.end(), parameter.defaultValue, parameter.defaultValue + 4);
            v.parameters.emplace_back(parameter.name, values);
        }
        for (uint32_t i = 0; i < info->textureCount; ++i) v.textures.emplace_back(info->textures[i].name, info->textures[i].path);
    }, &view, error, sizeof(error)) == EngineSceneOk, "read material");
    return view;
}
void CheckMaterialEditing(const EngineEditSessionApi& api, const fs::path& content)
{
    const char* surface = "cbuffer MaterialParams : register(b1) { float4 Tint = float4(1, 0, 0, 1); float Gloss = 0.5f; };\n"
                          "Texture2D BaseMap : register(t6);\n"
                          "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = Tint.rgb * BaseMap.Sample(MaterialSampler, i.UV).rgb; o.SpecularWeight = Gloss; }\n";
    Write(content / "Materials/Tinted.hlsl", surface);
    Write(content / "Materials/Tinted.mat", R"({ "shader": "Tinted.hlsl", "editor": { "note": "kept" } })");
    const char* material = "/Game/Materials/Tinted.mat";
    char error[1024]{};

    auto view = ReadMaterial(api, material);
    Check(view.shader == "/Game/Materials/Tinted.hlsl" && view.errors.empty() && !view.modified, "material description");
    Check(view.parameters.size() == 2 && view.parameters[0].first == "Tint" && view.parameters[0].second[4] == 1 &&
        view.parameters[1].second[0] == 0.5f, "parameters with their defaults");
    Check(view.textures.size() == 1 && view.textures[0].first == "BaseMap" && view.textures[0].second.empty(), "texture slots");

    const float green[4] = {0, 1, 0, 1};
    Check(api.setMaterialParameter(api.context, material, "Tint", green) == EngineSceneOk, "set parameter");
    Check(api.setMaterialParameter(api.context, material, "Missing", green) == EngineSceneNotFound, "unknown parameter");
    view = ReadMaterial(api, material);
    Check(view.parameters[0].second[1] == 1 && view.parameters[0].second[0] == 0 && view.modified, "edited value, not yet saved");
    EngineMaterialRenderState render{EngineMaterialMasked, 0.25f, 0, 1, 1};
    Check(api.setMaterialRenderState(api.context, material, &render) == EngineSceneOk, "set render state");
    render.blend = 7;
    Check(api.setMaterialRenderState(api.context, material, &render) == EngineSceneInvalidArgument, "reject unknown blend");
    Check(ReadMaterial(api, material).render.blend == EngineMaterialMasked, "render state reported");
    Check(api.saveMaterial(api.context, material, error, sizeof(error)) == EngineSceneOk && !ReadMaterial(api, material).modified, "save material");
    std::ifstream savedFile(content / "Materials/Tinted.mat");
    const auto saved = Json::parse(savedFile);
    Check(saved.at("render").at("blend") == "masked" && saved.at("params").at("Tint")[1] == 1.0 &&
        saved.at("editor").at("note") == "kept", "saved file keeps unknown fields");

    const float blue[4] = {0, 0, 1, 1};
    Check(api.setMaterialParameter(api.context, material, "Tint", blue) == EngineSceneOk, "edit after save");
    Check(api.revertMaterial(api.context, material, error, sizeof(error)) == EngineSceneOk, "revert material");
    view = ReadMaterial(api, material);
    Check(view.parameters[0].second[1] == 1 && view.parameters[0].second[2] == 0 && !view.modified, "revert restores the saved values");

    // The editor saves the surface, then asks to recompile: errors name the surface file.
    Write(content / "Materials/Tinted.hlsl", "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = Tnt; }\n");
    Check(api.reloadShaderFile(api.context, (content / "Materials/Tinted.hlsl").u8string().c_str()) == EngineSceneOk, "reload shader file");
    view = ReadMaterial(api, material);
    Check(view.errors.find("Tinted.hlsl(1,") != std::string::npos && view.errors.find("Tnt") != std::string::npos, "compile errors reported");
    Write(content / "Materials/Tinted.hlsl", surface);
    api.reloadShaderFile(api.context, (content / "Materials/Tinted.hlsl").u8string().c_str());
    Check(ReadMaterial(api, material).errors.empty(), "fixed surface compiles again");

    // A preview is a scene of its own in another window.
    HWND window = CreateWindowExW(0, L"STATIC", L"Material preview", WS_OVERLAPPEDWINDOW, 0, 0, 256, 256, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(window != nullptr, "create preview HWND");
    uint64_t preview = 0;
    Check(api.attachMaterialPreview(api.context, window, 256, 256, &preview) == EngineSceneOk && preview, "attach preview");
    Check(api.showMaterialPreview(api.context, preview, material, EnginePreviewCube, error, sizeof(error)) == EngineSceneOk, "show material");
    Check(api.showMaterialPreview(api.context, preview, "/Game/Materials/Missing.mat", EnginePreviewCube, error, sizeof(error)) != EngineSceneOk, "missing material");
    Check(api.materialPreviewCamera(api.context, preview, 0.3f, 0.1f, 1) == EngineSceneOk &&
        api.resizeMaterialPreview(api.context, preview, 128, 128) == EngineSceneOk && api.frame(api.context, 0.016) == EngineSceneOk, "render preview");
    Check(api.detachMaterialPreview(api.context, preview) == EngineSceneOk &&
        api.detachMaterialPreview(api.context, preview) == EngineSceneNotFound, "detach preview");
    DestroyWindow(window);
}
void Gltf(const fs::path& folder)
{
    const float vertices[] = {-1,-1,0, 1,-1,0, 0,1,0};
    const uint16_t indices[] = {0,1,2};
    std::string bytes(reinterpret_cast<const char*>(vertices), sizeof(vertices));
    bytes.append(reinterpret_cast<const char*>(indices), sizeof(indices));
    Write(folder / "mesh data.bin", bytes);
    Json json = {
        {"asset", {{"version","2.0"}}}, {"scene",0}, {"scenes", {{{"nodes", {0,1}}}}},
        {"nodes", {{{"mesh",0}}, {{"mesh",0},{"translation",{3,0,0}}}}},
        {"buffers", {{{"uri","mesh%20data.bin"},{"byteLength",bytes.size()}}}},
        {"bufferViews", {{{"buffer",0},{"byteOffset",0},{"byteLength",36}}, {{"buffer",0},{"byteOffset",36},{"byteLength",6}}}},
        {"accessors", {{{"bufferView",0},{"componentType",5126},{"count",3},{"type","VEC3"},{"min",{-1,-1,0}},{"max",{1,1,0}}},
            {{"bufferView",1},{"componentType",5123},{"count",3},{"type","SCALAR"}}}},
        {"meshes", {{{"primitives", {{{"attributes",{{"POSITION",0}}},{"indices",1}}}}}}}
    };
    Write(folder / "two-nodes.gltf", json.dump());
}
int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 5, "AssetTests <Game.dll> <EngineContent> <work> <CesiumMan.glb>");
        const auto root = fs::absolute(argv[3]); fs::create_directories(root / "Content"); fs::create_directories(root / "Sources");
        const auto content = root / "Content", sources = root / "Sources";
        Bitmap(sources / L"цвет.bmp"); Write(sources / "bad.png", "not an image"); Gltf(sources);
        const auto saved = root / "assets.scene.json";
        HWND hwnd = CreateWindowExW(0,L"STATIC",L"Asset test",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Check(hwnd != nullptr, "create viewport HWND");
        struct Window { HWND h; ~Window() { DestroyWindow(h); } } window{hwnd};
        std::string importedTexture, importedModel;
        {
            GameEditModule module(fs::absolute(argv[1]), fs::absolute(argv[2]), content);
            const auto api = module.Api(); char error[2048]{}, path[4096]{};
            auto import = [&](const fs::path& file, unsigned kind) {
                const auto status = api.importAsset(api.context, file.u8string().c_str(), kind, path, sizeof(path), error, sizeof(error));
                if (status) std::cerr << error << '\n';
                return status;
            };
            Check(import(sources / L"цвет.bmp", 1) == EngineSceneOk, "import Unicode texture"); importedTexture = path;
            Check(import(sources / L"цвет.bmp", 1) == EngineSceneOk && importedTexture != path, "duplicate filename never overwrites");
            Check(import(sources / "bad.png", 1) != EngineSceneOk, "reject corrupt image");
            Check(!fs::exists(content / "Textures/bad.png"), "bad texture leaves no asset");
            Write(sources / "empty.fbx", "");
            Check(import(sources / "empty.fbx", 0) != EngineSceneOk && std::string(error).find("empty (0 bytes)") != std::string::npos,
                "empty model has a specific diagnostic");
            Write(sources / "large.fbx", "x");
            fs::resize_file(sources / "large.fbx", 256ull * 1024 * 1024 + 1);
            Check(import(sources / "large.fbx", 0) != EngineSceneOk && std::string(error).find("268435457 bytes") != std::string::npos,
                "oversized model reports actual size");
            Check(!fs::exists(content / "Models/empty") && !fs::exists(content / "Models/large"), "size rejection leaves no imported asset");
            fs::remove(sources / "large.fbx");
            uint64_t cube = 0;
            Check(api.scene.createObject(api.scene.context,"Primitive.Cube","Textured cube",nullptr,&cube) == EngineSceneOk, "create cube");
            Check(api.setTexture(api.context,cube,importedTexture.c_str(),error,sizeof(error)) == EngineSceneOk, "assign texture before GPU exists");
            Check(api.setTexture(api.context,cube,"/Game/../bad.png",error,sizeof(error)) != EngineSceneOk, "reject traversal");
            Check(api.setTexture(api.context,cube,"/Game/missing.png",error,sizeof(error)) != EngineSceneOk, "reject missing texture");
            Check(Objects(api).front().texture == importedTexture, "failed assignment retains texture");
            Check(api.attachViewport(api.context,hwnd,640,480) == EngineSceneOk && api.frame(api.context,0) == EngineSceneOk, "draw texture assigned before attach");
            Check(api.setTexture(api.context,cube,"",error,sizeof(error)) == EngineSceneOk && Objects(api).front().texture.empty(), "clear texture");
            Check(api.setTexture(api.context,cube,importedTexture.c_str(),error,sizeof(error)) == EngineSceneOk, "assign texture with GPU");
            unsigned threaded = 0;
            std::thread wrong([&] { threaded = api.setTexture(api.context,cube,"",nullptr,0); }); wrong.join();
            Check(threaded == EngineSceneWrongThread, "asset mutation rejects wrong thread");
            // Materials: a .mat file inside Content, assigned to a primitive and kept by the scene document.
            fs::create_directories(content / "Materials");
            Write(content / "Materials/Red.hlsl", "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = float3(1, 0, 0); }\n");
            Write(content / "Materials/Red.mat", R"({ "shader": "Red.hlsl", "render": { "lighting": "unlit" } })");
            Write(content / "Materials/Broken.mat", "{ not json");
            const std::string material = "/Game/Materials/Red.mat";
            Check(api.setMaterial(api.context,cube,material.c_str(),error,sizeof(error)) == EngineSceneOk, "assign material");
            Check(api.setMaterial(api.context,cube,"/Game/Materials/Broken.mat",error,sizeof(error)) != EngineSceneOk &&
                std::string(error).find("Broken.mat") != std::string::npos, "invalid material is rejected with the reason");
            Check(api.setMaterial(api.context,cube,importedTexture.c_str(),error,sizeof(error)) != EngineSceneOk, "only .mat files are materials");
            Check(api.setMaterial(api.context,cube,"/Game/Materials/Missing.mat",error,sizeof(error)) != EngineSceneOk, "reject missing material");
            Check(Objects(api).front().material == material, "failed assignment keeps the material");
            Check(api.frame(api.context,0) == EngineSceneOk, "draw the material");
            CheckMaterialEditing(api, content);
            RunModelImportTests(api, content, sources);
#if defined(ENGINE_ENABLE_ASSIMP)
            for (const auto& o : Objects(api))
                if (o.name == "FBX after source deleted")
                    Check(api.setMaterial(api.context,o.id,"",error,sizeof(error)) == EngineSceneOk, "explicitly clear auto material");
            Check(import(sources / "two-nodes.gltf",0) == EngineSceneOk, "import glTF with external buffer"); importedModel = path;
            const auto physical = content / fs::u8path(importedModel.substr(6));
            Check(fs::exists(physical.parent_path() / "mesh data.bin"), "glTF dependency copied");
            uint64_t model = 0;
            Check(api.createModel(api.context,path,"Imported model",nullptr,&model,error,sizeof(error)) == EngineSceneOk && model, "place imported model");
            Check(api.setTexture(api.context,model,importedTexture.c_str(),error,sizeof(error)) == EngineSceneOk, "texture model");
            Check(api.frame(api.context,0) == EngineSceneOk, "draw imported model");
            uint64_t rejected = 999;
            Check(api.createModel(api.context,path,"Imported model",nullptr,&rejected,error,sizeof(error)) == EngineSceneDuplicateName && !rejected, "duplicate model name rejected");
            Check(import(fs::absolute(argv[4]),0) == EngineSceneOk, "import skeletal GLB");
            Check(api.createModel(api.context,path,"Animated model",nullptr,&model,error,sizeof(error)) == EngineSceneOk, "place skeletal GLB");
            Check(api.setTexture(api.context,model,importedTexture.c_str(),error,sizeof(error)) == EngineSceneOk, "texture skeletal GLB");
            Write(sources / "triangle.obj", "v -1 -1 0\nv 1 -1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0.5 1\nf 1/1 2/2 3/3\n");
            Check(import(sources / "triangle.obj",0) == EngineSceneOk, "import OBJ geometry and UV");
            auto bad = Json::parse(std::ifstream(sources / "two-nodes.gltf"));
            bad["buffers"][0]["uri"] = "absent.bin"; Write(sources / "missing.gltf", bad.dump());
            Check(import(sources / "missing.gltf",0) != EngineSceneOk && !fs::exists(content / "Models/missing"), "missing glTF dependency rolls back import");
            bad["buffers"][0]["uri"] = "../mesh.bin"; Write(sources / "escape.gltf", bad.dump());
            Check(import(sources / "escape.gltf",0) != EngineSceneOk && !fs::exists(content / "Models/escape"), "glTF dependency cannot escape source folder");
#else
            Check(import(sources / "two-nodes.gltf",0) != EngineSceneOk && !fs::exists(content / "Models/two-nodes"), "disabled importer reports failure and rolls back");
#endif
            Check(api.saveScene(api.context,saved.u8string().c_str(),error,sizeof(error)) == EngineSceneOk, "save asset references");
            const auto json = SceneDocument::Read(saved);
            Check(json.components.front().textureAsset == importedTexture, "serialized texture path");
            Check(json.components.front().materialAsset == material, "serialized material path");
#if defined(ENGINE_ENABLE_ASSIMP)
            for (const auto& o : json.components)
            {
                if (o.name == "FBX external") Check(!o.materialAsset.empty(), "automatic material saved with the scene");
                if (o.name == "FBX after source deleted") Check(o.materialAsset.empty(), "explicitly cleared default saved as empty");
            }
#endif
            Check(api.control(api.context,EnginePlay) == EngineSceneOk, "play clone retains assets");
            Check(Objects(api).front().texture == importedTexture, "play snapshot texture");
            Check(Objects(api).front().material == material, "play snapshot material");
            Check(api.frame(api.context,0.05) == EngineSceneOk, "animate and render Play");
            Check(api.setTexture(api.context,cube,"",error,sizeof(error)) == EngineSceneReadOnly, "cannot mutate assets in Play");
            Check(import(sources / L"цвет.bmp",1) == EngineSceneReadOnly, "cannot import during Play");
            Check(api.control(api.context,EngineStop) == EngineSceneOk, "stop returns to edit scene");
            uint64_t scene = 0;
#if defined(ENGINE_ENABLE_ASSIMP)
            const auto importMetadata = content / "Models/auto-external/auto-external.fbx.import.json";
            std::ifstream metadataFile(importMetadata);
            const std::string originalMetadata{std::istreambuf_iterator<char>(metadataFile),std::istreambuf_iterator<char>()};
            metadataFile.close();
            Write(importMetadata,"{ broken import metadata");
#endif
            Check(api.loadScene(api.context,saved.u8string().c_str(),&scene,error,sizeof(error)) == EngineSceneOk, "reopen textured scene");
#if defined(ENGINE_ENABLE_ASSIMP)
            for (const auto& o : Objects(api))
            {
                if (o.name == "FBX external") Check(!o.material.empty(), "restoring saved material does not depend on import metadata");
                if (o.name == "FBX after source deleted") Check(o.material.empty(), "scene restore does not reapply a cleared automatic material");
            }
            Write(importMetadata,originalMetadata);
#endif
            Check(Objects(api).front().texture == importedTexture, "reopened texture retained");
            Check(Objects(api).front().material == material, "reopened material retained");
            const auto texture = content / fs::u8path(importedTexture.substr(6)); fs::rename(texture, texture.wstring() + L".backup");
            Check(api.loadScene(api.context,saved.u8string().c_str(),&scene,error,sizeof(error)) != EngineSceneOk, "missing asset rejects scene load");
            Check(Objects(api).front().texture == importedTexture, "failed load preserves current scene");
            fs::rename(texture.wstring() + L".backup", texture);
            // A broken material does not stop the scene: the object keeps the reference and draws the error checker.
            const auto redMat = content / "Materials/Red.mat";
            Write(redMat, "{ not json");
            Check(api.loadScene(api.context,saved.u8string().c_str(),&scene,error,sizeof(error)) == EngineSceneOk, "broken material keeps the scene loadable");
            Check(Objects(api).front().material == material && api.frame(api.context,0) == EngineSceneOk, "broken material reference kept and drawn");
            fs::remove(redMat);
            Check(api.loadScene(api.context,saved.u8string().c_str(),&scene,error,sizeof(error)) == EngineSceneOk, "missing material keeps the scene loadable");
            Write(redMat, R"({ "shader": "Red.hlsl", "render": { "lighting": "unlit" } })");
            Check(api.reloadShaderFile(api.context,"") == EngineSceneOk && api.frame(api.context,0) == EngineSceneOk, "full reload picks up the restored material");
            Check(api.reloadShaderFile(api.context,nullptr) == EngineSceneInvalidArgument, "reload requires a path or an empty string");
        }
        const auto moved = root.wstring() + L"-moved"; fs::rename(root, moved);
        {
            GameEditModule module(fs::absolute(argv[1]), fs::absolute(argv[2]), fs::path(moved) / "Content");
            auto api = module.Api(); char error[1024]{}; uint64_t scene = 0;
            Check(api.loadScene(api.context,(fs::path(moved) / "assets.scene.json").u8string().c_str(),&scene,error,sizeof(error)) == EngineSceneOk, "project remains portable after move");
            Check(Objects(api).front().texture == importedTexture, "moved project resolves same virtual path");
        }
        Check(!SceneAssets::ValidPath("C:/outside.png") && !SceneAssets::ValidPath("/Game/a/../b") &&
            !SceneAssets::ValidPath("/Game//b") && SceneAssets::ValidPath(importedTexture), "portable path validation");
        RunAssetRenderTests(hwnd,fs::absolute(argv[2]),fs::path(moved) / "Content",importedTexture,importedModel);
        BaseEngine runtime;
        RuntimeLaunchOptions options;
        options.module = fs::absolute(argv[1]); options.projectName = "Imported scene standalone";
        options.contentRoot = (fs::path(moved) / "Content").u8string();
        options.startupScene = fs::path(moved) / "assets.scene.json"; options.frames = 2;
        Check(runtime.Run(options) == 0, "standalone runtime loads and renders saved imported scene");
        std::cout << "Asset workflow: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "Asset workflow failed: " << e.what() << '\n'; return 1; }
}
