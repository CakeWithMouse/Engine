#pragma once
#include <Engine/Components/GameComponents.h>

namespace RuntimeExample
{
    // Minimal CPU-only user component; no rendering/resource assumptions.
    ENGINE_COMPONENT("Moving object")
    class Mover final : public GameComponent
    {
    public:
        void Tick(float dt) override
        {
            SetPosition(GetLocalPosition() + glm::vec3(dt, 0.0f, 0.0f));
        }
    };
}
