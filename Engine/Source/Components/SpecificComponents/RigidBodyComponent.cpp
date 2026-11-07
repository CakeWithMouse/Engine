#include <Engine/Components/SpecificComponents/RigidBodyComponent.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <algorithm>
#include <cmath>

RigidBodyComponent::RigidBodyComponent() = default;
RigidBodyComponent::~RigidBodyComponent()
{
    DestroyBody();
}

void RigidBodyComponent::BeginPlay()
{
    CreateBody();
}

void RigidBodyComponent::Tick(float)
{
    if (!bodyHandle_) return;
    Game* game = GetGame();
    PhysicsWorld* world = game ? game->GetPhysicsWorld() : nullptr;
    if (!world || !world->IsValid(bodyHandle_)) return;
    if (bodyType_ == PhysicsBodyType::Dynamic)
    {
        // Physics drives the visual transform.
        SetPosition(world->GetPosition(bodyHandle_));
        SetRotationQuat(world->GetRotation(bodyHandle_));
    }
    else
    {
        // Gameplay drives the physics body.
        world->SetPositionAndRotation(bodyHandle_, GetWorldPosition(), GetWorldRotationQuat());
    }
}

void RigidBodyComponent::SetBodyType(PhysicsBodyType type)
{
    if (bodyType_ == type) return;
    DestroyBody();
    bodyType_ = type;
    CreateBody();
}

void RigidBodyComponent::SetShapeType(PhysicsShapeType shape)
{
    if (shape_ == shape) return;
    DestroyBody();
    shape_ = shape;
    CreateBody();
}

void RigidBodyComponent::SetPhysicsEnabled(bool enabled)
{
    if (physicsEnabled_ == enabled) return;
    physicsEnabled_ = enabled;
    if (!enabled) DestroyBody();
    else CreateBody();
}

glm::vec3 RigidBodyComponent::GetLinearVelocity() const
{
    Game* game = GetGame();
    PhysicsWorld* world = game ? game->GetPhysicsWorld() : nullptr;
    if (!world || !bodyHandle_) return glm::vec3(0.0f);
    return world->GetLinearVelocity(bodyHandle_);
}

void RigidBodyComponent::SetLinearVelocity(const glm::vec3& velocity)
{
    Game* game = GetGame();
    PhysicsWorld* world = game ? game->GetPhysicsWorld() : nullptr;
    if (world && bodyHandle_) world->SetLinearVelocity(bodyHandle_, velocity);
}

void RigidBodyComponent::AddImpulse(const glm::vec3& impulse)
{
    Game* game = GetGame();
    PhysicsWorld* world = game ? game->GetPhysicsWorld() : nullptr;
    if (world && bodyHandle_) world->AddImpulse(bodyHandle_, impulse);
}

void RigidBodyComponent::CreateBody()
{
    if (bodyHandle_ || !physicsEnabled_) return;
    Game* game = GetGame();
    PhysicsWorld* world = game ? game->GetPhysicsWorld() : nullptr;
    if (!world) return;

    PhysicsBodyDesc desc;
    desc.type = bodyType_;
    desc.shape = shape_;
    desc.position = GetWorldPosition();
    desc.rotation = GetWorldRotationQuat();
    const glm::vec3 scale = glm::abs(GetScale());
    desc.halfExtents = 0.5f * scale;
    desc.radius = 0.5f * std::max({scale.x, scale.y, scale.z});
    desc.halfHeight = 0.5f * scale.y;
    desc.density = density_;
    desc.friction = friction_;
    desc.restitution = restitution_;
    bodyHandle_ = world->CreateBody(desc);
}

void RigidBodyComponent::DestroyBody()
{
    if (!bodyHandle_) return;
    Game* game = GetGame();
    PhysicsWorld* world = game ? game->GetPhysicsWorld() : nullptr;
    if (world) world->DestroyBody(bodyHandle_);
    bodyHandle_ = 0;
}
