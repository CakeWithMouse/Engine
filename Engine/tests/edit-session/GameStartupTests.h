#include <Engine/Runtime/GameSceneApi.h>
#include <Engine/Runtime/GameWindowApi.h>
#include <io.h>

static std::pair<int, std::string> CaptureGameRun(const RuntimeLaunchOptions& options)
{
    FILE* file = nullptr;
    Check(tmpfile_s(&file) == 0 && file, "capture runtime lifecycle");
    std::fflush(stdout);
    const int previous = _dup(_fileno(stdout));
    struct RestoreOutput {
        int previous; FILE* file;
        ~RestoreOutput() { std::fflush(stdout); _dup2(previous, _fileno(stdout)); _close(previous); std::fclose(file); }
    } restore{previous, file};
    Check(previous >= 0 && _dup2(_fileno(file), _fileno(stdout)) == 0, "redirect runtime lifecycle");
    BaseEngine engine;
    const auto result = engine.Run(options);
    std::fflush(stdout); std::rewind(file);
    std::string text; char buffer[1024]; size_t count;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file))) text.append(buffer, count);
    return {result, text};
}

static void ExerciseGameStartup(const std::filesystem::path& module)
{
    const auto file = std::filesystem::temp_directory_path() / (L"game-startup-" + std::to_wstring(GetCurrentProcessId()) + L".json");
    struct Cleanup { std::filesystem::path file; ~Cleanup() { std::error_code ec; std::filesystem::remove(file, ec); } } cleanup{file};
    HMODULE dll = LoadLibraryW(module.c_str()); Check(dll != nullptr, "game startup DLL");
    struct Unload { HMODULE dll; ~Unload() { FreeLibrary(dll); } } unload{dll};
    auto get = reinterpret_cast<EngineGetModuleApiFn>(GetProcAddress(dll, "EngineGetModuleApi"));
    auto getLoad = reinterpret_cast<EngineGetGameSceneApiFn>(GetProcAddress(dll, "EngineGetGameSceneApi"));
    auto getScene = reinterpret_cast<EngineGetSceneApiFn>(GetProcAddress(dll, "EngineGetSceneApi"));
    Check(get && getLoad && getScene, "game scene exports");
    EngineModuleApi api{}; Check(get(ENGINE_MODULE_VERSION, &api) != 0, "game module query");
    std::vector<std::string> events;
    EngineModuleHost host{ENGINE_MODULE_VERSION, &events,
        [](void* p, const char* message) { static_cast<std::vector<std::string>*>(p)->push_back(message); },
        [](void* p) { static_cast<std::vector<std::string>*>(p)->push_back("ExitRequested"); }, "game-load-test", "", ""};
    auto* session = api.create(&host); Check(session != nullptr, "game create");
    struct Destroy { EngineModuleApi& api; void* session; bool started = false;
        ~Destroy() { if (started) api.stop(session); api.destroy(session); } } destroy{api, session};
    EngineGameSceneApi loader{};
    Check(!getLoad(999, sizeof(loader), session, &loader) && !loader.context, "reject loader version");
    Check(!getLoad(ENGINE_GAME_SCENE_API_VERSION, 0, session, &loader), "reject loader size");
    Check(getLoad(ENGINE_GAME_SCENE_API_VERSION, sizeof(loader), session, &loader) && IsCompleteGameSceneApi(loader), "pre-start loader");
    EngineSceneApi scene{};
    Check(!getScene(ENGINE_SCENE_API_VERSION, sizeof(scene), session, &scene), "ordinary scene API still post-start");
    SceneSnapshot data; data.name = "Game startup";
    SceneComponentSnapshot object; object.id = 10; object.name = "Mover"; object.typeName = "RuntimeExample::Mover";
    object.transform.position.x = 5; data.components.push_back(object);
    SceneDocument::Write(file, data);
    const auto path = file.u8string(); char error[512]{};
    Check(loader.loadScene(loader.context, path.c_str(), error, sizeof(error)) == EngineSceneOk, "load before start");
    data.components[0].typeName = "Unknown"; SceneDocument::Write(file, data);
    Check(loader.loadScene(loader.context, path.c_str(), error, sizeof(error)) == EngineSceneUnknownType && error[0], "unknown type rollback");
    uint32_t status = 0;
    std::thread worker([&] { status = loader.loadScene(loader.context, path.c_str(), nullptr, 0); }); worker.join();
    Check(status == EngineSceneWrongThread, "game load thread guard");
    Check(api.start(session) == 1, "start with restored scene"); destroy.started = true;
    Check(getScene(ENGINE_SCENE_API_VERSION, sizeof(scene), session, &scene), "scene query after start");
    auto position = [&] {
        EngineSceneInfo info{}; float x = -100;
        Check(scene.readScene(scene.context, &info, [](void* p, const EngineSceneObjectInfo* o) {
            *static_cast<float*>(p) = o->transform.position[0]; }, &x) == EngineSceneOk && info.objectCount == 1, "one restored object");
        return x;
    };
    Check(position() == 5, "load/start do not Tick; failed load retained object");
    Check(loader.loadScene(loader.context, path.c_str(), error, sizeof(error)) == EngineSceneReadOnly, "load locked after start");
    Check(api.update(session, 0.0625) == 1 && position() == 5.0625f, "one Tick per update");
    Check(api.update(session, 0.0625) == 1 && position() == 5.125f, "second update ticks once");
    Check(events.size() >= 3 && events[0] == "GameCreate" && events[1] == "GameSceneLoaded" && events[2] == "GameStart", "create load start order");
    auto getWindow = reinterpret_cast<EngineGetGameWindowApiFn>(GetProcAddress(dll, "EngineGetGameWindowApi"));
    EngineGameWindowApi windowApi{};
    Check(getWindow && getWindow(ENGINE_GAME_WINDOW_API_VERSION, sizeof(windowApi), session, &windowApi), "window API");
    const auto window = static_cast<HWND>(windowApi.getWindow(windowApi.context));
    Check(IsWindow(window), "module owns real window");
    PostMessageW(window, WM_CLOSE, 0, 0);
    Check(api.update(session, 0.0625) && events.back() == "ExitRequested" && position() == 5.125f, "close requests host exit before another Tick");
    Check(IsWindow(window), "window retained until resources destroyed");
    api.stop(session); destroy.started = false;
    Check(loader.loadScene(loader.context, path.c_str(), error, sizeof(error)) == EngineSceneReadOnly, "stopped session cannot reload");
    // Runtime compatibility: old game DLL works without startupScene, fails with it.
    const auto legacy = EnginePaths::ExecutableDirectory() / "RuntimeFixtures/SceneLegacy.dll";
    RuntimeLaunchOptions options; options.mode = RuntimeMode::Game; options.module = legacy; options.frames = 1;
    { BaseEngine engine; Check(engine.Run(options) == 0, "legacy module still starts"); }
    options.startupScene = file;
    auto result = CaptureGameRun(options);
    Check(result.first != 0 && result.second.find("FixtureStart") == std::string::npos &&
        result.second.find("FixtureDestroy") != std::string::npos, "missing loader destroys session without start/stop");
    options.module = module;
    for (const auto& invalid : {SceneDocument::Serialize(data), std::string("{"), std::string("{\"version\":2}")})
    {
        { std::ofstream output(file); output << invalid; }
        result = CaptureGameRun(options);
        Check(result.first != 0 && result.second.find("GameStart") == std::string::npos &&
            result.second.find("GameStop") == std::string::npos && result.second.find("GameDestroy") != std::string::npos,
            "failed startup destroys without start/update/stop");
    }
    data.components[0].typeName = "RuntimeExample::Mover"; SceneDocument::Write(file, data);
    result = CaptureGameRun(options);
    Check(result.first == 0 && result.second.find("GameSceneLoaded") < result.second.find("GameStart") &&
        result.second.find("GameUpdates=1") != std::string::npos && result.second.find("GameStop") < result.second.find("GameDestroy"),
        "runtime loads before start and updates once per frame");
    // The actual shipped JSON must be deployed beside the project and load its visible type.
    options.startupScene = module.parent_path() / "Content/Scenes/Main.scene.json";
    result = CaptureGameRun(options);
    Check(result.first == 0 && result.second.find("Cube initialized") != std::string::npos &&
        result.second.find("EngineFrame (game window)") != std::string::npos, "shipped visible scene starts");
    options.frames = 0;
    const auto threadId = GetCurrentThreadId();
    std::thread closeWindow([threadId] { Sleep(500); PostThreadMessageW(threadId, WM_QUIT, 0, 0); });
    result = CaptureGameRun(options);
    closeWindow.join();
    MSG quit{}; PeekMessageW(&quit, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE);
    Check(result.first == 0 && result.second.find("GameUpdate") != std::string::npos,
        "window game advances without console Enter and quits cleanly");
}
