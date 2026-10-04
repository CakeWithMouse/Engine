#include <Engine/Base/Base.h>
#include <Engine/Runtime/ModuleApi.h>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace
{
void __cdecl Log(void*, const char* message) { if (message) std::puts(message); }
void __cdecl Exit(void* requestFlag) { *static_cast<bool*>(requestFlag) = true; }

class LoadedSession
{
    HMODULE library = nullptr;
    EngineModuleApi api{};
    void* session = nullptr;
    bool started = false;
public:
    LoadedSession() = default;
    LoadedSession(const LoadedSession&) = delete;
    LoadedSession& operator=(const LoadedSession&) = delete;
    ~LoadedSession() { Close(); }

    void Open(const std::filesystem::path& path, uint32_t kind, const EngineModuleHost& host)
    {
        library = LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!library) throw std::runtime_error("Cannot load module: " + path.u8string() +
            " (Windows error " + std::to_string(GetLastError()) + ")");
        std::puts("ModuleLoaded");
        const auto getApi = reinterpret_cast<EngineGetModuleApiFn>(GetProcAddress(library, "EngineGetModuleApi"));
        if (!getApi) throw std::runtime_error("Missing export EngineGetModuleApi");
        if (!getApi(ENGINE_MODULE_VERSION, &api) || api.version != ENGINE_MODULE_VERSION || api.kind != kind)
            throw std::runtime_error("Incompatible module version or kind");
        if (!api.create || !api.start || !api.update || !api.stop || !api.destroy ||
            (kind == ENGINE_MODULE_EDITOR && !api.overlay))
            throw std::runtime_error("Incomplete module API");
        session = api.create(&host);
        if (!session) throw std::runtime_error("Module session creation failed");
        if (!api.start(session)) throw std::runtime_error("Module start failed");
        started = true;
    }
    void Update(double dt)
    {
        if (!api.update(session, dt)) throw std::runtime_error("Module update failed");
    }
    void Overlay()
    {
        if (!api.overlay(session)) throw std::runtime_error("Module overlay failed");
    }
    bool Close() noexcept
    {
        if (session)
        {
            if (started) api.stop(session);
            api.destroy(session);
        }
        session = nullptr;
        started = false;
        api = {};
        if (!library) return true;
        const HMODULE closing = library;
        library = nullptr;
        if (!FreeLibrary(closing))
        {
            std::fprintf(stderr, "ModuleUnloadFailed: %lu\n", GetLastError());
            return false;
        }
        std::puts("ModuleUnloaded");
        return true;
    }
};
}

BaseEngine::~BaseEngine() { Stop(); }

bool BaseEngine::Initialize(const BaseGameConfig& config, bool failForTest)
{
    if (state != State::New) return false;
    state = State::Starting;
    Config = config;
    std::puts("EngineStarting");
    return StartUp(failForTest);
}

bool BaseEngine::StartUp(bool failForTest)
{
    if (!Resources.Initialize()) return false;
    if (failForTest) throw std::runtime_error("Injected engine initialization failure");
    std::puts("EngineServicesReady (stub: no window or GPU)");
    state = State::Ready;
    std::puts("EngineReady");
    return true;
}

int BaseEngine::Run(const RuntimeLaunchOptions& options, const BaseGameConfig& config)
{
    if (state != State::New)
    {
        std::fputs("Engine instance has already been used\n", stderr);
        return 1;
    }
    int result = 1;
    try
    {
        if (!Initialize(config, options.failEngineForTest)) throw std::runtime_error("Engine initialization failed");
        result = Play(options);
    }
    catch (const std::exception& error) { std::fprintf(stderr, "RuntimeError: %s\n", error.what()); }
    Stop();
    return result;
}

int BaseEngine::Play(const RuntimeLaunchOptions& options)
{
    if (state != State::Ready) throw std::runtime_error("Engine is not ready");
    const EngineModuleHost host{ENGINE_MODULE_VERSION, &exitRequested, Log, Exit,
        options.projectName.c_str(), options.projectFile.c_str(), options.contentRoot.c_str()};
    LoadedSession module; // Destroy before host/options and before Engine resources.
    try
    {
        module.Open(options.module, options.mode == RuntimeMode::Game ? ENGINE_MODULE_GAME : ENGINE_MODULE_EDITOR, host);
        state = State::Playing;
        if (!options.frames) std::puts("Interactive stub: Enter = next frame, q + Enter = exit (EOF also exits).");
        auto previous = std::chrono::steady_clock::now();
        for (uint64_t frame = 0; !exitRequested && (!options.frames || frame < options.frames); ++frame)
        {
            if (!options.frames)
            {
                std::string command;
                if (!std::getline(std::cin, command) || command == "q") break;
            }
            const auto now = std::chrono::steady_clock::now();
            const double dt = options.frames ? 1.0 / 60.0 : std::chrono::duration<double>(now - previous).count();
            previous = now;
            module.Update(dt);
            std::puts("EngineUpdate (stub)");
            std::puts("EngineRender (stub)");
            if (options.mode == RuntimeMode::Editor) module.Overlay();
            std::puts("EngineFrameEnd (stub)");
        }
    }
    catch (...)
    {
        module.Close();
        state = State::Ready;
        throw;
    }
    const bool closed = module.Close();
    state = State::Ready;
    return closed ? 0 : 1;
}

void BaseEngine::Stop() noexcept
{
    // While a callback is on the stack only request exit, never tear it down.
    if (state == State::Playing) { exitRequested = true; return; }
    if (state == State::New || state == State::Stopped) return;
    Resources.DeInitialize();
    state = State::Stopped;
    std::puts("EngineStopped");
}
