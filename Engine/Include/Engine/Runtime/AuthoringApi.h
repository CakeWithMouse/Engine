#pragma once
#include <Engine/Runtime/ProjectDataApi.h>
constexpr const char* EngineAuthoringExtension = "Engine.Editor.Authoring";
constexpr uint32_t EngineAuthoringService = 0x01415554u;
struct EngineAuthoringApi {
    uint32_t version;
    void* context;
    uint32_t (ENGINE_MODULE_CALL *describe)(void*, uint64_t, EngineProjectDataVisitor, void*);
    uint32_t (ENGINE_MODULE_CALL *setProperty)(void*, uint64_t, const char* pointer, const char* jsonValue, char* error, uint32_t capacity);
    // operation 0: reimport path from absolute source (empty uses recorded source).
    // operation 1: move/rename path to another /Game path; references are updated.
    uint32_t (ENGINE_MODULE_CALL *asset)(void*, uint32_t operation, const char* path, const char* sourceOrTarget, char* error, uint32_t capacity);
};
