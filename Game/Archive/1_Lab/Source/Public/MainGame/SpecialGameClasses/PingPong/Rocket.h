#pragma once
#include <iostream>
#include "../../../../../Source/Public/Components/GameComponents.h"
#include "../../../../../Source/Public/Components/SpecificComponents/CubeComponent.h"

enum RocketType
{
    Left,
    Right
};

class RocketComponent : public CubeComponent
{
public:
    RocketComponent(){};
    RocketComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    RocketComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);

    void Tick(float deltaTime) override;
    void SetRocketType(RocketType Type) { rocketType = Type; }
    bool DoSmthWithCollision(GameComponent* AnotherComponent) override;
protected:
    
private:
    float Lenght = 2.4f;
    RocketType rocketType;
    GameComponentNames::GeometryType ObjectType = GameComponentNames::GeometryType::Object3D;
};
