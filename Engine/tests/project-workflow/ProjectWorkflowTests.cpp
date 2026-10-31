#define NOMINMAX
#include <Windows.h>
#include <Engine/Runtime/GameEditModule.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Resources/Paths.h>
#include "../../../Game/Launcher/Source/ProjectLibrary.h"
#include "../../../Game/SceneProject/Source/Primitives.h"
#include <iostream>
#include <limits>
#include <thread>
#include <fstream>

static int checks = 0;
static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
    ++checks;
}
static EngineSceneObjectInfo ReadLight(const EngineEditSessionApi& api, const char* name)
{
    struct Query { const char* name; EngineSceneObjectInfo result{}; } query{name};
    EngineSceneInfo info{};
    Check(api.readActiveScene(api.context, &info, [](void* p, const EngineSceneObjectInfo* o) {
        auto& q = *static_cast<Query*>(p);
        if (std::string(o->name) == q.name) { q.result = *o; q.result.name = q.result.typeName = q.result.modelAsset = q.result.textureAsset = nullptr; }
    }, &query) == EngineSceneOk, "read light snapshot");
    Check(query.result.id && query.result.hasPointLight, "light exists in active scene");
    return query.result;
}
class LightCaptureGame : public Game
{
public:
    uint64_t channels[3]{};
    std::vector<uint8_t> pixels;
    unsigned captureWidth=0,captureHeight=0;
    void SaveCapture(const std::filesystem::path& path) {
        BITMAPFILEHEADER file{}; file.bfType=0x4d42; file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER); file.bfSize=file.bfOffBits+static_cast<DWORD>(pixels.size());
        BITMAPINFOHEADER info{}; info.biSize=sizeof(info); info.biWidth=captureWidth;info.biHeight=-static_cast<LONG>(captureHeight);info.biPlanes=1;info.biBitCount=32;
        auto bgra=pixels;for(size_t i=0;i<bgra.size();i+=4)std::swap(bgra[i],bgra[i+2]);
        std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(bgra.data()),bgra.size());
    }
    void Draw() override
    {
        Game::Draw();
        RenderEditorSelection();
        D3D11_TEXTURE2D_DESC desc{}; BackTexture->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        Check(SUCCEEDED(Device->CreateTexture2D(&desc, nullptr, &staging)), "create light readback");
        Context->CopyResource(staging.Get(), BackTexture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(SUCCEEDED(Context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "map light readback");
        for (auto& channel : channels) channel = 0;
        captureWidth=desc.Width;captureHeight=desc.Height;pixels.resize(desc.Width*desc.Height*4);
        for (unsigned y = 0; y < desc.Height; ++y)
            for (unsigned x = 0; x < desc.Width; ++x)
            {
                const auto* pixel = static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch + x * 4;
                std::copy(pixel,pixel+4,pixels.begin()+(y*desc.Width+x)*4);
                for (int c = 0; c < 3; ++c) channels[c] += pixel[c];
            }
        Context->Unmap(staging.Get(), 0);
    }
};
static void CheckLightPixels(HWND window, const std::filesystem::path& content)
{
    EnginePaths::SetContentRoot(content);
    LightCaptureGame game;
    game.SetRenderingType(Custom); game.SetSimulationEnabled(false); game.SetVSync(false);
    Check(game.InitializeViewport(window,640,480,true), "attach light pixel viewport");
    game.GetPlayer()->SetPosition({0,0,5});
    auto cube = SceneProject::GetComponentTypes().Find("Primitive.Cube")->create(); cube->SetColor({1,1,1,1});
    Check(game.GetScene().AddComponent("cube", cube, "Primitive.Cube").result == SceneResult::Ok, "add lit cube");
    auto light = SceneProject::GetComponentTypes().Find("Light.Point")->create();
    const auto added = game.GetScene().AddComponent("light", light, "Light.Point");
    Check(added.result == SceneResult::Ok, "add pixel light");
    SceneTransform t; t.position = {0,0,2};
    Check(game.GetScene().SetTransform(added.id, t) == SceneResult::Ok, "position pixel light");
    SceneComponentSnapshot::PointLight value{{0,1,0},4,10,true};
    Check(game.GetScene().SetPointLight(added.id, value) == SceneResult::Ok && game.RenderViewportFrame(0), "render green light");
    const auto green = game.channels[1];
    value.color = {1,0,0};
    Check(game.GetScene().SetPointLight(added.id, value) == SceneResult::Ok && game.RenderViewportFrame(0), "render red light");
    Check(green > game.channels[1] + 10000, "light color changes actual D3D pixels");
    value.color = {0,1,0}; value.enabled = false;
    Check(game.GetScene().SetPointLight(added.id, value) == SceneResult::Ok && game.RenderViewportFrame(0), "disable pixel light");
    const auto unlit = game.channels[1];
    Check(green > unlit + 10000, "disabling light removes illumination");
    value.enabled = true;
    Check(game.GetScene().SetPointLight(added.id, value) == SceneResult::Ok, "enable pixel light");
    t.position = {100,0,2};
    Check(game.GetScene().SetTransform(added.id, t) == SceneResult::Ok && game.RenderViewportFrame(0), "move light outside radius");
    Check(game.channels[1] == unlit, "moving light updates actual illumination");
    const auto cubeId=game.GetScene().FindComponentByName("cube")->GetComponentId();
    const auto beforeSelection=game.pixels;
    game.SetEditorSelection(cubeId);Check(game.RenderViewportFrame(0),"draw selection");
    Check(game.pixels!=beforeSelection,"selection changes actual D3D pixels");
    game.SetEditorSelection(0);game.RenderViewportFrame(0);
    Check(game.pixels==beforeSelection,"deselect removes overlay without changing material");
    const auto catalog=SceneProject::GetComponentTypes();
    auto sky=catalog.Find("Environment.Sky")->create();
    Check(sky->IsSkybox()&&!sky->CastsShadow(),"sky metadata");
    float distance=10000;Check(!sky->RaycastPrimitive({0,0,5},{0,0,-1},distance),"sky does not intercept picking");
    game.GetScene().AddComponent("Sky",sky,"Environment.Sky");
    game.RenderViewportFrame(0);Check(game.pixels!=beforeSelection,"sky renders without texture assets");
    const auto skyPixels=game.pixels;
    for(const auto* type:{"Particle.Sparks","Particle.Smoke"}) {
        auto effect=catalog.Find(type)->create();effect->SetPosition({type==std::string("Particle.Sparks")?-1.2f:1.2f,0,0});
        game.GetScene().AddComponent(type,effect,type);
    }
    for(int i=0;i<45;++i) {
        for(const auto& item:game.GetScene().GetComponents())if(auto* p=dynamic_cast<ParticleSystemComponent*>(item.second.get()))p->Tick(0.05f);
        game.RenderViewportFrame(0);
    }
    Check(game.pixels!=skyPixels,"particle presets produce visible pixels");
    const auto paused=game.pixels;game.RenderViewportFrame(0);Check(game.pixels==paused,"particles stop without simulation steps");
    game.SetEditorSelection(cubeId);game.RenderViewportFrame(0);
    game.SaveCapture(std::filesystem::temp_directory_path()/"engine-effects-selection.bmp");
}
static void CheckShadowPixels(HWND window)
{
    LightCaptureGame game;
    game.SetRenderingType(Custom); game.SetSimulationEnabled(false); game.SetVSync(false);
    game.SetSceneLightingEnabled(true);
    Check(game.InitializeViewport(window, 640, 480, true), "attach shadow pixel viewport");
    game.GetPlayer()->SetPosition({0, 3, 6});
    game.GetPlayer()->Rotate(0, -0.4f);
    auto cube = SceneProject::GetComponentTypes().Find("Primitive.Cube")->create();
    cube->SetPosition({0, 0.5f, 0});
    Check(game.GetScene().AddComponent("caster", cube, "Primitive.Cube").result == SceneResult::Ok, "add shadow caster");
    auto floor = SceneProject::GetComponentTypes().Find("Primitive.Plane")->create();
    floor->SetScale({10, 1, 10});
    Check(game.GetScene().AddComponent("floor", floor, "Primitive.Plane").result == SceneResult::Ok, "add shadow receiver");
    Check(game.RenderViewportFrame(0), "render scene without a sun");
    const auto dark = game.pixels;
    const auto center = (240 * 640 + 320) * 4;
    Check(dark[center] == 0 && dark[center+1] == 0 && dark[center+2] == 0,
        "ordinary geometry has no hidden ambient or default sun");
    auto sun = SceneProject::GetComponentTypes().Find("Light.Directional")->create();
    auto* light = static_cast<DirectionalLightComponent*>(sun.get());
    auto settings = ProjectLibrary::Json::parse(light->SaveProjectData());
    settings["shadows"] = false; light->LoadProjectData(settings.dump());
    const auto added = game.GetScene().AddComponent("sun", sun, "Light.Directional");
    Check(added.result == SceneResult::Ok, "add explicit directional light");
    Check(game.RenderViewportFrame(0), "render without shadows");
    const auto before = game.pixels;
    Check(before != dark, "adding sun lights geometry");
    settings["shadows"] = true; light->LoadProjectData(settings.dump());
    Check(game.RenderViewportFrame(0) && game.AreShadowsActive(), "render with shadows");
    size_t darkened = 0;
    for (size_t i = 0; i < before.size(); i += 4)
        if (int(before[i]) + before[i+1] + before[i+2] > int(game.pixels[i]) + game.pixels[i+1] + game.pixels[i+2] + 30)
            ++darkened;
    Check(darkened > 100, "cube casts a visible shadow onto the floor");
    game.SaveCapture(std::filesystem::temp_directory_path() / "engine-editor-shadows.bmp");
    const auto lit = game.pixels;
    light->SetPosition({100, 200, -50}); light->SetScale({2, 3, 4});
    Check(game.RenderViewportFrame(0) && game.pixels == lit, "sun position and scale do not affect light");
    light->SetScale({-2, 3, 4});
    Check(game.RenderViewportFrame(0) && game.pixels == lit, "negative sun scale does not reverse lighting");
    light->SetRotation({-55, 65, 0});
    Check(game.RenderViewportFrame(0) && game.pixels != lit, "sun rotation changes illumination and shadows");
    settings["enabled"] = false; light->LoadProjectData(settings.dump());
    Check(game.RenderViewportFrame(0) && game.pixels == dark && !game.AreShadowsActive(), "disabled sun leaves no light or shadows");
    settings["enabled"] = true; settings["color"] = {0,1,0}; light->LoadProjectData(settings.dump());
    Check(game.RenderViewportFrame(0) && game.pixels != dark, "sun color editable at runtime");
    Check(game.GetFrameConstants().DirectionalLightColorIntensity.x == 0 && game.GetFrameConstants().DirectionalLightColorIntensity.y == 1, "authored color reaches GPU constants");
    Check(game.GetScene().RemoveComponent(added.id) == SceneResult::Ok, "delete sun");
    Check(game.RenderViewportFrame(0) && game.pixels == dark && !game.AreShadowsActive(), "deleted sun leaves no residual lighting");
}

static void CheckDirectionalAuthoring(const std::filesystem::path& modulePath,
    const std::filesystem::path& content, const std::filesystem::path& root)
{
    GameEditModule module(modulePath, content);
    const auto& api = module.Api();
    const auto& authoring = module.AuthoringApi();
    const auto& projectData = module.ProjectDataApi();
    uint64_t id = 0, second = 123; char error[1024]{};
    Check(api.scene.createObject(api.scene.context, "Light.Directional", "Sun", nullptr, &id) == EngineSceneOk, "create singleton sun via DLL");
    Check(api.scene.createObject(api.scene.context, "Light.Directional", "Second sun", nullptr, &second) == EngineSceneInvalidArgument && !second, "second sun rejected");
    auto read = [&](uint64_t object) {
        std::string data;
        Check(projectData.read(projectData.context, object, [](void* p, const char* value) { *static_cast<std::string*>(p) = value; }, &data) == EngineSceneOk, "read sun settings");
        return ProjectLibrary::Json::parse(data);
    };
    auto set = [&](const char* path, const char* value) {
        return authoring.setProperty(authoring.context, id, path, value, error, sizeof(error));
    };
    Check(set("/intensity", "2.5") == EngineSceneOk && set("/color", "[0.2,0.6,1]") == EngineSceneOk &&
        set("/shadowDistance", "350") == EngineSceneOk && set("/shadows", "false") == EngineSceneOk, "edit sun through inspector API");
    const auto authored = read(id);
    Check(set("/color", "[2,0,0]") != EngineSceneOk && set("/intensity", "-1") != EngineSceneOk &&
        set("/shadowDistance", "1") != EngineSceneOk && read(id) == authored, "invalid sun settings preserve previous values");
    const auto& batch = module.BatchApi(); uint64_t output = 0;
    Check(batch.apply(batch.context, EngineBatchDuplicate, &id, 1, id, nullptr, &output, error, sizeof(error)) != EngineSceneOk &&
        read(id) == authored, "batch duplicate cannot create second sun");
    const auto file = (root / "sun.scene.json").u8string();
    Check(api.saveScene(api.context, file.c_str(), error, sizeof(error)) == EngineSceneOk, "save sun scene");
    const auto saved = SceneDocument::Read(std::filesystem::u8path(file));
    Check(saved.documentVersion == 3 && saved.components.size() == 1, "sun persists in scene document");
    Check(api.control(api.context, EnginePlay) == EngineSceneOk && read(id) == authored, "Play copies sun settings");
    Check(set("/intensity", "1") == EngineSceneReadOnly, "sun inspector read-only during Play");
    Check(api.control(api.context, EngineStop) == EngineSceneOk, "Stop restores authored sun");
    Check(api.scene.removeObject(api.scene.context, id) == EngineSceneOk, "delete singleton");
    uint64_t scene = 0;
    Check(api.loadScene(api.context, file.c_str(), &scene, error, sizeof(error)) == EngineSceneOk && read(id) == authored, "reopen preserves sun properties");
    auto invalid = saved; auto copy = invalid.components.front(); copy.id = 999; copy.name = "Second sun";
    invalid.components.push_back(copy);
    bool rejected = false;
    try { SceneDocument::Validate(invalid, SceneProject::GetComponentTypes()); } catch (const SceneDocumentError&) { rejected = true; }
    Check(rejected, "document rejects multiple suns before restoration");
    Scene pending;
    auto first = SceneProject::GetComponentTypes().Find("Light.Directional")->create();
    auto another = SceneProject::GetComponentTypes().Find("Light.Directional")->create();
    {
        auto scope = pending.BeginUpdate();
        Check(pending.AddComponent("first", first).result == SceneResult::Deferred && !first, "deferred sun owns singleton slot");
        Check(pending.AddComponent("another", another).result == SceneResult::DuplicateSingleton && another, "pending duplicate retains ownership");
    }
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 4, "usage: ProjectWorkflowTests <template Game.dll> <EngineContent> <new test directory>");
        const auto root = std::filesystem::absolute(argv[3]);
        const auto file = ProjectLibrary::Create(root, "Проверка проекта Unicode", std::filesystem::absolute(argv[1]));
        auto project = ProjectLibrary::Validate(file);
        Check(project.name == "Проверка проекта Unicode", "UTF-8 project name");
        Check(project.content == root / "Content" && project.module == root / "Game.dll", "isolated project roots");
        Check(SceneDocument::Read(project.startupScene).components.size() == 4, "starter floor, cube and two lights");
        bool rejected = false;
        try { ProjectLibrary::Create(root, "Overwrite", argv[1]); } catch (...) { rejected = true; }
        Check(rejected && ProjectLibrary::Validate(file).name == project.name, "existing project never overwritten");
        rejected = false;
        try { ProjectLibrary::Create(root / "bad", "  ", argv[1]); } catch (...) { rejected = true; }
        Check(rejected && !std::filesystem::exists(root / "bad"), "invalid creation leaves no project");

        HWND window = CreateWindowExW(0, L"STATIC", L"Scene project test", WS_OVERLAPPEDWINDOW,
            0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(window != nullptr, "test viewport");
        struct WindowGuard { HWND window; ~WindowGuard() { DestroyWindow(window); } } guard{window};
        {
            GameEditModule module(project.module, std::filesystem::absolute(argv[2]));
            const auto& api = module.Api(); char error[1024]{}; uint64_t scene = 0;
            Check(module.SelectionApi().select!=nullptr,"selection extension available");
            Check(api.loadScene(api.context, project.startupScene.u8string().c_str(), &scene, error, sizeof(error)) == EngineSceneOk, "load starter scene");
            std::vector<std::string> types;
            Check(api.scene.listTypes(api.scene.context, [](void* p, const EngineSceneTypeInfo* t) {
                static_cast<std::vector<std::string>*>(p)->push_back(t->typeName);
            }, &types) == EngineSceneOk && types.size() == 14, "primitives, lights, particles and sky");
            Check(api.attachViewport(api.context, window, 640, 480) == EngineSceneOk, "attach real D3D viewport");
            Check(api.frame(api.context, 0.016) == EngineSceneOk, "initial frame");
            uint64_t picked = 0;
            Check(api.pickObject(api.context, 0, 0, &picked) == EngineSceneOk && picked, "pick starter cube at viewport center");
            Check(api.focusObject(api.context, picked) == EngineSceneOk, "focus selected object");
            Check(api.pickObject(api.context, 2, 0, &picked) == EngineSceneInvalidArgument && !picked, "reject invalid viewport coordinate");
            int index = 0; uint64_t first = 0;
            for (const auto& type : types)
            {
                EngineSceneTransform transform{{static_cast<float>(index++),1,0},{0,0.38268343f,0,0.92387953f},{1,2,1}};
                uint64_t id = 0;
                Check(api.scene.createObject(api.scene.context, type.c_str(), type.c_str(), &transform, &id) == EngineSceneOk && id, "create primitive after attach");
                if (!first) first = id;
                Check(api.frame(api.context, 0.016) == EngineSceneOk, "render newly added primitive");
            }
            EngineSceneTransform changed{{-2,3,-1},{0,0,0,1},{2,3,4}};
            Check(api.scene.setTransform(api.scene.context, first, &changed) == EngineSceneOk, "edit transform");
            Check(api.scene.renameObject(api.scene.context, first, "Изменённый куб") == EngineSceneOk, "rename object");
            uint64_t duplicate = 0;
            Check(api.scene.createObject(api.scene.context, "Primitive.Cube", "Изменённый куб", nullptr, &duplicate) == EngineSceneDuplicateName && !duplicate, "duplicate name rejected");
            Check(api.scene.createObject(api.scene.context, "Primitive.Cube", "Temporary", nullptr, &duplicate) == EngineSceneOk, "duplicate object");
            Check(api.scene.removeObject(api.scene.context, duplicate) == EngineSceneOk, "delete object");
            const auto light = ReadLight(api, "Light.Point");
            EngineScenePointLight authored{{0.1f,0.8f,0.25f},3.5f,12.0f,1};
            Check(api.scene.setPointLight(api.scene.context, light.id, &authored) == EngineSceneOk, "edit light color and falloff");
            Check(api.scene.setTransform(api.scene.context, light.id, &changed) == EngineSceneOk, "move light");
            Check(api.scene.setPointLight(api.scene.context, first, &authored) == EngineSceneInvalidArgument, "light properties rejected on cube");
            Check(api.scene.setPointLight(api.scene.context, UINT64_MAX, &authored) == EngineSceneNotFound, "unknown light ID rejected");
            Check(api.scene.setPointLight(api.scene.context, light.id, nullptr) == EngineSceneInvalidArgument, "null light properties rejected");
            for (int i = 0; i < 6; ++i)
            {
                auto bad = authored;
                if (i == 0) bad.color[0] = -1;
                if (i == 1) bad.color[0] = 2;
                if (i == 2) bad.intensity = -1;
                if (i == 3) bad.radius = 0;
                if (i == 4) bad.radius = std::numeric_limits<float>::quiet_NaN();
                if (i == 5) bad.enabled = 2;
                Check(api.scene.setPointLight(api.scene.context, light.id, &bad) == EngineSceneInvalidArgument, "invalid light parameters rejected");
            }
            uint32_t threadResult = 0;
            std::thread other([&] { threadResult = api.scene.setPointLight(api.scene.context, light.id, &authored); }); other.join();
            Check(threadResult == EngineSceneWrongThread, "light mutations enforce engine thread");
            const auto edited = ReadLight(api, "Light.Point");
            Check(edited.pointLight.color[1] == 0.8f && edited.pointLight.intensity == 3.5f && edited.pointLight.radius == 12 &&
                edited.transform.position[0] == -2, "invalid mutations preserve authored light");
            Check(api.saveScene(api.context, project.startupScene.u8string().c_str(), error, sizeof(error)) == EngineSceneOk, "save authored scene");
            const auto saved = SceneDocument::Read(project.startupScene);
            Check(saved.components.size() == 18, "all objects persisted including gameplay types");
            for (const auto& o : saved.components) if (o.name == "Light.Point")
                Check(o.pointLight && o.pointLight->color.y == 0.8f && o.pointLight->radius == 12, "JSON persists light properties");
            Check(api.control(api.context, EnginePlay) == EngineSceneOk, "play authored scene");
            const auto playingLight = ReadLight(api, "Light.Point");
            Check(playingLight.pointLight.color[1] == 0.8f && playingLight.transform.position[0] == -2, "Play copy preserves light");
            Check(api.scene.setPointLight(api.scene.context, light.id, &authored) == EngineSceneReadOnly, "light edits locked in Play");
            Check(api.frame(api.context, 0.016) == EngineSceneOk, "render play copy");
            Check(api.scene.removeObject(api.scene.context, first) == EngineSceneReadOnly, "edit locked in play");
            Check(api.control(api.context, EnginePause) == EngineSceneOk, "pause");
            Check(api.scene.setPointLight(api.scene.context, light.id, &authored) == EngineSceneReadOnly, "light edits locked in Pause");
            Check(api.control(api.context, EngineResume) == EngineSceneOk, "resume");
            Check(api.control(api.context, EngineStop) == EngineSceneOk, "stop");
            Check(api.loadScene(api.context, project.startupScene.u8string().c_str(), &scene, error, sizeof(error)) == EngineSceneOk, "reopen saved scene");
            Check(api.frame(api.context, 0.016) == EngineSceneOk, "render reopened scene");
            const auto invalid = root / "invalid.scene.json";
            ProjectLibrary::Write(invalid, {{"version",99},{"name","invalid"},{"objects",ProjectLibrary::Json::array()}});
            Check(api.loadScene(api.context, invalid.u8string().c_str(), &scene, error, sizeof(error)) == EngineSceneUnsupportedVersion, "reject invalid scene without replacing current scene");
            Check(api.frame(api.context, 0.016) == EngineSceneOk, "render preserved scene after failed load");
            const auto backup = root / "reopened.scene.json";
            Check(api.saveScene(api.context, backup.u8string().c_str(), error, sizeof(error)) == EngineSceneOk, "save reopened scene");
            auto reopened = SceneDocument::Read(backup);
            Check(reopened.components.size() == saved.components.size(), "round trip count");
            for (size_t i = 0; i < saved.components.size(); ++i)
            {
                const auto& a = saved.components[i]; const auto& b = reopened.components[i];
                Check(a.typeName == b.typeName && a.name == b.name && a.transform.position == b.transform.position &&
                    a.transform.scale == b.transform.scale && glm::length(a.transform.rotation - b.transform.rotation) < 1e-6f, "round trip authored properties");
                Check(a.pointLight.has_value() == b.pointLight.has_value(), "round trip light type");
                if (a.pointLight) Check(a.pointLight->color == b.pointLight->color && a.pointLight->intensity == b.pointLight->intensity &&
                    a.pointLight->radius == b.pointLight->radius && a.pointLight->enabled == b.pointLight->enabled, "round trip light parameters");
            }
            auto invalidLight = ProjectLibrary::Json::parse(SceneDocument::Serialize(saved));
            for (auto& o : invalidLight["objects"]) {
                if (o.contains("pointLight")) o["pointLight"]["radius"] = 0;
                if (o.contains("properties") && o["properties"].contains("Engine.PointLight"))
                    o["properties"]["Engine.PointLight"]["value"]["radius"] = 0;
            }
            ProjectLibrary::Write(invalid, invalidLight);
            Check(api.loadScene(api.context, invalid.u8string().c_str(), &scene, error, sizeof(error)) == EngineSceneInvalidDocument,
                "malformed light JSON rejected transactionally");
            Check(ReadLight(api, "Light.Point").pointLight.radius == 12, "failed light load preserves current scene");
            auto legacy = saved;
            for (auto& o : legacy.components) o.pointLight.reset();
            SceneDocument::Write(invalid, legacy);
            Check(api.loadScene(api.context, invalid.u8string().c_str(), &scene, error, sizeof(error)) == EngineSceneOk,
                "documents without light properties remain readable");
            Check(ReadLight(api, "Light.Point").pointLight.color[0] == 1, "absent properties use factory defaults");
        }
        CheckLightPixels(window, std::filesystem::absolute(argv[2]));
        CheckShadowPixels(window);
        CheckDirectionalAuthoring(project.module, std::filesystem::absolute(argv[2]), root);
        // The procedural catalog also produces drawable CPU meshes before any GPU initialization.
        for (const auto& type : {"Primitive.Cube", "Primitive.Sphere", "Primitive.Plane", "Primitive.Cylinder", "Primitive.Cone", "Primitive.Pyramid"})
        {
            const auto object = SceneProject::GetComponentTypes().Find(type)->create();
            Check(object->GetIndexCount() >= 6 && object->GetLocalBoundingRadius() > 0, "primitive geometry exists");
        }
        std::cout << "ProjectWorkflowTests: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "ProjectWorkflowTests: " << e.what() << '\n'; return 1; }
}
