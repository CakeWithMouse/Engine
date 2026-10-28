#pragma once
#include <Engine/Runtime/ModuleApi.h>
constexpr const char* EngineProjectDataExtension = "Engine.Project.Data";
typedef void (ENGINE_MODULE_CALL *EngineProjectDataVisitor)(void*, const char*);
struct EngineProjectDataApi
{
    uint32_t version;
    void* context;
    uint32_t (ENGINE_MODULE_CALL *read)(void*, uint64_t, EngineProjectDataVisitor, void*);
    uint32_t (ENGINE_MODULE_CALL *write)(void*, uint64_t, const char*);
    uint32_t (ENGINE_MODULE_CALL *remapDuplicate)(void*, uint64_t target, uint64_t source);
};
typedef int (ENGINE_MODULE_CALL *EngineBindProjectDataFn)(uint32_t, uint32_t, void*, const EngineProjectDataApi*);
