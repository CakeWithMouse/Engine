#include <Engine/Components/SpecificComponents/RigidBodyComponent.h>

RigidBodyComponent::RigidBodyComponent()
{
    // The base class keeps the body in sync during Play; enabling physics marks the
    // component as physics-configured so the settings persist into the scene document.
    SetPhysicsEnabled(true);
}

RigidBodyComponent::~RigidBodyComponent() = default;
