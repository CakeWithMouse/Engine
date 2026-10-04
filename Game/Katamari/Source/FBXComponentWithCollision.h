#pragma once

#include <string>
#include <vector>
#include <Engine/Components/SpecificComponents/FBXComponent.h>

class FBXComponentCollision : public FBXComponent
{
public:
    FBXComponentCollision();
    FBXComponentCollision(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    FBXComponentCollision(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);

    virtual void Tick(float deltaTime) override;

private:
};
