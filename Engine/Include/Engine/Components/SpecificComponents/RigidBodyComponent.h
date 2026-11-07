#pragma once
#include <Engine/Components/GameComponents.h>
#include <Engine/Physics/PhysicsWorld.h>

// Attaches the component's world transform to a body in the Game physics world.
// Dynamic bodies drive the component transform each frame; static and kinematic
// bodies follow the component transform instead. Shape dimensions are derived
// from the component scale (box: half extents, sphere: max axis, capsule: y height).
class RigidBodyComponent : public GameComponent
{
public:
    RigidBodyComponent();
    ~RigidBodyComponent() override;

    void BeginPlay() override;
    void Tick(float deltaTime) override;

    // Not drawable: the component only carries physics state.
    bool IsRenderable() const override { return false; }

    void SetBodyType(PhysicsBodyType type);
    PhysicsBodyType GetBodyType() const { return bodyType_; }
    void SetShapeType(PhysicsShapeType shape);
    PhysicsShapeType GetShapeType() const { return shape_; }

    // Dynamic bodies only: density used to compute the mass.
    void SetDensity(float density) { density_ = density > 0.0f ? density : density_; }
    float GetDensity() const { return density_; }
    void SetFriction(float friction) { friction_ = friction; }
    float GetFriction() const { return friction_; }
    void SetRestitution(float restitution) { restitution_ = restitution; }
    float GetRestitution() const { return restitution_; }

    // false disables physics for this component entirely.
    void SetPhysicsEnabled(bool enabled);
    bool IsPhysicsEnabled() const { return physicsEnabled_; }

    bool HasPhysicsBody() const { return bodyHandle_ != 0; }
    glm::vec3 GetLinearVelocity() const;
    void SetLinearVelocity(const glm::vec3& velocity);
    void AddImpulse(const glm::vec3& impulse);

private:
    void CreateBody();
    void DestroyBody();

    uint32_t bodyHandle_ = 0;
    PhysicsBodyType bodyType_ = PhysicsBodyType::Dynamic;
    PhysicsShapeType shape_ = PhysicsShapeType::Box;
    float density_ = 1000.0f;
    float friction_ = 0.5f;
    float restitution_ = 0.0f;
    bool physicsEnabled_ = true;
};
