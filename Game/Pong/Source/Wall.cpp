#include "Wall.h"
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include "Ball.h"
#include "PongGame.h"

Wall::Wall(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) : CubeComponent(pos, rot, scale)
{
}

Wall::Wall(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color) : CubeComponent(
    pos, rot, scale, color)
{
};



bool Wall::DoSmthWithCollision(GameComponent* AnotherComponent)
{
    assert(AnotherComponent);
    BallComponent* Ball = static_cast<BallComponent*>(AnotherComponent);
    if (Ball == nullptr)
    {
        return false;
    }
    float Distance = Ball->GetCenter().x - GetCenter().x;
    if (abs(Distance) > 0.3f)
    {
        return false;
    }
    glm::vec2 Score = wallType == LeftWall ? glm::vec2{0,1} : glm::vec2{0,1};
    static_cast<PongGame*>(GamePtr)->AddScore(Score);
    Ball->Restart();
    return false;
}