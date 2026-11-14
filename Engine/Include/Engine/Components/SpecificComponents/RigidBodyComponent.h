#pragma once
#include <Engine/Components/GameComponents.h>
#include <Engine/Physics/PhysicsWorld.h>

// Convenience component that carries only physics state: every GameComponent already
// owns the rigid body machinery (see SetPhysicsEnabled/SetPhysicsBodyType in the base
// class), this subclass just enables it by default and is never drawn itself.
class RigidBodyComponent : public GameComponent
{
public:
    RigidBodyComponent();
    ~RigidBodyComponent() override;

    // Not drawable: the component only carries physics state.
    bool IsRenderable() const override { return false; }

    void SetBodyType(PhysicsBodyType type) { SetPhysicsBodyType(type); }
    PhysicsBodyType GetBodyType() const { return GetPhysicsBodyType(); }
    void SetShapeType(PhysicsShapeType shape) { SetPhysicsShapeType(shape); }
    PhysicsShapeType GetShapeType() const { return GetPhysicsShapeType(); }

    // Dynamic bodies only: density used to compute the mass.
    void SetDensity(float density) { SetPhysicsDensity(density); }
    float GetDensity() const { return GetPhysicsDensity(); }
    void SetFriction(float friction) { SetPhysicsFriction(friction); }
    float GetFriction() const { return GetPhysicsFriction(); }
    void SetRestitution(float restitution) { SetPhysicsRestitution(restitution); }
    float GetRestitution() const { return GetPhysicsRestitution(); }

    // false disables physics for this component entirely.
    void SetPhysicsEnabled(bool enabled) { GameComponent::SetPhysicsEnabled(enabled); }
    bool IsPhysicsEnabled() const { return GameComponent::IsPhysicsEnabled(); }

    bool HasPhysicsBody() const { return GameComponent::HasPhysicsBody(); }
    glm::vec3 GetLinearVelocity() const { return GetPhysicsLinearVelocity(); }
    void SetLinearVelocity(const glm::vec3& velocity) { SetPhysicsLinearVelocity(velocity); }
    void AddImpulse(const glm::vec3& impulse) { AddPhysicsImpulse(impulse); }
};
