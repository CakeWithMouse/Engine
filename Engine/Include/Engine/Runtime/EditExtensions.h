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

// Visual scripts (.vscript, see Engine/docs/VISUAL-SCRIPTING.md). Edit scene objects carry a script path
// (saved as "assets.script"); Play runs the scripts. Modules without this extension have no scripts.
constexpr const char* EngineScriptExtension = "Engine.Scene.Script";
constexpr uint32_t EngineScriptService = 0x01534352u;
// Text valid during the call only.
using EngineScriptTextVisitor = void (ENGINE_MODULE_CALL *)(void* user, const char* utf8);
// A problem of a script: node 0 is the file as a whole; objectId 0 when it comes from check().
struct EngineScriptProblem
{
    uint64_t objectId;
    const char* objectName; // "" without an object
    const char* script;     // /Game path
    int32_t node;
    const char* message;
    uint32_t runtime;       // 1: stopped while running, 0: did not compile or cannot be read
};
using EngineScriptProblemVisitor = void (ENGINE_MODULE_CALL *)(void* user, const EngineScriptProblem* problem);
struct EngineScriptApi
{
    uint32_t version; // 1
    void* context;
    // The node catalog as JSON (schema, nodes, pins, properties, key names). Any state.
    uint32_t (ENGINE_MODULE_CALL *describe)(void*, EngineScriptTextVisitor, void* user);
    // Script path of an object of the active scene, the one readActiveScene shows ("" without one). Any state.
    uint32_t (ENGINE_MODULE_CALL *get)(void*, uint64_t id, EngineScriptTextVisitor, void* user);
    // Edit state only. "" clears; otherwise an existing .vscript inside /Game.
    uint32_t (ENGINE_MODULE_CALL *set)(void*, uint64_t id, const char* path, char* error, uint32_t capacity);
    // Compiles the file now with the same compiler Play uses; every problem is visited. Ok even with problems.
    uint32_t (ENGINE_MODULE_CALL *check)(void*, const char* path, EngineScriptProblemVisitor, void* user);
    // Problems of the scripts in the running Play scene: stopped scripts and scripts that did not start.
    uint32_t (ENGINE_MODULE_CALL *readProblems)(void*, EngineScriptProblemVisitor, void* user);
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
