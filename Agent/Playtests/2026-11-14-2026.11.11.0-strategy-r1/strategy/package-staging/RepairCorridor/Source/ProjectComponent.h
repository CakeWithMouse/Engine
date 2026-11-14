#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include "Engine/Components/GameComponents.h"
ENGINE_COMPONENT("ProjectComponent")
class ProjectComponent : public GameComponent
{
public:
    ProjectComponent() = default;
    void BeginPlay() override;
    void Tick(float deltaTime) override;
};
