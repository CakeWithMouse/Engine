#pragma once
#include <Engine/Runtime/ModuleApi.h>
constexpr const char *EngineViewportCursorExtension = "Engine.Viewport.Cursor";
constexpr uint32_t EngineViewportCursorService = 0x01435552u;
// Optional v1 extension. Coordinates are physical viewport pixels; no old table changes.
struct EngineViewportCursorApi
{
    uint32_t version;
    void *context;
    uint32_t(ENGINE_MODULE_CALL *set)(void *, float x, float y, uint32_t width, uint32_t height, uint32_t valid);
};
