#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <Engine/Base/Base.h>
#include <Engine/Runtime/ModuleApi.h>
#include <Engine/Resources/Paths.h>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
void __cdecl Log(void*, const char* message) { if (message) std::puts(message); }
void __cdecl Exit(void* requestFlag) { *static_cast<bool*>(requestFlag) = true; }

class ScopedHandle
{
    HANDLE handle = INVALID_HANDLE_VALUE;
public:
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE h) noexcept : handle(h) {}
    ~ScopedHandle() noexcept { Close(); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : handle(other.handle) { other.handle = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            handle = other.handle;
            other.handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    void Close() noexcept
    {
        if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
        {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }
    HANDLE Get() const noexcept { return handle; }
    bool IsValid() const noexcept { return handle != INVALID_HANDLE_VALUE && handle != nullptr; }
};

std::filesystem::path FindRepositoryRoot(const std::filesystem::path& startHint = {})
{
    std::filesystem::path dir = startHint.empty() ? EnginePaths::ExecutableDirectory() : startHint;
    while (!dir.empty())
    {
        if (std::filesystem::exists(dir / "Engine" / "build.ps1"))
        {
            return dir;
        }
        const auto parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}
}

class BaseEngine::LoadedSession
{
    HMODULE library = nullptr;
    EngineModuleApi api{};
    void* session = nullptr;
    bool started = false;

    std::filesystem::path originalPath;
    std::filesystem::path shadowPath;
    uint32_t moduleKind = 0;
    EngineModuleHost moduleHost{};

    static void CleanStaleShadows(const std::filesystem::path& shadowDir, const std::wstring& stem)
    {
        std::error_code ec;
        if (!std::filesystem::exists(shadowDir, ec)) return;
        for (const auto& entry : std::filesystem::directory_iterator(shadowDir, ec))
        {
            if (entry.is_regular_file(ec))
            {
                const auto entryStem = entry.path().stem().wstring();
                if (entryStem.rfind(stem, 0) == 0)
                {
                    std::filesystem::remove(entry.path(), ec);
                }
            }
        }
    }

    std::filesystem::path CreateShadow(const std::filesystem::path& sourcePath)
    {
        if (!std::filesystem::exists(sourcePath))
        {
            throw std::runtime_error("Cannot load module: source does not exist: " + sourcePath.u8string());
        }

        const auto shadowDir = sourcePath.parent_path() / ".shadow_cache";
        std::error_code ec;
        std::filesystem::create_directories(shadowDir, ec);

        const auto stem = sourcePath.stem().wstring();
        const auto ext = sourcePath.extension().wstring();

        CleanStaleShadows(shadowDir, stem);

        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto shadowFilename = stem + L"_" + std::to_wstring(timestamp) + ext;
        const auto targetPath = shadowDir / shadowFilename;

        int retries = 5;
        while (retries-- > 0)
        {
            std::filesystem::copy_file(sourcePath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (!ec) break;
            Sleep(50);
        }
        if (ec)
        {
            throw std::runtime_error("Failed to copy shadow module: " + ec.message());
        }

        auto sourcePdb = sourcePath;
        sourcePdb.replace_extension(L".pdb");
        if (std::filesystem::exists(sourcePdb, ec))
        {
            auto targetPdb = targetPath;
            targetPdb.replace_extension(L".pdb");
            std::filesystem::copy_file(sourcePdb, targetPdb, std::filesystem::copy_options::overwrite_existing, ec);
        }

        return targetPath;
    }

    void RemoveShadow() noexcept
    {
        if (shadowPath.empty()) return;
        std::error_code ec;
        std::filesystem::remove(shadowPath, ec);
        auto shadowPdb = shadowPath;
        shadowPdb.replace_extension(L".pdb");
        std::filesystem::remove(shadowPdb, ec);
        shadowPath.clear();
    }

    void LoadAndInitialize(const std::filesystem::path& path, uint32_t kind, const EngineModuleHost& host)
    {
        originalPath = path;
        moduleKind = kind;
        moduleHost = host;

        shadowPath = CreateShadow(originalPath);

        DLL_DIRECTORY_COOKIE cookie = AddDllDirectory(originalPath.parent_path().c_str());
        library = LoadLibraryExW(shadowPath.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
        const DWORD loadError = GetLastError();
        if (cookie) RemoveDllDirectory(cookie);

        if (!library)
        {
            RemoveShadow();
            throw std::runtime_error("Cannot load shadow module: " + shadowPath.u8string() +
                " (Windows error " + std::to_string(loadError) + ")");
        }
        std::puts("ModuleLoaded");

        const auto getApi = reinterpret_cast<EngineGetModuleApiFn>(GetProcAddress(library, "EngineGetModuleApi"));
        if (!getApi)
        {
            Close();
            throw std::runtime_error("Missing export EngineGetModuleApi");
        }
        if (!getApi(ENGINE_MODULE_VERSION, &api) || api.version != ENGINE_MODULE_VERSION || api.kind != kind)
        {
            Close();
            throw std::runtime_error("Incompatible module version or kind");
        }
        if (!api.create || !api.start || !api.update || !api.stop || !api.destroy ||
            (kind == ENGINE_MODULE_EDITOR && !api.overlay))
        {
            Close();
            throw std::runtime_error("Incomplete module API");
        }
        session = api.create(&moduleHost);
        if (!session)
        {
            Close();
            throw std::runtime_error("Module session creation failed");
        }
        if (!api.start(session))
        {
            Close();
            throw std::runtime_error("Module start failed");
        }
        started = true;
    }
public:
    LoadedSession() = default;
    LoadedSession(const LoadedSession&) = delete;
    LoadedSession& operator=(const LoadedSession&) = delete;
    ~LoadedSession() { Close(); }

    void Open(const std::filesystem::path& path, uint32_t kind, const EngineModuleHost& host)
    {
        LoadAndInitialize(path, kind, host);
    }

    bool Reload()
    {
        std::puts("ModuleReloadRequested");
        const auto path = originalPath;
        const auto kind = moduleKind;
        const auto host = moduleHost;

        Close();

        try
        {
            LoadAndInitialize(path, kind, host);
            std::puts("ModuleReloadSuccess");
            return true;
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "ModuleReloadError: %s\n", error.what());
            originalPath = path;
            moduleKind = kind;
            moduleHost = host;
            return false;
        }
    }

    bool IsStarted() const noexcept { return started && session != nullptr; }

    void Update(double dt)
    {
        if (session && started && !api.update(session, dt)) throw std::runtime_error("Module update failed");
    }
    void Overlay()
    {
        if (session && started && !api.overlay(session)) throw std::runtime_error("Module overlay failed");
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
        bool unloadOk = true;
        if (library)
        {
            const HMODULE closing = library;
            library = nullptr;
            if (!FreeLibrary(closing))
            {
                std::fprintf(stderr, "ModuleUnloadFailed: %lu\n", GetLastError());
                unloadOk = false;
            }
            else
            {
                std::puts("ModuleUnloaded");
            }
        }
        RemoveShadow();
        return unloadOk;
    }
};

BaseEngine::BaseEngine() = default;
BaseEngine::~BaseEngine() { Stop(); }

bool BaseEngine::ReloadActiveModule()
{
    if (state != State::Playing || !activeSession)
    {
        std::fputs("Cannot reload module: engine is not playing or has no active session\n", stderr);
        return false;
    }
    return activeSession->Reload();
}

bool BaseEngine::TriggerBuild(const std::filesystem::path& repositoryRoot,
                              const std::string& target,
                              const std::string& configuration,
                              bool waitForCompletion)
{
    const auto buildScript = repositoryRoot / "Engine" / "build.ps1";
    if (!std::filesystem::exists(buildScript))
    {
        std::fprintf(stderr, "BuildScriptNotFound: %s\n", buildScript.u8string().c_str());
        return false;
    }

    std::wstring commandLine = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" +
                               buildScript.wstring() + L"\"";
    if (!configuration.empty())
    {
        commandLine += L" -Configuration " + std::wstring(configuration.begin(), configuration.end());
    }
    if (!target.empty())
    {
        commandLine += L" -Target " + std::wstring(target.begin(), target.end());
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
    cmdBuffer.push_back(L'\0');

    std::wprintf(L"Triggering build: %s\n", commandLine.c_str());

    const BOOL success = CreateProcessW(
        nullptr,
        cmdBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        repositoryRoot.c_str(),
        &si,
        &pi
    );

    if (!success)
    {
        std::fprintf(stderr, "CreateProcessFailed: %lu\n", GetLastError());
        return false;
    }

    ScopedHandle process(pi.hProcess);
    ScopedHandle thread(pi.hThread);

    if (waitForCompletion)
    {
        WaitForSingleObject(process.Get(), INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(process.Get(), &exitCode);
        if (exitCode != 0)
        {
            std::fprintf(stderr, "Build failed with exit code: %lu\n", exitCode);
            return false;
        }
        std::puts("Build completed successfully");
    }
    else
    {
        std::printf("Background build process launched (PID %lu)\n", pi.dwProcessId);
    }
    return true;
}

bool BaseEngine::TriggerProjectBuild(bool waitForCompletion)
{
    const auto hint = activeOptions.projectFile.empty() ? std::filesystem::path{} : std::filesystem::path(activeOptions.projectFile).parent_path();
    const auto repoRoot = FindRepositoryRoot(hint);
    if (repoRoot.empty())
    {
        std::fputs("Cannot find repository root for build trigger\n", stderr);
        return false;
    }
    return TriggerBuild(repoRoot, "EngineRuntime", "Debug", waitForCompletion);
}

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
    Resources.SetVFS(&Vfs);
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
    activeOptions = options;

    // Automatic VFS Mounts
    auto engineContent = EnginePaths::ContentRoot();
    if (!std::filesystem::exists(engineContent))
    {
        const auto repoRoot = FindRepositoryRoot(options.projectFile.empty() ? std::filesystem::path{} : std::filesystem::path(options.projectFile).parent_path());
        if (!repoRoot.empty() && std::filesystem::exists(repoRoot / "Engine" / "Content"))
        {
            engineContent = repoRoot / "Engine" / "Content";
        }
    }
    Vfs.Mount("/Engine", engineContent);
    if (!options.contentRoot.empty())
    {
        Vfs.Mount("/Game", options.contentRoot);
    }

    for (const auto& mount : Vfs.GetMounts())
    {
        std::printf("[VFS] Mounted '%s' -> '%s'\n",
            mount.virtualRoot.c_str(),
            mount.physicalPath.generic_u8string().c_str());
    }

    const EngineModuleHost host{ENGINE_MODULE_VERSION, &exitRequested, Log, Exit,
        options.projectName.c_str(), options.projectFile.c_str(), options.contentRoot.c_str()};
    activeSession = std::make_unique<LoadedSession>();
    try
    {
        activeSession->Open(options.module, options.mode == RuntimeMode::Game ? ENGINE_MODULE_GAME : ENGINE_MODULE_EDITOR, host);
        state = State::Playing;
        if (!options.frames) std::puts("Interactive stub: Enter = frame, r = reload, b = build, br = build+reload, q = exit.");
        auto previous = std::chrono::steady_clock::now();
        for (uint64_t frame = 0; !exitRequested && (!options.frames || frame < options.frames); ++frame)
        {
            if (!options.frames)
            {
                std::string command;
                if (!std::getline(std::cin, command) || command == "q") break;
                if (command == "r")
                {
                    ReloadActiveModule();
                    continue;
                }
                if (command == "b")
                {
                    TriggerProjectBuild(false);
                    continue;
                }
                if (command == "br")
                {
                    if (TriggerProjectBuild(true))
                    {
                        ReloadActiveModule();
                    }
                    continue;
                }
            }
            const auto now = std::chrono::steady_clock::now();
            const double dt = options.frames ? 1.0 / 60.0 : std::chrono::duration<double>(now - previous).count();
            previous = now;
            if (activeSession && activeSession->IsStarted())
            {
                activeSession->Update(dt);
                std::puts("EngineUpdate (stub)");
                std::puts("EngineRender (stub)");
                if (options.mode == RuntimeMode::Editor) activeSession->Overlay();
                std::puts("EngineFrameEnd (stub)");
            }
            else
            {
                std::puts("EngineWaitingForModule (stub)");
            }
        }
    }
    catch (...)
    {
        if (activeSession) activeSession->Close();
        activeSession.reset();
        state = State::Ready;
        throw;
    }
    const bool closed = activeSession ? activeSession->Close() : true;
    activeSession.reset();
    state = State::Ready;
    return closed ? 0 : 1;
}

void BaseEngine::Stop() noexcept
{
    // While a callback is on the stack only request exit, never tear it down.
    if (state == State::Playing) { exitRequested = true; return; }
    if (state == State::New || state == State::Stopped) return;
    if (activeSession)
    {
        activeSession->Close();
        activeSession.reset();
    }
    Resources.DeInitialize();
    Resources.SetVFS(nullptr);
    Vfs.UnmountAll();
    state = State::Stopped;
    std::puts("EngineStopped");
}
