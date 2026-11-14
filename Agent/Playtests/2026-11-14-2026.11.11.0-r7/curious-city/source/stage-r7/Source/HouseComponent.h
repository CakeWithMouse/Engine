#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/Components/GameComponents.h>

ENGINE_COMPONENT("City House", "CuriousCity.House")
class HouseComponent final : public GameComponent
{
public:
    HouseComponent();
};
