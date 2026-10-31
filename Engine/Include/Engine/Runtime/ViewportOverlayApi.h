#pragma once
#include <Engine/Runtime/ModuleApi.h>

constexpr uint32_t EngineViewportOverlayService = 0x014F564Cu;
constexpr const char* EngineViewportOverlayExtension = "Engine.Viewport.Overlay";

struct EngineOverlayLine
{
    float x0, y0, z0;
    float x1, y1, z1;
    uint32_t rgba;
    uint32_t flags; // bit 0: depthTest, bit 1: alwaysOnTop
};

struct EngineOverlayBillboard
{
    uint32_t iconId;
    float x, y, z;
    float sizePixels;
    uint32_t rgbaTint;
    uint64_t objectId;
};

struct EngineCameraInfo
{
    float position[3];
    float viewMatrix[16];
    float projMatrix[16];
    float fov;
    float aspect;
    uint32_t width;
    uint32_t height;
};

struct EngineViewportOverlayApi
{
    uint32_t version; // 1
    void* context;
    uint32_t (ENGINE_MODULE_CALL *getCamera)(void* context, EngineCameraInfo* cameraInfo);
    uint32_t (ENGINE_MODULE_CALL *setOverlayLines)(void* context, const EngineOverlayLine* lines, uint32_t count, float thickness);
    uint32_t (ENGINE_MODULE_CALL *setOverlayIcon)(void* context, uint32_t iconId, uint32_t width, uint32_t height, const uint8_t* rgbaPixels);
    uint32_t (ENGINE_MODULE_CALL *setOverlayBillboards)(void* context, const EngineOverlayBillboard* billboards, uint32_t count);
};

typedef int (ENGINE_MODULE_CALL *EngineBindViewportOverlayFn)(uint32_t version, uint32_t size, void* instance, const EngineViewportOverlayApi* api);

