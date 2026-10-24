#pragma once
#include <Engine/Runtime/SceneModuleApi.h>

#define ENGINE_EDIT_API_VERSION 4u
enum EngineEditState : uint32_t { EngineEditing = 0, EnginePlaying = 1, EnginePaused = 2 };
enum EngineEditControl : uint32_t { EnginePlay = 0, EngineStop = 1, EnginePause = 2, EngineResume = 3 };

// Module-owned context. Same thread, no exceptions/reentry, callback strings copied
// by caller. Invalid after factory.destroy, which MUST precede HWND destruction/unload.
struct EngineEditSessionApi
{
    uint32_t version;
    void* context;
    EngineSceneApi scene; // ALWAYS the persistent edit scene; mutations locked in Play/Pause
    uint32_t (ENGINE_MODULE_CALL *control)(void*, uint32_t command);
    uint32_t (ENGINE_MODULE_CALL *getState)(void*, uint32_t* state);
    uint32_t (ENGINE_MODULE_CALL *readActiveScene)(void*, EngineSceneInfo*, EngineSceneObjectVisitor, void*);
    uint32_t (ENGINE_MODULE_CALL *setParent)(void*, uint64_t child, uint64_t parent, uint32_t inheritScale);
    uint32_t (ENGINE_MODULE_CALL *attachViewport)(void*, void* hwnd, uint32_t width, uint32_t height);
    uint32_t (ENGINE_MODULE_CALL *resizeViewport)(void*, uint32_t width, uint32_t height);
    uint32_t (ENGINE_MODULE_CALL *camera)(void*, float right, float up, float forward, float yaw, float pitch, float zoom);
    uint32_t (ENGINE_MODULE_CALL *frame)(void*, double dt); // ticks once only in Playing, then renders if attached
    // Absolute UTF-8 paths. Optional caller-owned error buffer (capacity includes NUL).
    uint32_t (ENGINE_MODULE_CALL *saveScene)(void*, const char* path, char* error, uint32_t capacity);
    // Edit only; newSceneId is zero on failure. Success invalidates all previous object IDs.
    uint32_t (ENGINE_MODULE_CALL *loadScene)(void*, const char* path, uint64_t* newSceneId, char* error, uint32_t capacity);
    // Viewport NDC coordinates [-1,1], Y upwards. Successful miss returns ID zero.
    uint32_t (ENGINE_MODULE_CALL *pickObject)(void*, float x, float y, uint64_t* id);
    uint32_t (ENGINE_MODULE_CALL *focusObject)(void*, uint64_t id);
    // Absolute project Content directory; configure before loading a scene with assets.
    uint32_t (ENGINE_MODULE_CALL *setContentRoot)(void*, const char*, char*, uint32_t);
    // kind: 0 model, 1 texture. Output buffer must have at least 4096 bytes.
    uint32_t (ENGINE_MODULE_CALL *importAsset)(void*, const char* source, uint32_t kind,
        char* virtualPath, uint32_t pathCapacity, char* error, uint32_t errorCapacity);
    uint32_t (ENGINE_MODULE_CALL *createModel)(void*, const char* virtualPath, const char* name,
        const EngineSceneTransform*, uint64_t* id, char* error, uint32_t capacity);
    // Empty texture path clears the override. All mutations are edit-only.
    uint32_t (ENGINE_MODULE_CALL *setTexture)(void*, uint64_t id, const char* virtualPath, char* error, uint32_t capacity);
};
struct EngineEditModuleApi
{
    uint32_t version;
    // engineContentRoot: absolute UTF-8 EngineContent path (shader root in this DLL).
    // Does NOT call EngineModuleApi create/start/update. Failure leaves output unchanged.
    int (ENGINE_MODULE_CALL *create)(const char* engineContentRoot, EngineEditSessionApi* output);
    void (ENGINE_MODULE_CALL *destroy)(void* context);
};
typedef int (ENGINE_MODULE_CALL *EngineGetEditModuleApiFn)(uint32_t, uint32_t outputSize, EngineEditModuleApi*);
// Optional Editor export, called between Editor.create and start. nullptr unbinds
// before stop/destroy. Editor must copy table, route calls to engine thread, not destroy it.
typedef int (ENGINE_MODULE_CALL *EngineBindEditSessionFn)(uint32_t, uint32_t, void* editorSession, const EngineEditSessionApi*);

inline bool IsCompleteEditSessionApi(const EngineEditSessionApi& api) noexcept
{
    return api.version == ENGINE_EDIT_API_VERSION && api.context && IsCompleteSceneApi(api.scene) &&
        api.control && api.getState && api.readActiveScene && api.setParent && api.attachViewport &&
        api.resizeViewport && api.camera && api.frame && api.saveScene && api.loadScene && api.pickObject && api.focusObject &&
        api.setContentRoot && api.importAsset && api.createModel && api.setTexture;
}
