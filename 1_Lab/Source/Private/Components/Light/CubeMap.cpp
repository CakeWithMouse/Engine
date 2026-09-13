#include "../../../Public/Components/Light/CubeMap.h"


void SkyboxComponent::Tick(float deltaTime)
{
    CubeComponent::Tick(deltaTime);

    if (GamePtr && GamePtr->GetPlayer())
    {
        ComponentPosition = GamePtr->GetPlayer()->GetPosition();
        MarkTransformDirty();
    }
}


SkyboxComponent::SkyboxComponent()
    : CubeComponent(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(2500.0f), glm::vec4(1.0f))
{
    bIsSkybox = true;
}

SkyboxComponent::SkyboxComponent(glm::vec3 scale, glm::vec4 color)
    : CubeComponent(glm::vec3(0.0f), glm::vec3(0.0f), scale, color)
{
    bIsSkybox = true;
}
