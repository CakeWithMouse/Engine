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
constexpr const char* EnginePointLightV2Extension = "Engine.Scene.PointLightV2";
constexpr uint32_t EnginePointLightV2Service = 0x01504C32u;
struct EnginePointLightV2Api
{
    uint32_t version; // 1
    void* context;
    uint32_t (ENGINE_MODULE_CALL *set)(void*, uint64_t id, const EngineScenePointLight*, uint32_t castShadows);
};
constexpr const char* EngineDocumentExtension = "Engine.Scene.Document";
struct EngineDocumentCapabilities
{
    uint32_t version; // 1
    uint32_t minReadVersion, maxReadVersion, preferredWriteVersion;
};

// Audio asset preview: decodes and plays a /Game audio file through the edit scene's
// audio engine. Edit state only — Play owns its game audio.
constexpr const char* EngineAudioPreviewExtension = "Engine.Scene.AudioPreview";
struct EngineAudioPreviewApi
{
    uint32_t version; // 1
    void* context;
    uint32_t (ENGINE_MODULE_CALL *play)(void*, const char* virtualPath, char* error, uint32_t capacity);
    uint32_t (ENGINE_MODULE_CALL *stop)(void*);
};
// Optional Editor export; nullptr unbinds before stop/destroy. Editor must copy the table.
typedef int (ENGINE_MODULE_CALL *EngineBindAudioPreviewFn)(uint32_t, uint32_t, void* editorSession, const EngineAudioPreviewApi*);

// Asset import with options: enables passing serialization options (e.g. sidecar JSON)
// to model/asset importers without changing the frozen EngineEditSessionApi layout.
constexpr const char* EngineAssetImportExtension = "Engine.Asset.Import";
struct EngineAssetImportApi
{
    uint32_t version; // 1
    void* context;
    uint32_t (ENGINE_MODULE_CALL *importWithOptions)(void* context, const char* source, uint32_t kind,
        const char* optionsJson, char* virtualPath, uint32_t pathCapacity, char* error, uint32_t errorCapacity);
};
