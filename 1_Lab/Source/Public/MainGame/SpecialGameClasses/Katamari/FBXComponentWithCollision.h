#pragma once

#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "../../../../Public/MainGame/SpecialGameClasses/Katamari/FBXComponentWithCollision.h"
#include "../../../../Public/Components/SpecificComponents/FBXComponent.h"

class FBXComponentCollision : public FBXComponent
{
public:
    FBXComponentCollision();
    FBXComponentCollision(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    FBXComponentCollision(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);

    virtual void Tick(float deltaTime) override;

private:
};