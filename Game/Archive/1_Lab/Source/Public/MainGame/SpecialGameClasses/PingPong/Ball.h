#pragma once
#include "../../../../../Source/Public/Components/SpecificComponents/CubeComponent.h"


class BallComponent : public CubeComponent
{
public:
    BallComponent(){};
    BallComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    BallComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale,glm::vec4 color);

    void CheckCollisions(std::vector<GameComponent*> ComponentsWithCollision);
    void Tick(float deltaTime) override;

    void Restart();
    glm::vec2 Velocity{1.f,0.f};
    float Speed = 0.6f;
    float maxSpeed = 3.f;
    //glm::vec2 CubeBB{1.f,1.f};
private:
    GameComponentNames::GeometryType ObjectType =  GameComponentNames::GeometryType::Object3D;
};

