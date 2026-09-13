#pragma once
#include "../../../../../Source/Public/Components/GameComponents.h"
#include "../../../../../Source/Public/Components/SpecificComponents/CubeComponent.h"

enum WallType
{
    LeftWall,
    RightWall
};

class Wall : public CubeComponent
{
public:
    Wall()
    {
    };
    Wall(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    Wall(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);
    void SetWallType(WallType WallType) { wallType = WallType; }

    void Tick(float deltaTime) override
    {
    };
    bool DoSmthWithCollision(GameComponent* AnotherComponent) override;

private:
    WallType wallType;
    GameComponentNames::GeometryType ObjectType = GameComponentNames::GeometryType::Object3D;
};
