#include "../../../../../Source/Public/MainGame/SpecialGameClasses/PingPong/Ball.h"


class CubeComponent;

BallComponent::BallComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) : ::CubeComponent(pos, rot, scale)
{
}

BallComponent::BallComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color): ::CubeComponent(
    pos, rot, scale, color)
{
}

void BallComponent::CheckCollisions(std::vector<GameComponent*> ComponentsWithCollision)
{
    for(const auto& Component : ComponentsWithCollision)
    {
        if(Component->DoSmthWithCollision(this))
        {
            break;
        }
    }
};

namespace Ball
{
    constexpr float left = -5.f;
    constexpr float Right = left * -1;
    constexpr float top = -5.f;
    constexpr float down = top * -1;
}
void BallComponent::Tick(float deltaTime)
{
    using namespace Ball;
    CubeComponent::Tick(deltaTime);

    static float angle = 0;
    angle += deltaTime;
    float ResultX = ComponentPosition.x + Velocity.x * Speed * deltaTime;
    float ResultY = ComponentPosition.y + Velocity.y * Speed * deltaTime;
    if (ResultX <= left)
    {
        //ResultX = left;
        //Velocity.x *= -1.4;
    }
    if (ResultX >= Right)
    {
        //ResultX = Right;
        //Velocity.x *= -0.5;
    }
    if (ResultY <= top)
    {
        ResultY = top;
        Velocity.y *= -1.f;
    }
    if (ResultY >= down)
    {
        ResultY = down;
        Velocity.y *=  -1.f;
    }
    ComponentPosition.x = ResultX;
    ComponentPosition.y = ResultY;
    ComponentRotation.y += deltaTime * 20.0f * Speed;
    MarkTransformDirty();
    //ComponentRotation.x += deltaTime * 45.0f;
    /*if (ComponentRotation.y >= 360.0f)
    {
        ComponentRotation.y -= 360.0f;
    }*/
}

void BallComponent::Restart()
{
    if(rand() %2)
    {
        Velocity = {1.f,0.f};
    }else
    {
        Velocity = {-1.f,0.f};
    }
    
    Speed = 0.6f;
    ComponentPosition.x = 0;
    ComponentPosition.y = 0;
    MarkTransformDirty();
}
