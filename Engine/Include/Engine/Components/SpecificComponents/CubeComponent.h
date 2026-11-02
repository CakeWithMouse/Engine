#pragma once
#include <iostream>
#include <Engine/Components/GameComponents.h>

class CubeComponent : public GameComponent
{
public:
    CubeComponent();
    CubeComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    CubeComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);
    
    // @engine-internal-begin
private:
    virtual void InitCube();
public:
    // @engine-internal-end
    
    void Tick(float deltaTime) override;
    
private:
    GameComponentNames::GeometryType ObjectType =  GameComponentNames::GeometryType::Object3D;
};

