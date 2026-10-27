#pragma once
#include <Engine/Runtime/SceneModuleApi.h>

#define ENGINE_EDIT_API_VERSION 6u
enum EngineEditState : uint32_t { EngineEditing = 0, EnginePlaying = 1, EnginePaused = 2 };
enum EngineEditControl : uint32_t { EnginePlay = 0, EngineStop = 1, EnginePause = 2, EngineResume = 3 };

// v6 material editing. A material is named by the /Game path of its .mat file. Edits reach every
// scene and preview drawing it at once; only saveMaterial writes the file.
enum EngineMaterialBlend : uint32_t
{
    EngineMaterialOpaque = 0, EngineMaterialMasked = 1, EngineMaterialTranslucent = 2, EngineMaterialAdditive = 3
};
struct EngineMaterialRenderState
{
    uint32_t blend; // EngineMaterialBlend
    float alphaClip; // masked only, [0, 1]
    uint32_t unlit, twoSided, castShadows; // 0 or 1
};
struct EngineMaterialParameter
{
    const char* name;
    uint32_t components; // 1-4 floats; 0: a type the editor cannot set
    float value[4];
    float defaultValue[4]; // HLSL initializer
};
struct EngineMaterialTexture
{
    const char* name;
    const char* path; // /Game path; empty: white
};
struct EngineMaterialInfo
{
    const char* shader; // /Game path of the surface .hlsl
    const char* errors; // compiler output; empty when the surface compiles
    EngineMaterialRenderState render;
    uint32_t parameterCount;
    const EngineMaterialParameter* parameters;
    uint32_t textureCount;
    const EngineMaterialTexture* textures;
    uint32_t modified; // changed since it was loaded or saved
};
typedef void (ENGINE_MODULE_CALL *EngineMaterialVisitor)(void*, const EngineMaterialInfo*);
enum EngineMaterialPreviewShape : uint32_t { EnginePreviewSphere = 0, EnginePreviewCube = 1, EnginePreviewPlane = 2 };

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
    // v6: /Game path of a .mat file; empty clears it. Edit only.
    uint32_t (ENGINE_MODULE_CALL *setMaterial)(void*, uint64_t id, const char* virtualPath, char* error, uint32_t capacity);

    // v6 material editing (any state). The visitor runs once, its data valid during the call.
    uint32_t (ENGINE_MODULE_CALL *readMaterial)(void*, const char* material, EngineMaterialVisitor, void*, char* error, uint32_t capacity);
    uint32_t (ENGINE_MODULE_CALL *setMaterialRenderState)(void*, const char* material, const EngineMaterialRenderState*);
    uint32_t (ENGINE_MODULE_CALL *setMaterialParameter)(void*, const char* material, const char* name, const float value[4]);
    // Texture: /Game path, empty for white.
    uint32_t (ENGINE_MODULE_CALL *setMaterialTexture)(void*, const char* material, const char* name, const char* texture,
        char* error, uint32_t capacity);
    uint32_t (ENGINE_MODULE_CALL *saveMaterial)(void*, const char* material, char* error, uint32_t capacity);
    // Re-reads the .mat file, dropping unsaved edits.
    uint32_t (ENGINE_MODULE_CALL *revertMaterial)(void*, const char* material, char* error, uint32_t capacity);
    // Absolute path of a shader file that changed on disk: every material and shader using it recompiles.
    uint32_t (ENGINE_MODULE_CALL *reloadShaderFile)(void*, const char* path);

    // v6 material previews: one small scene per HWND, rendered by frame. Same HWND rules as the viewport.
    uint32_t (ENGINE_MODULE_CALL *attachMaterialPreview)(void*, void* hwnd, uint32_t width, uint32_t height, uint64_t* preview);
    uint32_t (ENGINE_MODULE_CALL *resizeMaterialPreview)(void*, uint64_t preview, uint32_t width, uint32_t height);
    uint32_t (ENGINE_MODULE_CALL *detachMaterialPreview)(void*, uint64_t preview);
    uint32_t (ENGINE_MODULE_CALL *showMaterialPreview)(void*, uint64_t preview, const char* material, uint32_t shape,
        char* error, uint32_t capacity);
    // Orbit around the shape: radians and wheel steps.
    uint32_t (ENGINE_MODULE_CALL *materialPreviewCamera)(void*, uint64_t preview, float yaw, float pitch, float zoom);
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
        api.setContentRoot && api.importAsset && api.createModel && api.setTexture && api.setMaterial &&
        api.readMaterial && api.setMaterialRenderState && api.setMaterialParameter && api.setMaterialTexture &&
        api.saveMaterial && api.revertMaterial && api.reloadShaderFile && api.attachMaterialPreview &&
        api.resizeMaterialPreview && api.detachMaterialPreview && api.showMaterialPreview && api.materialPreviewCamera;
}
