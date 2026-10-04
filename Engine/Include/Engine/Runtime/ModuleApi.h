#pragma once
#include <stdint.h>

// ABI v1: Windows x64, C calling convention, no STL, exceptions or cross-DLL deletes.
// All strings are UTF-8 and borrowed for the duration of the session.
// Host and its user pointer remain valid until Destroy returns. Callbacks are
// synchronous, on the host thread; modules must not keep work alive after Destroy.
#define ENGINE_MODULE_CALL __cdecl
#define ENGINE_MODULE_VERSION 1u
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
