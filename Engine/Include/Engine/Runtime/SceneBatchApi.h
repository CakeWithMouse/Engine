#pragma once
#include <Engine/Runtime/SceneModuleApi.h>
constexpr const char* EngineSceneBatchExtension = "Engine.Editor.Batch";
constexpr uint32_t EngineEditorBatchService = 0x01425445u;
enum EngineBatchOperation : uint32_t { EngineBatchTransform = 0, EngineBatchDuplicate = 1, EngineBatchDelete = 2 };
// Optional extension. Old edit tables and DLL layouts remain unchanged.
struct EngineSceneBatchApi {
    uint32_t version;
    void* context;
    // Selection IDs are process-local. apply returns replacement IDs in the same
    // order (zero for deletion). One successful call is an atomic scene edit.
    uint32_t (ENGINE_MODULE_CALL *apply)(void*, uint32_t operation, const uint64_t* ids, uint32_t count,
        uint64_t active, const EngineSceneTransform* target, uint64_t* output, char* error, uint32_t capacity);
    uint32_t (ENGINE_MODULE_CALL *select)(void*, const uint64_t*, uint32_t);
};
