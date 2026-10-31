#pragma once
#include <Engine/Runtime/ModuleApi.h>
constexpr const char* EngineViewportInputExtension = "Engine.Viewport.Input";
// Optional extension; existing edit API and old modules remain binary compatible.
// key uses Engine Keys values; state changes/focus loss cancel all active actions.
struct EngineViewportInputApi {
    uint32_t version;
    void* context;
    uint32_t (ENGINE_MODULE_CALL *focus)(void*, uint32_t focused);
    uint32_t (ENGINE_MODULE_CALL *key)(void*, uint32_t key, uint32_t down);
    uint32_t (ENGINE_MODULE_CALL *pointer)(void*, float x, float y, float wheel);
    uint32_t (ENGINE_MODULE_CALL *objectKind)(void*, uint64_t id, uint32_t* kind); // 1 Character, 2 Camera, 3 PlaySettings, 0 other
};
typedef int (ENGINE_MODULE_CALL *EngineBindViewportInputFn)(uint32_t, uint32_t, void*, const EngineViewportInputApi*);
