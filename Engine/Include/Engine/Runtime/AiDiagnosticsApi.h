#pragma once
#include <Engine/Runtime/ModuleApi.h>

// Optional, read-only diagnostics. No changes to the frozen edit-session ABI.
constexpr const char *EngineAiDiagnosticsExtension = "Engine.Scene.AiDiagnostics";
constexpr uint32_t EngineAiDiagnosticsService = 0x01414944u;
struct EngineAiDiagnosticsApi
{
    uint32_t version;
    void *context;
    // JSON request/response, copied during the call. Runs on the scene thread.
    // An empty layer mask clears the overlay, including on a paused scene.
    uint32_t(ENGINE_MODULE_CALL *inspect)(void *, const char *request,
                                          void(ENGINE_MODULE_CALL *visitor)(void *, const char *), void *user);
};
