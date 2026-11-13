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
    // Values of script variables for one object: a JSON object {"Speed": 45}, "" — the script's defaults
    // ("assets.scriptVariables"). Any state for get; set only in Edit, "" clears.
    uint32_t (ENGINE_MODULE_CALL *getVariables)(void*, uint64_t id, EngineScriptTextVisitor, void* user);
    uint32_t (ENGINE_MODULE_CALL *setVariables)(void*, uint64_t id, const char* json, char* error, uint32_t capacity);
};

// Live state of graph nodes in the running Play scene, for debugging in graph editors. Today the graphs are
// visual scripts; other graph runtimes (behavior trees, state machines) report through the same table and add
// states. Nothing is reported in Edit. Modules without this extension have no trace.
constexpr const char* EngineGraphTraceExtension = "Engine.Scene.GraphTrace";
constexpr uint32_t EngineGraphTraceService = 0x01475452u;
enum EngineGraphNodeState : uint32_t
{
    EngineGraphNodeRan = 1,    // ran or was evaluated at least once
    EngineGraphNodeFailed = 2, // the graph of this object stopped on this node
};
// An object of the Play scene that runs a graph asset. objectId — the edit-scene object of the same name
// (as in EngineScriptProblem); text is valid during the call only.
struct EngineGraphTraceObject
{
    uint64_t objectId;
    const char* objectName;
    uint32_t failed; // 1: the graph of this object is stopped
};
struct EngineGraphTraceNode
{
    int32_t node;       // node id in the file
    uint32_t state;     // EngineGraphNodeState
    uint32_t framesAgo; // 0: during the latest frame
    uint32_t count;     // runs since Play started (saturates)
};
// The last value a data output gave to a reader; text is valid during the call only.
struct EngineGraphTraceValue
{
    int32_t node;
    const char* pin;
    const char* text;
};
using EngineGraphTraceObjectVisitor = void (ENGINE_MODULE_CALL *)(void* user, const EngineGraphTraceObject* object);
using EngineGraphTraceNodeVisitor = void (ENGINE_MODULE_CALL *)(void* user, const EngineGraphTraceNode* node);
using EngineGraphTraceValueVisitor = void (ENGINE_MODULE_CALL *)(void* user, const EngineGraphTraceValue* value);
struct EngineGraphTraceApi
{
    uint32_t version; // 1
    void* context;
    // Objects of the running Play scene that run the graph asset (/Game path).
    uint32_t (ENGINE_MODULE_CALL *listObjects)(void*, const char* asset, EngineGraphTraceObjectVisitor, void* user);
    // Nodes of the asset that ran for objectId (0: the first object running it). NotFound: no such object runs it.
    uint32_t (ENGINE_MODULE_CALL *read)(void*, const char* asset, uint64_t objectId, EngineGraphTraceNodeVisitor, void* user);
    // Values on the data outputs for the same object, as read() chooses it.
    uint32_t (ENGINE_MODULE_CALL *readValues)(void*, const char* asset, uint64_t objectId, EngineGraphTraceValueVisitor, void* user);
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
