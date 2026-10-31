#include <Engine/MainGame/BaseGameClass/Game.h>
#ifdef SCENE_MODULE_CREATE_GAME
#define SCENE_MODULE_DOCUMENT_VERSION 3
#else
#define SCENE_MODULE_DOCUMENT_VERSION 2
#endif
#ifndef SCENE_MODULE_CREATE_GAME
static std::unique_ptr<Game> DefaultProjectGame() { return std::make_unique<Game>(); }
#define SCENE_MODULE_CREATE_GAME DefaultProjectGame
#endif
// Shared scene-only module lifecycle. The including project supplies ProjectComponentTypes().
// Optional SCENE_MODULE_START_SCENE(Game&) runs after a loaded scene's window is initialized.
#include <Engine/Runtime/ModuleApi.h>
#include <Engine/Runtime/EditExtensions.h>
#include <cstring>
#include <Engine/Runtime/VfsApi.h>
#include <Engine/Runtime/ModuleVfs.h>
#include <Engine/Scene/SceneCommands.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Runtime/GameSceneApi.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/Runtime/GameWindowApi.h>
#include <Engine/MainGame/Player.h>
#include <thread>
#include <cstdio>
#include <new>

namespace
{
struct Session
{
    EngineModuleHost host;
    std::unique_ptr<Game> game = SCENE_MODULE_CREATE_GAME();
    SceneCommands commands{game->GetScene(), ProjectComponentTypes()};
    uint64_t updates = 0;
    bool started = false, sceneLoaded = false;
    bool startCalled = false, loading = false;
    std::thread::id thread = std::this_thread::get_id();
    explicit Session(const EngineModuleHost& value) : host(value) {}
};
void Log(Session* s, const char* text) { s->host.log(s->host.user, text); }
void* __cdecl Create(const EngineModuleHost* host)
{
    if (!host || host->version != ENGINE_MODULE_VERSION || !host->log || !host->requestExit) return nullptr;
    bool boundVfs = false;
    if (host->getService)
    {
        const auto* vfsApi = static_cast<const EngineVfsApi*>(
            host->getService(host->user, ENGINE_SERVICE_VFS, ENGINE_VFS_API_VERSION));
        if (vfsApi)
        {
            boundVfs = EngineModuleVfs::Bind(vfsApi);
            if (boundVfs)
            {
                const std::string engineRoot = EngineModuleVfs::Resolve("/Engine");
                if (!engineRoot.empty())
                {
                    EnginePaths::SetContentRoot(std::filesystem::u8path(engineRoot));
                }
            }
        }
        else
        {
            std::fprintf(stderr, "[SceneModule] Host getService returned null for VFS service, falling back to local VFS\n");
        }
    }
    else
    {
        std::fprintf(stderr, "[SceneModule] Host getService is null, falling back to local VFS\n");
    }

    Session* s = nullptr;
    try { s = new Session(*host); }
    catch (...)
    {
        if (boundVfs)
        {
            EngineModuleVfs::Unbind();
            EnginePaths::SetContentRoot({});
        }
        return nullptr;
    }
    if (!s)
    {
        if (boundVfs)
        {
            EngineModuleVfs::Unbind();
            EnginePaths::SetContentRoot({});
        }
        return nullptr;
    }
    Log(s, "GameCreate");
    return s;
}
int __cdecl Start(void* instance)
{
    auto* s = static_cast<Session*>(instance);
    s->startCalled = true;
    Log(s, "GameStart");
    Log(s, s->host.projectName);
    try
    {
        if (s->sceneLoaded)
        {
            s->game->SetSimulationTimeScale(1.0f);
            if (!s->game->InitializeRuntimeWindow()) { Log(s, "Game window initialization failed"); return 0; }
            s->game->GetPlayer()->SetPosition({0, 0, 5});
#if defined(SCENE_MODULE_START_SCENE)
            SCENE_MODULE_START_SCENE(*s->game);
#endif
        }
    }
    catch (...) { Log(s, "Game start failed"); return 0; }
    s->started = true;
    return 1;
}
int __cdecl Update(void* instance, double dt)
{
    auto* s = static_cast<Session*>(instance);
    ++s->updates;
    Log(s, "GameUpdate");
    try
    {
        if (s->sceneLoaded)
        {
            const auto result = s->game->RunRuntimeFrame(static_cast<float>(dt));
            if (result == Game::RuntimeFrameResult::Failed) { Log(s, "Game frame failed"); return 0; }
            if (result == Game::RuntimeFrameResult::Closed) s->host.requestExit(s->host.user);
        }
        else s->game->GetScene().Tick(static_cast<float>(dt));
    }
    catch (...) { Log(s, "Game scene update failed"); return 0; }
    return 1;
}
void __cdecl Stop(void* instance)
{
    auto* s = static_cast<Session*>(instance);
    char count[96];
    std::snprintf(count, sizeof(count), "GameUpdates=%llu", static_cast<unsigned long long>(s->updates));
    Log(s, count);
    Log(s, "GameStop");
    s->started = false;
}
void __cdecl Destroy(void* instance)
{
    auto* s = static_cast<Session*>(instance);
    Log(s, "GameDestroy");
    delete s;
    EngineModuleVfs::Unbind();
    EnginePaths::SetContentRoot({});
}
}
// Allocation and user Tick failures are caught inside the module.
extern "C" __declspec(dllexport) int __cdecl EngineGetModuleApi(uint32_t version, EngineModuleApi* api)
{
    if (!api || version != ENGINE_MODULE_VERSION) return 0;
    *api = {ENGINE_MODULE_VERSION, ENGINE_MODULE_GAME, Create, Start, Update, nullptr, Stop, Destroy};
    return 1;
}

extern "C" __declspec(dllexport) int __cdecl EngineGetSceneApi(uint32_t version,
    uint32_t outputSize, void* instance, EngineSceneApi* output)
{
    if (version != ENGINE_SCENE_API_VERSION || outputSize != sizeof(EngineSceneApi) || !instance || !output) return 0;
    auto* session = static_cast<Session*>(instance);
    if (!session->started) return 0;
    *output = session->commands.GetApi();
    return 1;
}

extern "C" __declspec(dllexport) int __cdecl EngineGetGameSceneApi(uint32_t version,
    uint32_t size, void* instance, EngineGameSceneApi* output)
{
    if (version != ENGINE_GAME_SCENE_API_VERSION || size != sizeof(EngineGameSceneApi) || !instance || !output) return 0;
    *output = {ENGINE_GAME_SCENE_API_VERSION, instance,
        [](void* context, const char* path, char* error, uint32_t capacity) -> uint32_t {
            if (capacity && !error) return EngineSceneInvalidArgument;
            if (capacity) error[0] = 0;
            if (!context || !path || !*path) return EngineSceneInvalidArgument;
            auto& s = *static_cast<Session*>(context);
            if (std::this_thread::get_id() != s.thread) return EngineSceneWrongThread;
            if (s.loading) return EngineSceneBusy;
            if (s.startCalled) return EngineSceneReadOnly;
            s.loading = true;
            struct Guard { bool& flag; ~Guard() { flag = false; } } guard{s.loading};
            auto failure = [&](uint32_t status, const char* message) {
                if (capacity) std::snprintf(error, capacity, "%s", message);
                return status;
            };
            try
            {
                if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0))
                    return failure(EngineSceneInvalidArgument, "Expected UTF-8 path");
                const auto file = std::filesystem::u8path(path);
                if (!file.is_absolute()) return failure(EngineSceneInvalidArgument, "Expected absolute path");
                auto data = SceneDocument::Read(file);
                auto candidate = SCENE_MODULE_CREATE_GAME();
                SceneDocument::Restore(data, ProjectComponentTypes(), candidate->GetScene());
                s.commands.Rebind(candidate->GetScene());
                s.game.swap(candidate); s.sceneLoaded = true;
                Log(&s, "GameSceneLoaded");
                return EngineSceneOk;
            }
            catch (const SceneDocumentError& e) { return failure(e.status, e.what()); }
            catch (const std::exception& e) { return failure(EngineSceneFailure, e.what()); }
            catch (...) { return failure(EngineSceneFailure, "Game scene load failed"); }
        }};
    return 1;
}

extern "C" __declspec(dllexport) int __cdecl EngineGetGameWindowApi(uint32_t version,
    uint32_t size, void* instance, EngineGameWindowApi* output)
{
    if (version != ENGINE_GAME_WINDOW_API_VERSION || size != sizeof(EngineGameWindowApi) || !instance || !output) return 0;
    auto& s = *static_cast<Session*>(instance);
    if (!s.started) return 0;
    *output = {ENGINE_GAME_WINDOW_API_VERSION, instance, [](void* p) -> void* {
        auto* display = static_cast<Session*>(p)->game->GetDisplay();
        return display ? display->GetHwnd() : nullptr;
    }};
    return 1;
}

// Independent, edit-safe lifecycle. No ordinary game Create/Start/Update calls.
extern "C" __declspec(dllexport) int __cdecl EngineGetEditExtension(void* context,
    const char* name, uint32_t version, uint32_t size, void* output)
{
    if (!context || !name || !output || version != 1) return 0;
    if (std::strcmp(name, EngineEditorCameraExtension) == 0 && size == sizeof(EngineEditorCameraApi))
    {
        *static_cast<EngineEditorCameraApi*>(output) = static_cast<SceneEditSession*>(context)->GetEditorCameraApi();
        return 1;
    }
    if (std::strcmp(name, EngineViewportInputExtension) == 0 && size == sizeof(EngineViewportInputApi))
    {
        *static_cast<EngineViewportInputApi*>(output) = static_cast<SceneEditSession*>(context)->GetViewportInputApi();
        return 1;
    }
    if (std::strcmp(name, EngineAuthoringExtension)==0 && size==sizeof(EngineAuthoringApi)) {
        *static_cast<EngineAuthoringApi*>(output)=static_cast<SceneEditSession*>(context)->GetAuthoringApi();return 1;
    }
    if (std::strcmp(name, EngineSceneBatchExtension) == 0 && size == sizeof(EngineSceneBatchApi)) {
        *static_cast<EngineSceneBatchApi*>(output) = static_cast<SceneEditSession*>(context)->GetBatchApi(); return 1;
    }
    if (std::strcmp(name, EngineSelectionExtension) == 0 && size == sizeof(EngineSelectionApi))
    {
        *static_cast<EngineSelectionApi*>(output) = static_cast<SceneEditSession*>(context)->GetSelectionApi();
        return 1;
    }
    if (std::strcmp(name, EngineProjectDataExtension) == 0 && size == sizeof(EngineProjectDataApi))
    {
        *static_cast<EngineProjectDataApi*>(output) = static_cast<SceneEditSession*>(context)->GetProjectDataApi();
        return 1;
    }
    if (std::strcmp(name, EngineDocumentExtension) == 0 && size == sizeof(EngineDocumentCapabilities))
    {
        *static_cast<EngineDocumentCapabilities*>(output) = {1,1,3,SCENE_MODULE_DOCUMENT_VERSION};
        return 1;
    }
    if (std::strcmp(name, EnginePointLightExtension) != 0 || size != sizeof(EnginePointLightApi)) return 0;
    const auto scene = static_cast<SceneEditSession*>(context)->GetApi().scene;
    *static_cast<EnginePointLightApi*>(output) = {1, scene.context, scene.setPointLight};
    return 1;
}

extern "C" __declspec(dllexport) int __cdecl EngineGetEditModuleApi(uint32_t version,
    uint32_t size, EngineEditModuleApi* output)
{
    if (version != ENGINE_EDIT_API_VERSION || size != sizeof(EngineEditModuleApi) || !output) return 0;
    *output = {ENGINE_EDIT_API_VERSION,
        [](const char* content, EngineEditSessionApi* api) -> int {
            if (!content || !api) return 0;
            try
            {
                const auto root = std::filesystem::u8path(content);
                if (!root.is_absolute()) return 0;
                EnginePaths::SetContentRoot(root);
                auto session = std::make_unique<SceneEditSession>(ProjectComponentTypes(), SCENE_MODULE_CREATE_GAME);
                *api = session->GetApi();
                session.release(); // released only by the matching DLL destroy below
                return 1;
            }
            catch (...) { return 0; }
        },
        [](void* context) { delete static_cast<SceneEditSession*>(context); }};
    return 1;
}
