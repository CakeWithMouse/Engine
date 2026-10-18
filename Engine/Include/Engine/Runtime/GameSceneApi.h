#pragma once
#include <Engine/Runtime/SceneModuleApi.h>

#define ENGINE_GAME_SCENE_API_VERSION 1u
// Optional extension, queried on a created game session BEFORE start.
// Same thread, no exceptions across ABI. Borrowed until destroy/unload.
struct EngineGameSceneApi
{
    uint32_t version;
    void* context;
    // Absolute UTF-8 path; transactional load, no Tick. Only before start.
    // Optional caller-owned diagnostic buffer; capacity includes trailing NUL.
    uint32_t (ENGINE_MODULE_CALL *loadScene)(void*, const char*, char*, uint32_t);
};
typedef int (ENGINE_MODULE_CALL *EngineGetGameSceneApiFn)(uint32_t version,
    uint32_t outputSize, void* moduleSession, EngineGameSceneApi* output);
inline bool IsCompleteGameSceneApi(const EngineGameSceneApi& api) noexcept
{
    return api.version == ENGINE_GAME_SCENE_API_VERSION && api.context && api.loadScene;
}
