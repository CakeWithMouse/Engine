#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/PropertyMacros.h>

ENGINE_COMPONENT("RotatingCube")
class RotatingCube : public CubeComponent
{
public:
    RotatingCube();
    void Tick(float deltaTime) override;
    ENGINE_GENERATED_PROPERTIES(CubeComponent)
private:
    ENGINE_PROPERTY("Speed (degrees/sec)",-720,720)
    float speed = 90.0f;
    ENGINE_PROPERTY("Enabled")
    bool enabled = true;
};