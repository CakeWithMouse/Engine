#pragma once
#include <Engine/Runtime/EditSessionApi.h>
#include <Engine/Runtime/ProjectDataApi.h>
#include <Engine/Runtime/SelectionApi.h>
#include <Engine/Runtime/ViewportInputApi.h>

// Optional host service. Frozen Module/Edit v5 layouts are unchanged.
constexpr uint32_t EngineEditorReloadService = 0x01524445u;
struct EngineEditorReloadBindings {
    EngineEditSessionApi scene;
    EngineProjectDataApi data;
    EngineSelectionApi selection;
    EngineViewportInputApi input;
};
struct EngineEditorReloadApi {
    uint32_t version;
    void* context;
    // Engine thread only, Edit mode. Candidate is validated before commit.
    // Success invalidates old tables; failure leaves them callable. No reentry.
    uint32_t (ENGINE_MODULE_CALL *reload)(void*, const char* module, const char* checkpoint,
        void* hwnd, uint32_t width, uint32_t height, EngineEditorReloadBindings*, char*, uint32_t);
};

constexpr const char* EngineEditorCameraExtension = "Engine.Editor.Camera";
struct EngineEditorCameraState { float x, y, z, yaw, pitch; };
struct EngineEditorCameraApi {
    uint32_t version;
    void* context;
    uint32_t (ENGINE_MODULE_CALL *read)(void*, EngineEditorCameraState*);
    uint32_t (ENGINE_MODULE_CALL *write)(void*, const EngineEditorCameraState*);
};
