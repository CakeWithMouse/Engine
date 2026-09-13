#include "../../../../../Source/Public/MainGame/SpecialGameClasses/PingPong/Rocket.h"
#include "../../../../../Source/Public/Components/SpecificComponents/CubeComponent.h"
#include "../../../../Public/InputDevice/InputDevice.h"
#include "../../../../Public/MainGame/SpecialGameClasses/PingPong/Ball.h"

RocketComponent::RocketComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) : CubeComponent(pos, rot, scale)
{
}

RocketComponent::RocketComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color) : CubeComponent(
    pos, rot, scale, color)
{
};


void RocketComponent::Tick(float deltaTime)
{
    CubeComponent::Tick(deltaTime);
    InputDevice* InputDevice = GamePtr->GetInputDevice();
    assert(InputDevice);

    float Speed = 1.0f;
    //glm::vec2
    switch (rocketType)
    {
    case Right:
        {
            if (InputDevice->IsKeyDown(Keys::W)) { ComponentPosition.y += Speed * deltaTime; }
            if (InputDevice->IsKeyDown(Keys::S)) { ComponentPosition.y -= Speed * deltaTime; }
            if (InputDevice->IsKeyDown(Keys::A)) { ComponentPosition.x -= Speed * deltaTime; }
            if (InputDevice->IsKeyDown(Keys::D)) { ComponentPosition.x += Speed * deltaTime; }
            break;
        }
    case Left:
        {
            if (InputDevice->IsKeyDown(Keys::Up)) { ComponentPosition.y += Speed * deltaTime; }
            if (InputDevice->IsKeyDown(Keys::Down)) { ComponentPosition.y -= Speed * deltaTime; }
            if (InputDevice->IsKeyDown(Keys::Left)) { ComponentPosition.x -= Speed * deltaTime; }
            if (InputDevice->IsKeyDown(Keys::Right)) { ComponentPosition.x += Speed * deltaTime; }
            break;
        }
    }
    ComponentPosition.y = glm::clamp(ComponentPosition.y,-5.f,5.f);
}

bool RocketComponent::DoSmthWithCollision(GameComponent* AnotherComponent)
{
    assert(AnotherComponent);
    BallComponent* Ball = static_cast<BallComponent*>(AnotherComponent);
    if (Ball == nullptr)
    {
        return false;
    }
    float Distance = Ball->GetCenter().x - GetCenter().x;
    if (abs(Distance) > 0.1f)
    {
        return false;
    }
    float hitPos = (Ball->GetCenter().y - GetCenter().y) / (Lenght / 2.f);
    if (abs(Ball->GetCenter().y - GetCenter().y) > Lenght / 2.f)
    {
        return false;
    }
    
    float maxAngleDegrees = 75.0f;
    float angleRadians = hitPos * (maxAngleDegrees * 3.14159f / 180.0f);
    float directionX = (rocketType == Left) ? -1.0f : 1.0f;
    glm::vec2 currentVelocity = Ball->Velocity;
    float currentSpeed = glm::length(currentVelocity);

    Ball->Speed *= 1.1f;
    Ball->Speed = std::min(currentSpeed, Ball->maxSpeed);
    glm::vec2 newDirection;
    newDirection.x = directionX * cos(angleRadians);
    newDirection.y = sin(angleRadians);
    if (glm::length(newDirection) > 0)
    {
        newDirection = glm::normalize(newDirection);
    }
    Ball->Velocity = newDirection;
    
    return false;
}
