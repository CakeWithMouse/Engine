#include <Engine/Runtime/ModuleApi.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <cstdio>
#include <new>

namespace
{
    struct GameSession
    {
        EngineModuleHost host;
        Game* game = nullptr;
        uint64_t updates = 0;
    };

    void Log(GameSession* s, const char* text) { if (s->host.log) s->host.log(s->host.user, text); }

    GameSession* g_session = nullptr; // один игровой экземпляр процесса

    void* __cdecl Create(const EngineModuleHost* host)
    {
        if (!host || host->version != ENGINE_MODULE_VERSION || !host->log || !host->requestExit) return nullptr;
        auto* s = new(std::nothrow) GameSession{};
        if (!s) return nullptr;
        s->host = *host;
        s->game = new(std::nothrow) Game();
        if (!s->game || !s->game->InitializeEditorMode()) // headless: без окна/основного swap chain
        {
            delete s->game;
            delete s;
            return nullptr;
        }
        g_session = s;
        Log(s, "GameCreate (editor/headless)");
        return s;
    }

    int __cdecl Start(void* instance)
    {
        auto* s = static_cast<GameSession*>(instance);
        Log(s, "GameStart");
        Log(s, s->host.projectName ? s->host.projectName : "(no project)");
        return 1;
    }

    int __cdecl Update(void* instance, double dt)
    {
        auto* s = static_cast<GameSession*>(instance);
        ++s->updates;
        s->game->StepEditorFrame(static_cast<float>(dt)); // кадр игры в редакторский вьюпорт (поток движка)
        return 1;
    }

    void __cdecl Stop(void* instance)
    {
        auto* s = static_cast<GameSession*>(instance);
        if (s->game->IsEditorAttached()) s->game->ViewportDetach(); // страховка: отпустить HWND редактора
        s->game->SetPlayMode(ENGINE_PLAY_EDIT);
        char count[96];
        std::snprintf(count, sizeof(count), "GameUpdates=%llu", static_cast<unsigned long long>(s->updates));
        Log(s, count);
        Log(s, "GameStop");
    }

    void __cdecl Destroy(void* instance)
    {
        auto* s = static_cast<GameSession*>(instance);
        Log(s, "GameDestroy");
        if (g_session == s) g_session = nullptr;
        delete s->game;
        delete s;
    }

    // Обёртки backend: void* gameUser -> Game*; вызываются из потока кадров движка.
    int __cdecl BE_Attach(void* gu, void* hwnd, uint32_t w, uint32_t h)
    {
        return static_cast<Game*>(gu)->ViewportAttach(hwnd, w, h);
    }

    int __cdecl BE_Resize(void* gu, uint32_t w, uint32_t h) { return static_cast<Game*>(gu)->ViewportResize(w, h); }
    int __cdecl BE_Detach(void* gu) { return static_cast<Game*>(gu)->ViewportDetach(); }
    int __cdecl BE_PlayMode(void* gu, int mode) { return static_cast<Game*>(gu)->SetPlayMode(mode); }
}

extern "C" __declspec(dllexport) int __cdecl EngineGetModuleApi(uint32_t version, EngineModuleApi* api)
{
    if (!api || version != ENGINE_MODULE_VERSION) return 0;
    *api = {ENGINE_MODULE_VERSION, ENGINE_MODULE_GAME, Create, Start, Update, nullptr, Stop, Destroy};
    return 1;
}

extern "C" __declspec(dllexport) int __cdecl EngineGetGameBackend(uint32_t version, EngineGameBackend* out)
{
    if (!out || version != ENGINE_MODULE_GAME || !g_session || !g_session->game) return 0;
    *out = {ENGINE_MODULE_GAME, g_session->game, BE_Attach, BE_Resize, BE_Detach, BE_PlayMode};
    return 1;
}
