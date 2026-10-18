#define NOMINMAX
#include <Windows.h>
#include <Engine/Base/Base.h>
#include <Engine/Scene/SceneCommands.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
int checks = 0;
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
struct CopiedObject { uint64_t id; std::string name, type; EngineSceneTransform transform; };
struct Snapshot { EngineSceneInfo info{}; std::vector<CopiedObject> objects; };
void __cdecl CopyObject(void* target, const EngineSceneObjectInfo* item)
{
    // Like any ABI client, copying strings must not let exceptions escape.
    try { static_cast<Snapshot*>(target)->objects.push_back({item->id, item->name, item->typeName, item->transform}); }
    catch (...) { std::terminate(); }
}
Snapshot Read(const EngineSceneApi& api)
{
    Snapshot result; result.objects.reserve(16);
    Check(api.readScene(api.context, &result.info, CopyObject, &result) == EngineSceneOk, "read scene");
    Check(result.info.objectCount == result.objects.size(), "snapshot count");
    return result;
}
EngineSceneTransform Identity() { return {{0,0,0}, {0,0,0,1}, {1,1,1}}; }

int live = 0, constructed = 0;
struct Tracked : GameComponent
{
    Tracked() { ++live; ++constructed; }
    ~Tracked() override { --live; }
};
std::unique_ptr<GameComponent> Throwing() { throw std::runtime_error("Expected factory failure"); }
std::unique_ptr<GameComponent> NullFactory() { return {}; }
const ComponentType localTypes[] = {
    {"Tracked", "Tracked", &EngineComponentDetail::Create<Tracked>, {}},
    {"Throws", "Throws", Throwing, {}}, {"Null", "Null", NullFactory, {}}
};
void LocalTests()
{
    {
        Scene scene;
        SceneCommands commands(scene, {localTypes, 3});
        const auto api = commands.GetApi();
        uint64_t id = 42;
        Check(api.createObject(api.context, "Throws", "bad", nullptr, &id) == EngineSceneFailure && id == 0, "factory exception contained");
        Check(api.createObject(api.context, "Null", "bad", nullptr, &id) == EngineSceneFailure, "null factory contained");
        Check(scene.GetRevision() == 0 && Read(api).objects.empty(), "failed factories leave scene unchanged");
        Check(api.createObject(api.context, "Tracked", "one", nullptr, &id) == EngineSceneOk && live == 1, "tracked create");
        const auto first = id;
        const auto revision = scene.GetRevision();
        Check(api.createObject(api.context, "Tracked", "one", nullptr, &id) == EngineSceneDuplicateName && constructed == 1, "duplicate checked before constructor");
        auto invalid = Identity(); invalid.scale[1] = 0;
        Check(api.createObject(api.context, "Tracked", "bad", &invalid, &id) == EngineSceneInvalidArgument && constructed == 1, "invalid transform before constructor");
        invalid = Identity(); invalid.position[0] = std::numeric_limits<float>::quiet_NaN();
        Check(api.setTransform(api.context, first, &invalid) == EngineSceneInvalidArgument, "NaN rejected");
        Check(scene.GetRevision() == revision, "rejections preserve revision");
        {
            auto scope = scene.BeginUpdate();
            Check(api.removeObject(api.context, first) == EngineSceneBusy, "no command during update");
            Check(api.createObject(api.context, "Tracked", "busy", nullptr, &id) == EngineSceneBusy && id == 0, "no deferred command ambiguity");
        }
        uint32_t threaded = 0;
        std::thread worker([&] { threaded = api.removeObject(api.context, first); }); worker.join();
        Check(threaded == EngineSceneWrongThread && live == 1, "wrong thread rejected");
        struct Reentry { EngineSceneApi api; uint32_t result = 999; } reentry{api};
        Check(api.listTypes(api.context, [](void* p, const EngineSceneTypeInfo*) {
            auto& state = *static_cast<Reentry*>(p);
            state.result = state.api.removeObject(state.api.context, 1);
        }, &reentry) == EngineSceneOk && reentry.result == EngineSceneBusy, "visitor reentry rejected");
        Check(api.renameObject(api.context, first, "renamed") == EngineSceneOk, "rename");
        Check(Read(api).objects[0].type == "Tracked", "rename preserves type identity");
        Check(api.removeObject(api.context, first) == EngineSceneOk && live == 0, "remove destroys object in owner");
        Check(api.removeObject(api.context, first) == EngineSceneNotFound, "stale ID rejected");
        Check(api.createObject(api.context, "Tracked", "survivor", nullptr, &id) == EngineSceneOk, "survivor for scene teardown");
        Check(api.listTypes(nullptr, nullptr, nullptr) == EngineSceneInvalidArgument, "null arguments");
        Check(api.createObject(api.context, nullptr, "x", nullptr, &id) == EngineSceneInvalidArgument && id == 0, "null type");
        Check(api.createObject(api.context, "Tracked", "", nullptr, &id) == EngineSceneInvalidArgument, "empty name");
        Check(api.createObject(api.context, "Tracked", "x", nullptr, nullptr) == EngineSceneInvalidArgument, "missing ID output");
    }
    Check(live == 0, "scene teardown destroys remaining objects");
    Scene scene;
    SceneCommands empty(scene, {});
    auto api = empty.GetApi();
    int count = 0;
    Check(api.listTypes(api.context, [](void* p, const EngineSceneTypeInfo*) { ++*static_cast<int*>(p); }, &count) == EngineSceneOk && count == 0, "empty catalog");
    std::unique_ptr<GameComponent> legacy = std::make_unique<Tracked>();
    Check(scene.AddComponent("legacy", legacy).result == SceneResult::Ok, "legacy addition");
    Check(Read(api).objects[0].type.empty(), "legacy type explicitly unknown");
}
struct Logs { int creates = 0, destroys = 0, stops = 0; };
void __cdecl Log(void* data, const char* message)
{
    auto& logs = *static_cast<Logs*>(data);
    if (!std::strcmp(message, "GameCreate")) ++logs.creates;
    if (!std::strcmp(message, "GameDestroy")) ++logs.destroys;
    if (!std::strcmp(message, "GameStop")) ++logs.stops;
}
void __cdecl Exit(void*) {}
struct Module
{
    HMODULE library = nullptr;
    EngineModuleApi module{};
    void* session = nullptr;
    bool started = false;
    EngineSceneApi scene{};
    ~Module()
    {
        scene = {};
        if (session) { if (started) module.stop(session); module.destroy(session); }
        if (library) FreeLibrary(library);
    }
};
void DllTests(const std::filesystem::path& path)
{
    Logs logs;
    std::string copiedType, copiedLabel;
    for (int pass = 0; pass < 2; ++pass)
    {
        Module m;
        m.library = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        Check(m.library != nullptr, "load actual game DLL");
        auto getModule = reinterpret_cast<EngineGetModuleApiFn>(GetProcAddress(m.library, "EngineGetModuleApi"));
        auto getScene = reinterpret_cast<EngineGetSceneApiFn>(GetProcAddress(m.library, "EngineGetSceneApi"));
        Check(getModule && getScene, "module and optional scene exports");
        Check(getModule(ENGINE_MODULE_VERSION, &m.module) && m.module.kind == ENGINE_MODULE_GAME, "module ABI v4 retained");
        EngineModuleHost host{ENGINE_MODULE_VERSION, &logs, Log, Exit, "Test", "", ""};
        m.session = m.module.create(&host);
        Check(m.session != nullptr, "create session");
        Check(!getScene(ENGINE_SCENE_API_VERSION, sizeof(m.scene), m.session, &m.scene), "query before start rejected");
        Check(m.module.start(m.session) != 0, "start session"); m.started = true;
        Check(getScene(ENGINE_SCENE_API_VERSION, sizeof(m.scene), m.session, &m.scene) && IsCompleteSceneApi(m.scene), "query scene ABI");
        const auto original = m.scene;
        Check(!getScene(999, sizeof(m.scene), m.session, &m.scene) && !std::memcmp(&m.scene, &original, sizeof(original)), "version rejection leaves output intact");
        Check(!getScene(ENGINE_SCENE_API_VERSION, sizeof(m.scene) - 1, m.session, &m.scene), "size mismatch rejected");
        struct Catalog { std::string type, label; int count = 0; bool emptyVisuals = true; } catalog;
        Check(m.scene.listTypes(m.scene.context, [](void* p, const EngineSceneTypeInfo* type) {
            auto& c = *static_cast<Catalog*>(p);
            try { if (std::string(type->typeName) == "RuntimeExample::Mover") { c.type = type->typeName; c.label = type->displayName; } }
            catch (...) { std::terminate(); }
            ++c.count; c.emptyVisuals &= !type->geometryAsset && !type->materialAsset;
        }, &catalog) == EngineSceneOk, "list types across DLL");
        Check(catalog.count == 2 && catalog.type == "RuntimeExample::Mover" && catalog.emptyVisuals, "generated game catalog");
        copiedType = catalog.type; copiedLabel = catalog.label;
        Check(Read(m.scene).objects.empty(), "catalog listing does not construct scene objects");
        uint64_t id = 123;
        Check(m.scene.createObject(m.scene.context, "Missing", "x", nullptr, &id) == EngineSceneUnknownType && id == 0, "unknown type across DLL");
        auto transform = Identity(); transform.position[0] = 3;
        Check(m.scene.createObject(m.scene.context, copiedType.c_str(), "Mover 1", &transform, &id) == EngineSceneOk && id != 0, "create by generated factory across DLL");
        auto snapshot = Read(m.scene);
        Check(snapshot.objects[0].type == copiedType && snapshot.objects[0].transform.position[0] == 3, "initial transform and type preserved without ticking");
        const auto first = id;
        Check(m.scene.createObject(m.scene.context, copiedType.c_str(), "Mover 1", nullptr, &id) == EngineSceneDuplicateName, "duplicate across DLL");
        Check(m.module.update(m.session, 0.25) != 0, "module update");
        Check(std::abs(Read(m.scene).objects[0].transform.position[0] - 3.25f) < 0.0001f, "real derived Tick ran in DLL");
        Check(m.scene.renameObject(m.scene.context, first, "Renamed") == EngineSceneOk, "rename through DLL");
        transform.position[0] = 9;
        Check(m.scene.setTransform(m.scene.context, first, &transform) == EngineSceneOk, "transform through DLL");
        snapshot = Read(m.scene);
        Check(snapshot.objects[0].name == "Renamed" && snapshot.objects[0].type == copiedType && snapshot.objects[0].transform.position[0] == 9, "edits preserve type");
        Check(m.scene.removeObject(m.scene.context, first) == EngineSceneOk && Read(m.scene).objects.empty(), "remove through DLL");
        Check(m.scene.createObject(m.scene.context, copiedType.c_str(), "Alive until destroy", nullptr, &id) == EngineSceneOk, "live object at session teardown");
        // Teardown with a live component exercises destruction before FreeLibrary.
    }
    Check(logs.creates == 2 && logs.stops == 2 && logs.destroys == 2, "session lifecycle across unload/reload");
    Check(copiedType == "RuntimeExample::Mover" && copiedLabel == "Moving object", "copied metadata survives unload");
}
}
int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 3, "usage: SceneModuleTests <Game.dll> <fixtures directory>");
        LocalTests();
        DllTests(std::filesystem::absolute(argv[1]));
        BaseEngine runtime;
        EngineSceneApi empty{};
        Check(!runtime.TryGetActiveSceneApi(empty) && empty.context == nullptr, "no scene API before runtime starts");
        RuntimeLaunchOptions options;
        options.module = std::filesystem::absolute(argv[1]);
        options.projectName = "Scene bridge test";
        options.projectFile = (options.module.parent_path() / "Project.json").u8string();
        options.frames = 1;
        Check(runtime.Run(options) == 0, "actual runtime accepts scene extension");
        Check(!runtime.TryGetActiveSceneApi(empty) && empty.context == nullptr, "runtime clears extension after unload");
        for (const auto& fixture : {L"SceneLegacy", L"SceneBadVersion", L"SceneIncomplete"})
        {
            BaseEngine host;
            options.module = std::filesystem::absolute(argv[2]) / (std::wstring(fixture) + L".dll");
            Check(host.Run(options) == (std::wstring(fixture) == L"SceneLegacy" ? 0 : 1), "runtime optional extension compatibility");
            Check(!host.TryGetActiveSceneApi(empty) && empty.context == nullptr, "no extension after rejected/legacy session");
        }
        std::cout << "Scene module: " << checks << " checks passed; no editor/window/GPU\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
