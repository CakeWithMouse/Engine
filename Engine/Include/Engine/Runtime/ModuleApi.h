#pragma once
#include <stdint.h>

// ABI v5: Windows x64, C calling convention, no STL, exceptions or cross-DLL deletes.
// All strings are UTF-8 and borrowed for the duration of the session.
// Host and its user pointer remain valid until Destroy returns. Callbacks are
// synchronous, on the host thread; modules must not keep work alive after Destroy.
#define ENGINE_MODULE_CALL __cdecl
#define ENGINE_MODULE_VERSION 5u
#define ENGINE_MODULE_GAME 1u
#define ENGINE_MODULE_EDITOR 2u

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
    // v4: 0 toggles Play/Stop, 1 toggles Pause/Resume. Returns state: 0 Edit,
    // 1 Playing, 2 Paused; -1 on failure. No game callbacks run on the UI thread.
    int (*viewportControl)(void* user, uint32_t command);
    // v5: service broker for host capabilities (e.g. ENGINE_SERVICE_VFS).
    // Returns nullptr if service is unknown or version is less than minVersion.
    const void* (*getService)(void* user, uint32_t serviceId, uint32_t minVersion);
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
typedef int (ENGINE_MODULE_CALL *EngineGetModuleApiFn)(uint32_t version, EngineModuleApi* output);
