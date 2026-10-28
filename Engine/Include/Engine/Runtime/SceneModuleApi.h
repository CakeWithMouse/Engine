#pragma once
#include <Engine/Runtime/ModuleApi.h>

// Optional, separately versioned extension of ModuleApi v4. Windows x64/default
// packing. No STL, cross-DLL allocation ownership or exceptions. See SCENE-MODULE-API.md.
#define ENGINE_SCENE_API_VERSION 4u
enum EngineSceneStatus : uint32_t
{
    EngineSceneOk = 0, EngineSceneInvalidArgument = 1, EngineSceneUnknownType = 2,
    EngineSceneNotFound = 3, EngineSceneDuplicateName = 4, EngineSceneBusy = 5,
    EngineSceneFailure = 6, EngineSceneWrongThread = 7, EngineSceneReadOnly = 8,
    EngineSceneSimulationFailed = 9, // edit frame paused after a user Tick exception
    EngineSceneInvalidDocument = 10, EngineSceneIoError = 11, EngineSceneUnsupportedVersion = 12
};
struct EngineSceneTransform
{
    float position[3];
    float rotation[4]; // quaternion x,y,z,w; identity = {0,0,0,1}
    float scale[3];
};
struct EngineSceneTypeInfo
{
    const char* typeName;
    const char* displayName;
    // Reserved asset metadata; currently nullptr. Not GPU handles.
    const char* geometryAsset;
    const char* materialAsset;
};
struct EngineScenePointLight
{
    float color[3]; // linear RGB in [0,1]
    float intensity; // finite, >= 0
    float radius; // finite, > 0, world units
    uint32_t enabled; // 0 or 1
};
struct EngineSceneObjectInfo
{
    uint64_t id;
    const char* name;
    const char* typeName; // empty for objects added without a catalog type
    uint64_t parentId;
    uint32_t inheritParentScale;
    EngineSceneTransform transform;
    const char* modelAsset;
    const char* textureAsset;
    uint32_t hasPointLight;
    EngineScenePointLight pointLight;
    const char* materialAsset; // v4: /Game path of a .mat file, empty without one
};
struct EngineSceneInfo
{
    uint64_t id;
    uint64_t revision;
    uint64_t objectCount;
};
typedef void (ENGINE_MODULE_CALL *EngineSceneTypeVisitor)(void*, const EngineSceneTypeInfo*);
typedef void (ENGINE_MODULE_CALL *EngineSceneObjectVisitor)(void*, const EngineSceneObjectInfo*);

struct EngineSceneApi
{
    uint32_t version;
    void* context; // borrowed session-owned service, NOT a Scene/GameComponent pointer
    uint32_t (ENGINE_MODULE_CALL *listTypes)(void*, EngineSceneTypeVisitor, void*);
    uint32_t (ENGINE_MODULE_CALL *readScene)(void*, EngineSceneInfo*, EngineSceneObjectVisitor, void*);
    // On failure output ID = 0; transform nullptr uses the class defaults.
    uint32_t (ENGINE_MODULE_CALL *createObject)(void*, const char* typeName, const char* name,
        const EngineSceneTransform* transform, uint64_t* id);
    uint32_t (ENGINE_MODULE_CALL *removeObject)(void*, uint64_t id);
    uint32_t (ENGINE_MODULE_CALL *renameObject)(void*, uint64_t id, const char* name);
    uint32_t (ENGINE_MODULE_CALL *setTransform)(void*, uint64_t id, const EngineSceneTransform*);
    uint32_t (ENGINE_MODULE_CALL *setPointLight)(void*, uint64_t id, const EngineScenePointLight*);
};

// Optional export EngineGetSceneApi. Query only after successful module start.
// Rejected version/size/session: returns 0 WITHOUT writing output.
// All calls on the session thread, between updates. Visitors synchronous, no throw,
// no reentry; their pointers/strings valid ONLY during the callback. Copy in caller.
// Invalidate the entire table before Stop/Destroy/FreeLibrary/reload.
typedef int (ENGINE_MODULE_CALL *EngineGetSceneApiFn)(uint32_t version, uint32_t outputSize,
    void* moduleSession, EngineSceneApi* output);

inline bool IsCompleteSceneApi(const EngineSceneApi& api) noexcept
{
    return api.version == ENGINE_SCENE_API_VERSION && api.context && api.listTypes &&
        api.readScene && api.createObject && api.removeObject && api.renameObject && api.setTransform;
}
