#include "RotatingCube.h"
#include <cmath>

RotatingCube::RotatingCube()
{
    InitCube();
    SetColor({1.0f, 0.65f, 0.2f, 1.0f});
}

void RotatingCube::Tick(float deltaTime)
{
    CubeComponent::Tick(deltaTime);
    auto rotation = GetRotation();
    // SetRotation uses degrees. deltaTime is scaled simulation time.
    rotation.y = std::fmod(rotation.y + 90.0f * deltaTime, 360.0f);
    SetRotation(rotation);
}
