#pragma once
#include "../SpecificComponents/CubeComponent.h"

class SkyboxComponent : public CubeComponent
{
public:
    SkyboxComponent();
    SkyboxComponent(glm::vec3 scale, glm::vec4 color = glm::vec4(1.0f));

    virtual void Tick(float deltaTime) override;
};
