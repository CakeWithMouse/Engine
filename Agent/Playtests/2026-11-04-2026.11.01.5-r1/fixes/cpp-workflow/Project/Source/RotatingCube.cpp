#include "RotatingCube.h"
#include <cmath>
#include <nlohmann/json.hpp>

RotatingCube::RotatingCube()
{

    SetColor({1.0f, 0.65f, 0.2f, 1.0f});
}

void RotatingCube::Tick(float deltaTime)
{
    CubeComponent::Tick(deltaTime);
    auto rotation = GetRotation();
    // SetRotation uses degrees. deltaTime is scaled simulation time.
    if (enabled) rotation.y = std::fmod(rotation.y + speed * deltaTime, 360.0f);
    SetRotation(rotation);
}
