#pragma once
#include <stdint.h>

// ABI v3: Windows x64, C calling convention, no STL, exceptions or cross-DLL deletes.
// All strings are UTF-8 and borrowed for the duration of the session.
// Host and its user pointer remain valid until Destroy returns. Callbacks are
// synchronous, on the host thread; modules must not keep work alive after Destroy.
#define ENGINE_MODULE_CALL __cdecl
#define ENGINE_MODULE_VERSION 3u
#define ENGINE_MODULE_GAME 1u
#define ENGINE_MODULE_EDITOR 2u

enum EnginePlayMode : int
{
    ENGINE_PLAY_EDIT = 0, // редакторский кадр, логика не тикает
    ENGINE_PLAY_RUNNING = 1, // игровая логика тикает
    ENGINE_PLAY_PAUSED = 2, // логика заморожена, состояние сцены сохранено
};

struct EngineModuleHost
{
    uint32_t version;
    void* user;
    void (ENGINE_MODULE_CALL *log)(void* user, const char* message);
    void (ENGINE_MODULE_CALL *requestExit)(void* user);
    const char* projectName;
    const char* projectFile;
    const char* contentRoot;
    // Optional v2 viewport services (nullptr = unavailable). Call on the host thread.
    // Qt owns HWND and must keep it alive, pumping messages, until detach/Stop returns.
    // Sizes are physical pixels; zero size suspends rendering. No input is captured.
    int (*viewportAttach)(void* user, void* hwnd, uint32_t w, uint32_t h);
    int (*viewportResize)(void* user, uint32_t w, uint32_t h);
    int (*viewportDetach)(void* user);
    // v3: camera intent; movement is in seconds, look in radians, zoom in wheel steps.
    void (*viewportCamera)(void* user, float right, float up, float forward, float yaw, float pitch, float zoom);
    int (*setPlayMode)(void* user, int mode); // EnginePlayMode
};

struct EngineModuleApi
{
    uint32_t version;
    uint32_t kind;
    void* (ENGINE_MODULE_CALL *create)(const EngineModuleHost* host);
    // Nonzero = success. Failure must be logged before returning zero.
    int (ENGINE_MODULE_CALL *start)(void* session);
    int (ENGINE_MODULE_CALL *update)(void* session, double deltaSeconds);
    int (ENGINE_MODULE_CALL *overlay)(void* session); // Required only for editor.
    void (ENGINE_MODULE_CALL *stop)(void* session);
    // Also handles a partially started session. Never throws across the ABI.
    void (ENGINE_MODULE_CALL *destroy)(void* session);
};

// Export: extern "C" __declspec(dllexport) int __cdecl EngineGetModuleApi(...).
// Caller owns the output table; callee checks version before writing it.
using EngineGetModuleApiFn = int(ENGINE_MODULE_CALL *)(uint32_t version, EngineModuleApi* output);
