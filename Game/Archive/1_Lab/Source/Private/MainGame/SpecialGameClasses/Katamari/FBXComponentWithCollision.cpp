#include "../../../../Public/MainGame/SpecialGameClasses/Katamari/FBXComponentWithCollision.h"
#include <iostream>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

FBXComponentCollision::FBXComponentCollision() : FBXComponent()
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
}

FBXComponentCollision::FBXComponentCollision(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) 
    : FBXComponent(pos, rot, scale)
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
}

FBXComponentCollision::FBXComponentCollision(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color) 
    : FBXComponent(pos, rot, scale, color)
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
}

void FBXComponentCollision::Tick(float deltaTime)
{
    FBXComponent::Tick(deltaTime);
}

