#pragma once
#include <Engine/Runtime/SceneModuleApi.h>
#include <Engine/Runtime/EditSessionApi.h>

// v6 adds material callbacks; the shipped v4/v5 layouts remain frozen in their adapters.
static_assert(sizeof(EngineSceneApi) == 72 && sizeof(EngineSceneObjectInfo) == 136 && sizeof(EngineEditSessionApi) == 320,
    "Released base ABI is frozen. Add a named extension instead of changing these layouts.");

// Freeze base layouts. Add capabilities as separately named/versioned tables.
// Rejected queries leave output untouched. Context is the original edit session.
using EngineGetEditExtensionFn = int (ENGINE_MODULE_CALL *)(void* session,
    const char* name, uint32_t version, uint32_t outputSize, void* output);
constexpr const char* EnginePointLightExtension = "Engine.Scene.PointLight";
struct EnginePointLightApi
{
    uint32_t version; // 1
    void* context;
    uint32_t (ENGINE_MODULE_CALL *set)(void*, uint64_t id, const EngineScenePointLight*);
};
constexpr const char* EngineDocumentExtension = "Engine.Scene.Document";
struct EngineDocumentCapabilities
{
    uint32_t version; // 1
    uint32_t minReadVersion, maxReadVersion, preferredWriteVersion;
};
