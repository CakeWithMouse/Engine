#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <Engine/Physics/PhysicsSettings.h>

enum class PhysicsBodyType
{
    Static,   // never moves, infinite mass
    Dynamic,  // fully simulated
    Kinematic // moved by gameplay code, pushes dynamic bodies
};

enum class PhysicsShapeType
{
    Box,
    Sphere,
    Capsule
};

// Creation parameters of one physics body. Positions/rotations are world-space.
struct PhysicsBodyDesc
{
    PhysicsBodyType type = PhysicsBodyType::Dynamic;
    PhysicsShapeType shape = PhysicsShapeType::Box;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    // Box: half extents. Sphere: radius. Capsule: radius + half cylinder height.
    glm::vec3 halfExtents = glm::vec3(0.5f);
    float radius = 0.5f;
    float halfHeight = 0.5f;
    float friction = 0.5f;
    float restitution = 0.0f;
    // Used to compute the mass of dynamic bodies.
    float density = 1000.0f;
    bool allowSleeping = true;
};

struct PhysicsRaycastHit
{
    uint32_t body = 0;             // PhysicsWorld body handle, 0 on miss
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    float distance = 0.0f;
};

// World of rigid bodies, simulated by Jolt Physics (fixed-timestep accumulator).
// Public API is Jolt-free: the library types never cross this boundary.
// Threading: all calls on the game/simulation thread.
class PhysicsWorld
{
public:
    PhysicsWorld();
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool Initialize(const PhysicsSettings& settings, std::string& error);
    void Shutdown();
    bool IsInitialized() const;

    // Handles are opaque non-zero ids; 0 means failure/invalid.
    uint32_t CreateBody(const PhysicsBodyDesc& desc);
    void DestroyBody(uint32_t handle);
    bool IsValid(uint32_t handle) const;
    uint32_t GetBodyCount() const;

    glm::vec3 GetPosition(uint32_t handle) const;
    glm::quat GetRotation(uint32_t handle) const;
    void SetPositionAndRotation(uint32_t handle, const glm::vec3& position, const glm::quat& rotation);

    glm::vec3 GetLinearVelocity(uint32_t handle) const;
    void SetLinearVelocity(uint32_t handle, const glm::vec3& velocity);
    void AddImpulse(uint32_t handle, const glm::vec3& impulse);
    void SetActive(uint32_t handle, bool active);

    // Advances the simulation by deltaTime (real seconds), using the fixed
    // timestep/max substeps from the settings.
    void Update(float deltaTime);

    const PhysicsSettings& GetSettings() const;
    // Re-reads gravity and solver iteration counts from the settings.
    void ApplySettings(const PhysicsSettings& settings);

    // First hit along the ray; direction must be normalized. Returns false on miss.
    bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                 PhysicsRaycastHit& hit) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    PhysicsSettings settings_;
    float accumulator_ = 0.0f;
};
