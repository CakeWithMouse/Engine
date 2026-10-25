#pragma once
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <cmath>

namespace RuntimeExample
{
    ENGINE_COMPONENT("Moving cube")
    class MovingCube final : public CubeComponent
    {
    public:
        MovingCube() { InitCube(); }
        void Tick(float dt) override
        {
            elapsed += dt;
            SetPosition({std::sin(elapsed), GetLocalPosition().y, GetLocalPosition().z});
        }
    private:
        float elapsed = 0;
    };
}
