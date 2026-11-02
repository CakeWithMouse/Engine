#pragma once
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/PropertyMacros.h>
#include <string>

// Put this header in Source/. The SDK generator provides the property methods.
ENGINE_COMPONENT("Reference cube", "Reference.Cube")
class ReferenceComponent : public CubeComponent
{
public:
    ENGINE_GENERATED_PROPERTIES(CubeComponent)

    ReferenceComponent() { }

    void Tick(float deltaTime) override
    {
        CubeComponent::Tick(deltaTime);
        elapsed += deltaTime;
        ++ticks;
        if (enabled) {
            yaw += speed * deltaTime;
            SetRotation({0.0f, yaw, 0.0f});
        }
    }

private:
    // These three fields are editable and saved by the generated codec.
    ENGINE_PROPERTY("Speed, degrees/s", -720, 720)
    float speed = 90.0f;

    ENGINE_PROPERTY("Enabled")
    bool enabled = true;

    ENGINE_PROPERTY("Caption")
    std::string caption = "Cube";

    // These three fields are ordinary C++ state, absent from the inspector/save.
    float elapsed = 0.0f;
    int ticks = 0;
    float yaw = 0.0f;
};
