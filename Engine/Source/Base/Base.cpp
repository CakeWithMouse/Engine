#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <Engine/Base/Base.h>
#include <Engine/Runtime/ModuleApi.h>
#include <Engine/Runtime/ViewportSession.h>
#include <Engine/Runtime/GameEditModule.h>
#include <Engine/Runtime/GameSceneApi.h>
#include <Engine/Runtime/GameWindowApi.h>
#include <Engine/Resources/Paths.h>
#include "../Resources/VfsRead.h"
#include "../Runtime/VfsService.h"
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/MainGame/Player.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <thread>

namespace
{
void __cdecl Log(void*, const char* message) { if (message) std::puts(message); }
void __cdecl Exit(void* engine) { static_cast<BaseEngine*>(engine)->RequestExit(); }

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

std::vector<std::string> SplitCommandLine(std::string_view line)
{
    std::vector<std::string> args;
    size_t i = 0;
    while (i < line.size())
    {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= line.size()) break;
        std::string token;
        if (line[i] == '"' || line[i] == '\'')
        {
            const char quote = line[i++];
            while (i < line.size() && line[i] != quote)
            {
                token += line[i++];
            }
            if (i < line.size()) ++i;
        }
        else
        {
            while (i < line.size() && line[i] != ' ' && line[i] != '\t')
            {
                token += line[i++];
            }
        }
        args.push_back(std::move(token));
    }
    return args;
}
}

class BaseEngine::LoadedSession
{
    HMODULE library = nullptr;
    EngineModuleApi api{};
    EngineSceneApi sceneApi{};
    EngineLegacyV4::SceneApi legacyScene{};
    EngineLegacyV5::SceneApi legacy5Scene{};
    EngineBindEditSessionFn bindEdit = nullptr;
    EngineBindProjectDataFn bindProjectData = nullptr;
    EngineBindSelectionFn bindSelection = nullptr;
    EngineBindViewportInputFn bindViewportInput = nullptr;
    EngineBindViewportOverlayFn bindViewportOverlay = nullptr;
    std::function<void()> releaseEdit;
    void* session = nullptr;
    bool started = false;

    std::filesystem::path originalPath;
    std::filesystem::path shadowPath;
    uint32_t moduleKind = 0;
    EngineModuleHost moduleHost{};
    std::filesystem::path startupScenePath;
    bool continuousFrames = false;

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

    void LoadAndInitialize(const std::filesystem::path& path, uint32_t kind, const EngineModuleHost& host,
        const EngineEditSessionApi* editApi = nullptr, const EngineProjectDataApi* projectData = nullptr, const EngineSelectionApi* selection = nullptr, const EngineViewportInputApi* viewportInput = nullptr, const EngineViewportOverlayApi* overlay = nullptr)
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
        if (editApi)
        {
            bindViewportInput = reinterpret_cast<EngineBindViewportInputFn>(GetProcAddress(library, "EngineBindViewportInput"));
            if (bindViewportInput && viewportInput && !bindViewportInput(1, sizeof(*viewportInput), session, viewportInput))
                throw std::runtime_error("Cannot bind viewport input API");
            bindViewportOverlay = reinterpret_cast<EngineBindViewportOverlayFn>(GetProcAddress(library, "EngineBindViewportOverlay"));
            if (bindViewportOverlay && overlay && !bindViewportOverlay(1, sizeof(*overlay), session, overlay))
                throw std::runtime_error("Cannot bind viewport overlay API");
            bindSelection = reinterpret_cast<EngineBindSelectionFn>(GetProcAddress(library, "EngineBindSelection"));
            if (bindSelection && selection && !bindSelection(1, sizeof(*selection), session, selection))
                throw std::runtime_error("Cannot bind selection API");
            bindProjectData = reinterpret_cast<EngineBindProjectDataFn>(GetProcAddress(library, "EngineBindProjectData"));
            if (bindProjectData && projectData && !bindProjectData(1, sizeof(*projectData), session, projectData))
                throw std::runtime_error("Cannot bind project data API");
            bindEdit = reinterpret_cast<EngineBindEditSessionFn>(GetProcAddress(library, "EngineBindEditSession"));
            if (kind != ENGINE_MODULE_EDITOR || !bindEdit ||
                !bindEdit(ENGINE_EDIT_API_VERSION, sizeof(*editApi), session, editApi))
            {
                Close();
                throw std::runtime_error("Editor must support EngineBindEditSession for scene-api mode");
            }
        }
        if (kind == ENGINE_MODULE_GAME && !startupScenePath.empty())
        {
            const auto get = reinterpret_cast<EngineGetGameSceneApiFn>(GetProcAddress(library, "EngineGetGameSceneApi"));
            EngineGameSceneApi sceneLoader{};
            if (!get || !get(ENGINE_GAME_SCENE_API_VERSION, sizeof(sceneLoader), session, &sceneLoader) ||
                !IsCompleteGameSceneApi(sceneLoader))
                throw std::runtime_error("startupScene requires compatible EngineGetGameSceneApi");
            char error[1024]{};
            const auto file = startupScenePath.u8string();
            const auto result = sceneLoader.loadScene(sceneLoader.context, file.c_str(), error, sizeof(error));
            error[sizeof(error) - 1] = 0;
            if (result != EngineSceneOk)
                throw std::runtime_error("Cannot load game startupScene '" + file + "' (status " +
                    std::to_string(result) + "): " + error);
        }
        if (!api.start(session))
        {
            Close();
            throw std::runtime_error("Module start failed");
        }
        started = true;
        if (kind == ENGINE_MODULE_GAME)
        {
            const auto getWindowApi = reinterpret_cast<EngineGetGameWindowApiFn>(GetProcAddress(library, "EngineGetGameWindowApi"));
            if (getWindowApi)
            {
                EngineGameWindowApi windowApi{};
                if (!getWindowApi(ENGINE_GAME_WINDOW_API_VERSION, sizeof(windowApi), session, &windowApi) ||
                    windowApi.version != ENGINE_GAME_WINDOW_API_VERSION || !windowApi.context || !windowApi.getWindow)
                    throw std::runtime_error("Incompatible game window API");
                auto window = static_cast<HWND>(windowApi.getWindow(windowApi.context));
                if (window && !IsWindow(window)) throw std::runtime_error("Invalid game window");
                continuousFrames = window != nullptr;
            }
            const auto getSceneApi = reinterpret_cast<EngineGetSceneApiFn>(GetProcAddress(library, "EngineGetSceneApi"));
            if (getSceneApi && !getSceneApi(ENGINE_SCENE_API_VERSION, sizeof(sceneApi), session, &sceneApi))
            {
                const auto lightsGet = reinterpret_cast<EngineLegacyV5::GetScene>(getSceneApi);
                const auto oldGet = reinterpret_cast<EngineLegacyV4::GetScene>(getSceneApi);
                if (lightsGet(3, sizeof(legacy5Scene), session, &legacy5Scene) && EngineLegacyV5::Complete(legacy5Scene))
                    sceneApi = EngineLegacyV5::Adapt(legacy5Scene);
                else if (oldGet(2, sizeof(legacyScene), session, &legacyScene) && EngineLegacyV4::Complete(legacyScene))
                    sceneApi = EngineLegacyV4::Adapt(legacyScene);
            }
            if (getSceneApi && !IsCompleteSceneApi(sceneApi))
            {
                Close();
                throw std::runtime_error("Incompatible or incomplete scene module API");
            }
        }
    }
public:
    LoadedSession() = default;
    LoadedSession(const LoadedSession&) = delete;
    LoadedSession& operator=(const LoadedSession&) = delete;
    ~LoadedSession() { Close(); }

    void Open(const std::filesystem::path& path, uint32_t kind, const EngineModuleHost& host,
        const EngineEditSessionApi* editApi = nullptr, std::function<void()> cleanup = {},
        const std::filesystem::path& startupScene = {}, const EngineProjectDataApi* projectData = nullptr, const EngineSelectionApi* selection = nullptr, const EngineViewportInputApi* viewportInput = nullptr, const EngineViewportOverlayApi* overlay = nullptr)
    {
        startupScenePath = startupScene;
        releaseEdit = std::move(cleanup);
        LoadAndInitialize(path, kind, host, editApi, projectData, selection, viewportInput, overlay);
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
            Close(); // failed pre-start loading must release the candidate session too
            originalPath = path;
            moduleKind = kind;
            moduleHost = host;
            return false;
        }
    }

    bool IsStarted() const noexcept { return started && session != nullptr; }
    bool UsesContinuousFrames() const noexcept { return continuousFrames; }
    bool EditorViewportReady() const noexcept {
        const auto ready = reinterpret_cast<int(__cdecl*)(void*)>(GetProcAddress(library,"EngineEditorViewportReady"));
        return ready && ready(session);
    }
    bool GetSceneApi(EngineSceneApi& output) const noexcept
    {
        output = IsStarted() ? sceneApi : EngineSceneApi{};
        return IsCompleteSceneApi(output);
    }

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
        sceneApi = {}; // No callable pointers survive session destruction/unloading.
        legacyScene = {};
        legacy5Scene = {};
        continuousFrames = false;
        if (session && bindProjectData) bindProjectData(1, sizeof(EngineProjectDataApi), session, nullptr);
        bindProjectData = nullptr;
        if (session && bindViewportOverlay) bindViewportOverlay(1, sizeof(EngineViewportOverlayApi), session, nullptr);
        bindViewportOverlay = nullptr;
        if (session && bindViewportInput) bindViewportInput(1, sizeof(EngineViewportInputApi), session, nullptr);
        bindViewportInput = nullptr;
        if (session && bindSelection) bindSelection(1, sizeof(EngineSelectionApi), session, nullptr);
        bindSelection = nullptr;
        if (session && bindEdit) bindEdit(ENGINE_EDIT_API_VERSION, sizeof(EngineEditSessionApi), session, nullptr);
        bindEdit = nullptr;
        if (releaseEdit) { auto cleanup = std::move(releaseEdit); releaseEdit = {}; cleanup(); }
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

BaseEngine::BaseEngine()
    : Vfs(VfsRead::GetVFS()), vfsServiceApi(CreateVfsService(Vfs))
{
}
BaseEngine::~BaseEngine() { Stop(); }

bool BaseEngine::TryGetActiveSceneApi(EngineSceneApi& output) const noexcept
{
    output = {};
    return activeSession && activeSession->GetSceneApi(output);
}

bool BaseEngine::ReloadActiveModule()
{
    if (!activeOptions.projectFile.empty() && std::filesystem::exists(
        std::filesystem::u8path(activeOptions.projectFile).parent_path() / "Project.code.json"))
    {
        std::puts("Reopen the C++ project to use its validated module path; live reload is unavailable.");
        return false;
    }
    if (!activeOptions.editModule.empty())
    {
        std::fputs("Close edit session before reloading: live edit data is not reloadable yet\n", stderr);
        return false;
    }
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
    if (!activeOptions.projectFile.empty())
    {
        const auto project = std::filesystem::u8path(activeOptions.projectFile);
        if (std::filesystem::exists(project.parent_path() / "Project.code.json"))
        {
            const auto runtime = EnginePaths::ExecutableDirectory();
            const auto script = runtime / "SDK/ProjectTools.ps1";
            if (!std::filesystem::exists(script)) { std::fputs("Matching project SDK is missing\n", stderr); return false; }
            auto command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + script.wstring() +
                L"\" -Action Build -Project \"" + project.wstring() + L"\" -Runtime \"" + runtime.wstring() + L"\"";
            STARTUPINFOW startup{sizeof(startup)};
            PROCESS_INFORMATION process{};
            if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                project.parent_path().c_str(), &startup, &process)) return false;
            ScopedHandle processHandle(process.hProcess), threadHandle(process.hThread);
            if (waitForCompletion)
            {
                WaitForSingleObject(processHandle.Get(), INFINITE);
                DWORD result = 1;
                GetExitCodeProcess(processHandle.Get(), &result);
                if (result) return false;
            }
            std::puts("Project build dispatched. Reopen the project after a successful build.");
            return true;
        }
    }
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

void BaseEngine::ApplyMount(std::string_view virtualRoot, const std::filesystem::path& physicalPath)
{
    const std::string normRoot = VirtualFileSystem::NormalizeVirtualPath(virtualRoot);
    std::optional<std::filesystem::path> prev;
    for (const auto& m : Vfs.GetMounts())
    {
        if (_stricmp(m.virtualRoot.c_str(), normRoot.c_str()) == 0)
        {
            prev = m.physicalPath;
            break;
        }
    }

    // Record rollback entry BEFORE calling Vfs.Mount so that if Mount throws,
    // the previous mount state can still be restored during exception unwinding in Stop().
    appliedMounts.push_back({ normRoot, prev });
    Vfs.Mount(normRoot, physicalPath);
}

int BaseEngine::Play(const RuntimeLaunchOptions& options)
{
    if (state != State::Ready) throw std::runtime_error("Engine is not ready");
    activeOptions = options;

    // Automatic VFS Mounts
    auto engineContent = EnginePaths::ContentRoot();
    if (!std::filesystem::exists(engineContent))
    {
        const auto repoRoot = FindRepositoryRoot(options.projectFile.empty() ? std::filesystem::path{} : std::filesystem::u8path(options.projectFile).parent_path());
        if (!repoRoot.empty() && std::filesystem::exists(repoRoot / "Engine" / "Content"))
        {
            engineContent = repoRoot / "Engine" / "Content";
        }
    }
    ApplyMount("/Engine", engineContent);
    if (!options.contentRoot.empty())
    {
        ApplyMount("/Game", std::filesystem::u8path(options.contentRoot));
    }

    for (const auto& mount : Vfs.GetMounts())
    {
        std::printf("[VFS] Mounted '%s' -> '%s'\n",
            mount.virtualRoot.c_str(),
            mount.physicalPath.generic_u8string().c_str());
    }

    EngineModuleHost host{ENGINE_MODULE_VERSION, this, Log, Exit,
        options.projectName.c_str(), options.projectFile.c_str(), options.contentRoot.c_str()};
    reloadServiceApi = {1, this, [](void* p, const char* module, const char* checkpoint, void* hwnd,
        uint32_t width, uint32_t height, EngineEditorReloadBindings* output, char* error, uint32_t capacity) -> uint32_t {
        auto* engine = static_cast<BaseEngine*>(p);
        if (!engine->editModule || !module || !checkpoint || !output) return EngineSceneInvalidArgument;
        try {
            const auto result = engine->editModule->Reload(std::filesystem::u8path(module), std::filesystem::u8path(checkpoint), hwnd, width, height, error, capacity);
            if (result == EngineSceneOk) {
                auto& current = *engine->editModule;
                *output = {current.Api(), current.ProjectDataApi(), current.SelectionApi(), current.ViewportInputApi()};
                std::puts("EditorHotReloadSuccess");
            }
            return result;
        } catch (...) { return EngineSceneFailure; }
    }};
    host.getService = [](void* user, uint32_t serviceId, uint32_t minVersion) -> const void* {
        auto* engine = static_cast<BaseEngine*>(user);
        if (serviceId == EngineEditorReloadService && minVersion <= 1) return &engine->reloadServiceApi;
        if (serviceId == EngineViewportOverlayService && minVersion <= 1 && engine->editModule && engine->editModule->ViewportOverlayApi().getCamera) return &engine->editModule->ViewportOverlayApi();
        if(serviceId==EngineAuthoringService && minVersion<=1 && engine->editModule && engine->editModule->AuthoringApi().describe) return &engine->editModule->AuthoringApi();
        if (serviceId == EngineEditorBatchService && minVersion <= 1 && engine->editModule && engine->editModule->BatchApi().apply) return &engine->editModule->BatchApi();
        if (serviceId == ENGINE_SERVICE_VFS && minVersion <= ENGINE_VFS_API_VERSION)
            return &engine->vfsServiceApi;
        return nullptr;
    };
    if (!options.editModule.empty())
    {
        if (options.mode != RuntimeMode::Editor) throw std::runtime_error("editModule requires editor mode");
        editModule = std::make_unique<GameEditModule>(options.editModule, engineContent, std::filesystem::u8path(options.contentRoot));
        if (!options.startupScene.empty())
        {
            const auto& api = editModule->Api();
            const auto path = options.startupScene.u8string();
            char error[1024]{};
            uint64_t sceneId = 0;
            const auto result = api.loadScene(api.context, path.c_str(), &sceneId, error, sizeof(error));
            if (result != EngineSceneOk)
                throw std::runtime_error("Cannot load startupScene '" + path + "' (status " +
                    std::to_string(result) + "): " + error);
        }
    }
    if (options.mode == RuntimeMode::Editor && viewportFactory && !editModule)
    {
        host.viewportAttach = [](void* user, void* hwnd, uint32_t w, uint32_t h) -> int {
            auto& engine = *static_cast<BaseEngine*>(user);
            try
            {
                if (!hwnd || !w || !h || w > INT_MAX || h > INT_MAX) return 0;
                engine.viewportSession.reset();
                auto scene = std::make_unique<ViewportSession>(engine.viewportFactory);
                if (!scene->Attach(static_cast<HWND>(hwnd), static_cast<int>(w), static_cast<int>(h)))
                    return 0;
                engine.viewportSession = std::move(scene);
                std::puts("ViewportReady");
                return 1;
            }
            catch (const std::exception& error) { std::fprintf(stderr, "ViewportError: %s\n", error.what()); }
            catch (...) { std::fputs("ViewportError: initialization failed\n", stderr); }
            return 0;
        };
        host.viewportResize = [](void* user, uint32_t w, uint32_t h) -> int {
            auto& scene = static_cast<BaseEngine*>(user)->viewportSession;
            if (!scene || w > INT_MAX || h > INT_MAX) return 0;
            scene->Resize(static_cast<int>(w), static_cast<int>(h));
            return 1;
        };
        host.viewportCamera = [](void* user, float r, float u, float f, float y, float p, float z) {
            auto& session = static_cast<BaseEngine*>(user)->viewportSession;
            auto* scene = session ? session->Active() : nullptr;
            if (scene && scene->GetPlayer()) {
                scene->GetPlayer()->MoveEditorCamera(r, u, f, y, p, z);
                if (std::getenv("ENGINE_VIEWPORT_INPUT_TRACE")) {
                    const auto position = scene->GetPlayer()->GetPosition();
                    std::printf("ViewportCamera movement=%.4f,%.4f,%.4f position=%.4f,%.4f,%.4f\n", r,u,f,position.x,position.y,position.z);
                }
            }
        };
        host.viewportDetach = [](void* user) -> int {
            static_cast<BaseEngine*>(user)->viewportSession.reset();
            std::puts("ViewportStopped");
            return 1;
        };
        host.viewportControl = [](void* user, uint32_t command) -> int {
            auto& session = static_cast<BaseEngine*>(user)->viewportSession;
            if (!session) return -1;
            try
            {
                if (command == 0) { if (!session->TogglePlay()) return -1; }
                else if (command == 1) session->TogglePause();
                else return -1;
                const int state = static_cast<int>(session->GetState());
                std::printf("ViewportState=%s\n", state == 0 ? "Edit" : state == 1 ? "Play" : "Pause");
                return state;
            }
            catch (const std::exception& error) { std::fprintf(stderr, "ViewportControlError: %s\n", error.what()); }
            catch (...) { std::fputs("ViewportControlError: unknown failure\n", stderr); }
            return -1;
        };
    }
    activeSession = std::make_unique<LoadedSession>();
    try
    {
        activeSession->Open(options.module, options.mode == RuntimeMode::Game ? ENGINE_MODULE_GAME : ENGINE_MODULE_EDITOR, host,
            editModule ? &editModule->Api() : nullptr, [this] { editModule.reset(); }, options.startupScene,
            editModule ? &editModule->ProjectDataApi() : nullptr, editModule ? &editModule->SelectionApi() : nullptr, editModule ? &editModule->ViewportInputApi() : nullptr, editModule ? &editModule->ViewportOverlayApi() : nullptr);
        state = State::Playing;
        if (!options.frames && options.mode != RuntimeMode::Editor) std::puts("Interactive stub: Enter = frame, r = reload, b = build, br = build+reload, ls, find, import, info, mkdir, rm/del, mv, cp, cat, q = exit.");
        auto previous = std::chrono::steady_clock::now();
        for (uint64_t frame = 0; !exitRequested && (!options.frames || frame < options.frames); ++frame)
        {
            if (!options.frames && options.mode != RuntimeMode::Editor && !activeSession->UsesContinuousFrames())
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

                auto tokens = SplitCommandLine(command);
                if (tokens.empty())
                {
                    // Empty Enter advances a frame
                }
                else if (tokens[0] == "ls")
                {
                    const std::string path = tokens.size() > 1 ? tokens[1] : "/";
                    const auto files = Vfs.EnumerateFiles(path, false);
                    std::printf("[VFS] Listing '%s' (%zu file(s)):\n", path.c_str(), files.size());
                    for (const auto& f : files)
                    {
                        std::printf("  [%-7s] %s (%llu bytes)\n",
                            AssetTypeToString(f.type),
                            f.virtualPath.c_str(),
                            static_cast<unsigned long long>(f.sizeBytes));
                    }
                    continue;
                }
                else if (tokens[0] == "find")
                {
                    if (tokens.size() < 2)
                    {
                        std::puts("Usage: find <path> <type_or_pattern>");
                        continue;
                    }
                    const std::string path = tokens[1];
                    AssetType filterType = AssetType::Unknown;
                    std::string pattern;

                    if (tokens.size() >= 3)
                    {
                        const std::string& arg = tokens[2];
                        const AssetType parsed = StringToAssetType(arg);
                        if (parsed != AssetType::Unknown)
                        {
                            filterType = parsed;
                            if (tokens.size() >= 4)
                            {
                                pattern = tokens[3];
                            }
                        }
                        else
                        {
                            pattern = arg;
                        }
                    }

                    const auto results = Vfs.FindFiles(path, filterType, pattern, true);
                    std::printf("[VFS] Found %zu file(s) in '%s':\n", results.size(), path.c_str());
                    for (const auto& f : results)
                    {
                        std::printf("  [%-7s] %s (%llu bytes)\n",
                            AssetTypeToString(f.type),
                            f.virtualPath.c_str(),
                            static_cast<unsigned long long>(f.sizeBytes));
                    }
                    continue;
                }
                else if (tokens[0] == "import")
                {
                    if (tokens.size() < 3)
                    {
                        std::puts("Usage: import <external_source_path> <target_virtual_dir>");
                        continue;
                    }
                    const std::filesystem::path srcPath = std::filesystem::u8path(tokens[1]);
                    const std::string targetDir = tokens[2];
                    const auto imported = Vfs.ImportFile(srcPath, targetDir);
                    if (imported)
                    {
                        std::printf("[VFS] Successfully imported: %s\n", imported->virtualPath.c_str());
                        std::printf("  Type:     %s\n", AssetTypeToString(imported->type));
                        std::printf("  Physical: %s\n", imported->physicalPath.generic_u8string().c_str());
                        std::printf("  Size:     %llu bytes\n", static_cast<unsigned long long>(imported->sizeBytes));
                    }
                    else
                    {
                        std::fprintf(stderr, "[VFS] Import failed for source: %s\n", tokens[1].c_str());
                    }
                    continue;
                }
                else if (tokens[0] == "info")
                {
                    if (tokens.size() < 2)
                    {
                        std::puts("Usage: info <virtual_path>");
                        continue;
                    }
                    const std::string vpath = tokens[1];
                    const auto info = Vfs.GetFileInfo(vpath);
                    if (info)
                    {
                        std::printf("[VFS] Asset info for '%s':\n", info->virtualPath.c_str());
                        std::printf("  Type:     %s\n", AssetTypeToString(info->type));
                        std::printf("  Physical: %s\n", info->physicalPath.generic_u8string().c_str());
                        std::printf("  Size:     %llu bytes\n", static_cast<unsigned long long>(info->sizeBytes));
                        std::printf("  Ext:      %s\n", info->extension.c_str());
                    }
                    else
                    {
                        std::fprintf(stderr, "[VFS] Asset not found: %s\n", vpath.c_str());
                    }
                    continue;
                }
                else if (tokens[0] == "rm" || tokens[0] == "del")
                {
                    if (tokens.size() < 2)
                    {
                        std::puts("Usage: rm <virtual_path> (or del <virtual_path>)");
                        continue;
                    }
                    const std::string path = tokens[1];
                    bool deleted = Vfs.DeleteFile(path);
                    if (!deleted)
                    {
                        deleted = Vfs.DeleteDir(path, true);
                    }
                    if (deleted)
                    {
                        std::printf("[VFS] Deleted '%s'\n", path.c_str());
                    }
                    else
                    {
                        std::fprintf(stderr, "[VFS] Failed to delete '%s' (protected or not found)\n", path.c_str());
                    }
                    continue;
                }
                else if (tokens[0] == "mkdir")
                {
                    if (tokens.size() < 2)
                    {
                        std::puts("Usage: mkdir <virtual_dir>");
                        continue;
                    }
                    const std::string path = tokens[1];
                    if (Vfs.CreateDir(path))
                    {
                        std::printf("[VFS] Created directory '%s'\n", path.c_str());
                    }
                    else
                    {
                        std::fprintf(stderr, "[VFS] Failed to create directory '%s'\n", path.c_str());
                    }
                    continue;
                }
                else if (tokens[0] == "mv")
                {
                    if (tokens.size() < 3)
                    {
                        std::puts("Usage: mv <src_virtual_path> <dst_virtual_path>");
                        continue;
                    }
                    if (Vfs.Move(tokens[1], tokens[2]))
                    {
                        std::printf("[VFS] Moved '%s' -> '%s'\n", tokens[1].c_str(), tokens[2].c_str());
                    }
                    else
                    {
                        std::fprintf(stderr, "[VFS] Failed to move '%s' to '%s'\n", tokens[1].c_str(), tokens[2].c_str());
                    }
                    continue;
                }
                else if (tokens[0] == "cp")
                {
                    if (tokens.size() < 3)
                    {
                        std::puts("Usage: cp <src_virtual_path> <dst_virtual_path>");
                        continue;
                    }
                    if (Vfs.Copy(tokens[1], tokens[2], true))
                    {
                        std::printf("[VFS] Copied '%s' -> '%s'\n", tokens[1].c_str(), tokens[2].c_str());
                    }
                    else
                    {
                        std::fprintf(stderr, "[VFS] Failed to copy '%s' to '%s'\n", tokens[1].c_str(), tokens[2].c_str());
                    }
                    continue;
                }
                else if (tokens[0] == "cat")
                {
                    if (tokens.size() < 2)
                    {
                        std::puts("Usage: cat <virtual_file>");
                        continue;
                    }
                    try
                    {
                        const std::string text = Vfs.ReadText(tokens[1]);
                        std::printf("[VFS] File '%s' (%zu bytes):\n", tokens[1].c_str(), text.size());
                        std::puts(text.c_str());
                    }
                    catch (const std::exception& ex)
                    {
                        std::fprintf(stderr, "[VFS] Failed to read '%s': %s\n", tokens[1].c_str(), ex.what());
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
                if (exitRequested) break;
                if (editModule)
                {
                    const auto result = editModule->Api().frame(editModule->Api().context, std::clamp(dt, 0.0, 0.1));
                    if (result == EngineSceneSimulationFailed) std::fputs("Play Tick failed; game copy paused, edit scene preserved\n", stderr);
                    else if (result != EngineSceneOk) throw std::runtime_error("Edit scene frame failed");
                }
                if (viewportSession)
                {
                    if (!viewportSession->Active()->RenderViewportFrame(static_cast<float>(dt)))
                        throw std::runtime_error("Viewport rendering failed");
                    if (options.frames || frame == 0) std::puts("EngineRender (viewport)");
                }
                else if (editModule)
                {
                    if (options.frames || frame == 0) std::puts("EngineFrame (edit scene API)");
                }
                else if (activeSession->UsesContinuousFrames())
                {
                    if (options.frames || frame == 0) std::puts("EngineFrame (game window)");
                }
                else
                {
                    if (options.frames || options.mode != RuntimeMode::Editor || frame == 0)
                    {
                        std::puts("EngineUpdate (stub)");
                        std::puts("EngineRender (stub)");
                    }
                }
                if (options.mode == RuntimeMode::Editor) activeSession->Overlay();
                if (!activeOptions.readyEvent.empty() && (options.mode != RuntimeMode::Editor || activeSession->EditorViewportReady())) {
                    const auto event = OpenEventW(EVENT_MODIFY_STATE,FALSE,activeOptions.readyEvent.c_str());
                    if (event) { SetEvent(event); CloseHandle(event); }
                    activeOptions.readyEvent.clear();
                }
                if (options.frames || (options.mode != RuntimeMode::Editor && !activeSession->UsesContinuousFrames()))
                    std::puts(editModule ? "EngineFrameEnd (edit scene API)" : viewportSession ? "EngineFrameEnd (viewport)" : activeSession->UsesContinuousFrames() ? "EngineFrameEnd (game window)" : "EngineFrameEnd (stub)");
            }
            else
            {
                std::puts("EngineWaitingForModule (stub)");
            }
            if (options.mode == RuntimeMode::Editor)
                std::this_thread::sleep_until(now + std::chrono::milliseconds(16));
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
    viewportSession.reset();
    editModule.reset();
    Resources.DeInitialize();
    Resources.SetVFS(nullptr);

    // Revert mounts applied by this engine instance in reverse order
    for (auto it = appliedMounts.rbegin(); it != appliedMounts.rend(); ++it)
    {
        try
        {
            if (it->previousPath.has_value())
            {
                Vfs.Mount(it->virtualRoot, *it->previousPath);
            }
            else
            {
                Vfs.Unmount(it->virtualRoot);
            }
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "[BaseEngine] Failed to revert mount '%s': %s\n",
                it->virtualRoot.c_str(), error.what());
        }
        catch (...)
        {
            std::fprintf(stderr, "[BaseEngine] Failed to revert mount '%s': unknown error\n",
                it->virtualRoot.c_str());
        }
    }
    appliedMounts.clear();

    state = State::Stopped;
    std::puts("EngineStopped");
}
