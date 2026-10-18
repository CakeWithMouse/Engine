#pragma once
#include <Engine/Runtime/ModuleApi.h>
#define ENGINE_GAME_WINDOW_API_VERSION 1u
// Optional post-start extension. Non-null borrowed HWND opts into continuous host
// frames; module update pumps messages/renders, and requests exit on window close.
struct EngineGameWindowApi
{
    uint32_t version;
    void* context;
    void* (ENGINE_MODULE_CALL *getWindow)(void*);
};
typedef int (ENGINE_MODULE_CALL *EngineGetGameWindowApiFn)(uint32_t, uint32_t, void*, EngineGameWindowApi*);
