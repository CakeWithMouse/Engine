#pragma once
#include <Engine/Runtime/ModuleApi.h>
#include <cstdint>

// Named edit-session extension "Engine.Scene.PhysicsObject": per-object rigid body
// settings (enable flag, motion type, collision shape and material) of any scene
// object. Plain-C POD state; no engine or Jolt types cross this boundary.
// Both callbacks run on the engine session thread; the editor only sends writes
// in the Edit state, where the persistent scene is mutable.
constexpr const char* EnginePhysicsObjectExtension = "Engine.Scene.PhysicsObject";

// Motion type: 0 = static (never moves), 1 = dynamic (fully simulated),
// 2 = kinematic (moved by gameplay, pushes dynamic bodies).
constexpr uint32_t EnginePhysicsBodyStatic = 0;
constexpr uint32_t EnginePhysicsBodyDynamic = 1;
constexpr uint32_t EnginePhysicsBodyKinematic = 2;
// Collision shape: 0 = box, 1 = sphere, 2 = capsule.
constexpr uint32_t EnginePhysicsShapeBox = 0;
constexpr uint32_t EnginePhysicsShapeSphere = 1;
constexpr uint32_t EnginePhysicsShapeCapsule = 2;

struct EnginePhysicsObjectState
{
    uint32_t enabled;    // 0/1: the object takes part in the physics simulation
    uint32_t bodyType;   // EnginePhysicsBody*
    uint32_t shapeType;  // EnginePhysicsShape*
    float friction;      // >= 0
    float restitution;   // >= 0
    float density;       // > 0, dynamic bodies only (mass computation)
};

struct EnginePhysicsObjectApi
{
    uint32_t version; // 1
    void* context;
    // Absent (never configured) objects report enabled = 0 with the remaining defaults.
    uint32_t (ENGINE_MODULE_CALL *read)(void*, uint64_t objectId, EnginePhysicsObjectState*);
    uint32_t (ENGINE_MODULE_CALL *write)(void*, uint64_t objectId, const EnginePhysicsObjectState*);
    // 1 when the engine was built with Jolt physics; settings round-trip regardless.
    uint32_t (ENGINE_MODULE_CALL *available)(void*);
};
typedef int (ENGINE_MODULE_CALL *EngineBindPhysicsObjectFn)(uint32_t, uint32_t, void*, const EnginePhysicsObjectApi*);
