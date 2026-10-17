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

    // -------------------------------------------------------------------------------------
    // [BACKEND]  Игровой backend, вычитанный из Game.dll, и прокси движка.
    //            Прокси вызываются ТОЛЬКО из кадрового потока (редактор дёргает их внутри
    //            своего Update -> DrainCommands), поэтому глобального мьютекса нет:
    //            StepEditorFrame игры и прокси живут в одном потоке движка.
    //            Сбрасываются в начале каждого Play (см. BaseEngine::Play), чтобы предыдущий
    //            Run не оставил висячий gameUser для следующего инстанса BaseEngine.
    // -------------------------------------------------------------------------------------
    EngineGameBackend g_backend{};
    bool g_haveGame = false;

    int __cdecl Proxy_ViewportAttach(void* /*user*/, void* hwnd, uint32_t w, uint32_t h)
    {
        return (g_haveGame && g_backend.viewportAttach)
                   ? g_backend.viewportAttach(g_backend.gameUser, hwnd, w, h)
                   : 0;
    }

    int __cdecl Proxy_ViewportResize(void* /*user*/, uint32_t w, uint32_t h)
    {
        return (g_haveGame && g_backend.viewportResize)
                   ? g_backend.viewportResize(g_backend.gameUser, w, h)
                   : 0;
    }

    int __cdecl Proxy_ViewportDetach(void* /*user*/)
    {
        // 1 даже без игры: «успешно ничего не делать», чтобы редактор в Stop не получил fail.
        return (g_haveGame && g_backend.viewportDetach)
                   ? g_backend.viewportDetach(g_backend.gameUser)
                   : 1;
    }

    int __cdecl Proxy_SetPlayMode(void* /*user*/, int mode)
    {
        return (g_haveGame && g_backend.setPlayMode)
                   ? g_backend.setPlayMode(g_backend.gameUser, mode)
                   : 0;
    }

    class ScopedHandle
    {
        HANDLE handle = INVALID_HANDLE_VALUE;

    public:
        ScopedHandle() = default;

        explicit ScopedHandle(HANDLE h) noexcept : handle(h)
        {
        }

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
                                 LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
                                 LOAD_LIBRARY_SEARCH_USER_DIRS);
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

    // --- [BACKEND] геттеры для двухпроходной загрузки и reload игры ---
    uint32_t Kind() const noexcept { return moduleKind; }
    const std::filesystem::path& OriginalPath() const noexcept { return originalPath; }
    // Вычитывает EngineGameBackend из УЖЕ СОЗДАННОЙ игры (session жив после Open).
    // GetProcAddress, НЕ dllimport: движок грузит Game.dll динамически.
    bool TryGetGameBackend(EngineGameBackend& out) const
    {
        if (!library) return false;
        using GetBackendFn = int (__cdecl*)(uint32_t, EngineGameBackend*);
        const auto getBackend = reinterpret_cast<GetBackendFn>(GetProcAddress(library, "EngineGetGameBackend"));
        if (!getBackend) return false;
        EngineGameBackend candidate{};
        if (getBackend(ENGINE_MODULE_GAME, &candidate) != 1 || !candidate.gameUser) return false;
        out = candidate;
        return true;
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
    const auto hint = activeOptions.projectFile.empty()
                          ? std::filesystem::path{}
                          : std::filesystem::path(activeOptions.projectFile).parent_path();
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
        const auto repoRoot = FindRepositoryRoot(options.projectFile.empty()
                                                     ? std::filesystem::path{}
                                                     : std::filesystem::path(options.projectFile).parent_path());
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

    // --- [BACKEND] сброс на случай предыдущего Run в этом процессе ---
    g_haveGame = false;
    g_backend = {};

    // gameSession — ЛОКАЛЬНА Play (не член BaseEngine): в Editor-режиме держит игру как
    // backend-провайдера, в Game-режиме пуста. RAII-деструктор закроет её даже при
    // исключении, прошедшем мимо catch (хотя catch ниже вызывает shutdownAll первым).
    std::unique_ptr<LoadedSession> gameSession;
    activeSession = std::make_unique<LoadedSession>();

    // Присваивание по именам: робастно к порядку/числу полей EngineModuleHost (v3).
    // editor=false -> хост ИГРЫ (провайдер, viewport*/setPlayMode остаются nullptr);
    // editor=true  -> хост РЕДАКТОРА с прокси в игру (или nullptr, если backend недоступен,
    //                 и тогда редактор сам уходит в заглушку HasViewport()==false).
    auto fillHost = [&](EngineModuleHost& h, bool editor)
    {
        h = {};
        h.version = ENGINE_MODULE_VERSION;
        h.user = &exitRequested;
        h.log = Log;
        h.requestExit = Exit;
        h.projectName = options.projectName.c_str();
        h.projectFile = options.projectFile.c_str();
        h.contentRoot = options.contentRoot.c_str();
        if (editor)
        {
            h.viewportAttach = g_haveGame ? Proxy_ViewportAttach : nullptr;
            h.viewportResize = g_haveGame ? Proxy_ViewportResize : nullptr;
            h.viewportDetach = g_haveGame ? Proxy_ViewportDetach : nullptr;
            h.setPlayMode = g_haveGame ? Proxy_SetPlayMode : nullptr;
        }
    };

    // Порядок закрытия: РЕДАКТОР ПЕРВЫМ (его Stop шлёт viewportDetach через прокси, и это
    // должно произойти, пока игра жива и swap chain на HWND редактора валиден), затем ИГРА,
    // затем обнуление backend (gameUser становится висячим, но редактор уже мёртв).
    // Универсально по Kind(): в Game-режиме activeSession сама и есть игра.
    auto shutdownAll = [&]() -> bool
    {
        bool ok = true;
        if (activeSession && activeSession->Kind() != ENGINE_MODULE_GAME)
        {
            ok = activeSession->Close() && ok;
            activeSession.reset();
        }
        if (gameSession)
        {
            ok = gameSession->Close() && ok;
            gameSession.reset();
        }
        if (activeSession) // Game-режим: activeSession == игра
        {
            ok = activeSession->Close() && ok;
            activeSession.reset();
        }
        g_haveGame = false;
        g_backend = {};
        return ok;
    };

    try
    {
        if (options.mode == RuntimeMode::Editor)
        {
            // --- проход 1: игра (headless, без окна/основного swap chain внутри Game.dll) ---
            EngineModuleHost hostGame{};
            fillHost(hostGame, false);
            const std::filesystem::path gamePath = options.module.parent_path() / L"Game.dll";
            if (std::filesystem::exists(gamePath))
            {
                gameSession = std::make_unique<LoadedSession>();
                try
                {
                    gameSession->Open(gamePath, ENGINE_MODULE_GAME, hostGame);
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "GameModuleLoadFailed: %s\n", error.what());
                    gameSession.reset();
                }
            }
            else
            {
                std::fprintf(stderr, "GameModuleNotFound: %ls (editor will degrade to no-viewport)\n",
                             gamePath.c_str());
            }

            // --- вычитываем backend У ЖИВОЙ игры (gameUser существует после Open/Create) ---
            if (gameSession && gameSession->IsStarted() && gameSession->TryGetGameBackend(g_backend))
            {
                g_haveGame = true;
                std::puts("GameBackendAcquired");
            }
            else
            {
                g_haveGame = false;
                g_backend = {};
                std::puts("GameBackendUnavailable (editor degrades: no viewport / no play-mode)");
            }

            // --- проход 2: редактор, хост с прокси (или nullptr при !g_haveGame) ---
            EngineModuleHost hostEditor{};
            fillHost(hostEditor, true);
            activeSession->Open(options.module, ENGINE_MODULE_EDITOR, hostEditor);
        }
        else
        {
            // Game-режим: одна игровая сессия, backend не нужен (редактора нет, прокси некому дёргать).
            EngineModuleHost hostGame{};
            fillHost(hostGame, false);
            activeSession->Open(options.module, ENGINE_MODULE_GAME, hostGame);
        }

        state = State::Playing;
        if (!options.frames)
            std::puts(
                "Interactive stub: Enter = frame, r = reload editor, rg = reload game, b = build, br = build+reload, ls, find, import, info, mkdir, rm/del, mv, cp, cat, q = exit.");
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
                // --- [BACKEND] reload игры: прокси смотрят в g_backend, поэтому редактор НЕ
                // перезагружается — он прозрачно подхватит новый gameUser. ЛОКАЛЬНЫЙ лимит:
                // у редактора нет обратного канала host->editor, поэтому после reload игры
                // вьюпорт не переприсоединяется сам (залипнет до перезапуска редактора/движка).
                // Симметрично 'r' для редактора работает полностью: его Start поднимает UI-поток
                // заново, showEvent шлёт ViewportAttach, и attach проходит по свежему backend.
                if (command == "rg")
                {
                    if (options.mode != RuntimeMode::Editor || !gameSession)
                    {
                        std::puts("rg: not available (editor mode with a loaded game required)");
                        continue;
                    }
                    g_haveGame = false;
                    g_backend = {}; // прокси обязаны видеть мёртвый backend на время reload
                    const auto gp = gameSession->OriginalPath();
                    EngineModuleHost hostGame{};
                    fillHost(hostGame, false);
                    gameSession->Close();
                    try
                    {
                        gameSession->Open(gp, ENGINE_MODULE_GAME, hostGame);
                        if (gameSession->TryGetGameBackend(g_backend)) g_haveGame = true;
                        std::puts("GameReloadSuccess");
                    }
                    catch (const std::exception& error)
                    {
                        g_haveGame = false;
                        g_backend = {};
                        std::fprintf(stderr, "GameReloadError: %s\n", error.what());
                    }
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
                    auto filterType = AssetType::Unknown;
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
                        std::fprintf(stderr, "[VFS] Failed to move '%s' to '%s'\n", tokens[1].c_str(),
                                     tokens[2].c_str());
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
                        std::fprintf(stderr, "[VFS] Failed to copy '%s' to '%s'\n", tokens[1].c_str(),
                                     tokens[2].c_str());
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

            // --- [FRAME ORDER] ---
            if (options.mode == RuntimeMode::Editor)
            {
                // 1) Редактор: Update -> DrainCommands дёргает прокси (attach/resize/playmode),
                //    меняя состояние Game ДО шага.
                if (activeSession && activeSession->IsStarted()) activeSession->Update(dt);
                // 2) Игра: StepEditorFrame применяет resize на границе кадра, тикает логику
                //    (только в RUNNING) и рисует в активный (редакторский) набор целей.
                if (gameSession && gameSession->IsStarted()) gameSession->Update(dt);
                // 3) Overlay редактора (no-op, пока Qt не immediate-mode).
                if (activeSession && activeSession->IsStarted()) activeSession->Overlay();
            }
            else
            {
                if (activeSession && activeSession->IsStarted())
                {
                    activeSession->Update(dt);
                    std::puts("EngineUpdate (stub)");
                    std::puts("EngineRender (stub)");
                }
                else
                {
                    std::puts("EngineWaitingForModule (stub)");
                }
            }
        }
    }
    catch (...)
    {
        shutdownAll();
        state = State::Ready;
        throw;
    }
    const bool closed = shutdownAll();
    state = State::Ready;
    return closed ? 0 : 1;
}

void BaseEngine::Stop() noexcept
{
    // While a callback is on the stack only request exit, never tear it down.
    if (state == State::Playing)
    {
        exitRequested = true;
        return;
    }
    if (state == State::New || state == State::Stopped) return;
    // Страховка: в нормальном и исключительном путях Play уже вызвал shutdownAll, поэтому
    // activeSession здесь, как правило, пуст. game-сессия локальна Play и закрыта в нём же
    // (shutdownAll или RAII), сюда не попадает — членом BaseEngine она не является.
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
