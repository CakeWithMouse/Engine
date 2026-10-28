#pragma once
#include <Engine/Runtime/ModuleApi.h>
constexpr const char* EngineSelectionExtension = "Engine.Editor.Selection";
struct EngineSelectionApi {
    uint32_t version;
    void* context;
    uint32_t (ENGINE_MODULE_CALL *select)(void*, uint64_t);
};
typedef int (ENGINE_MODULE_CALL *EngineBindSelectionFn)(uint32_t, uint32_t, void*, const EngineSelectionApi*);
