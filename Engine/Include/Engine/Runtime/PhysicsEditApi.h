#pragma once
#include <Engine/Runtime/ModuleApi.h>
#include <cstdint>

// Named edit-session extension "Engine.Physics": read/write of the physics
// simulation settings of the session's active Game (edit scene or Play copy).
// Plain-C POD state; no engine or Jolt types cross this boundary.
// Both callbacks run on the engine session thread.
constexpr const char* EnginePhysicsExtension = "Engine.Physics";

struct EnginePhysicsSettingsState
{
    uint32_t enabled;     // 0/1: physics simulation while playing
    float gravityX, gravityY, gravityZ;
    float fixedTimeStep;  // seconds, > 0
    uint32_t maxSubSteps; // fixed steps per frame cap, > 0
    uint32_t velocitySteps;
    uint32_t positionSteps;
};

struct EnginePhysicsApi
{
    uint32_t version; // 1
    void* context;
    uint32_t (ENGINE_MODULE_CALL *read)(void*, EnginePhysicsSettingsState*);
    uint32_t (ENGINE_MODULE_CALL *write)(void*, const EnginePhysicsSettingsState*);
    // 1 when the engine was built with Jolt physics; settings round-trip regardless.
    uint32_t (ENGINE_MODULE_CALL *available)(void*);
};
typedef int (ENGINE_MODULE_CALL *EngineBindPhysicsFn)(uint32_t, uint32_t, void*, const EnginePhysicsApi*);
