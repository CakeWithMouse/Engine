#include "HouseComponent.h"
#include <Engine/Render/MeshPrimitives.h>

HouseComponent::HouseComponent()
{
    SetPrimitive(MeshPrimitives::Cube());
    SetScale({1.2f, 1.5f, 1.2f});
    SetColor({0.78f, 0.43f, 0.25f, 1.0f});
}
