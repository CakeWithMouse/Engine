#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/InputDevice/InputActions.h>

ENGINE_COMPONENT("City Controls", "CuriousCity.Interaction")
class CityInteractionComponent final : public GameComponent
{
public:
    void BeginPlay() override;
private:
    Input::Connection placeConnection;
    Input::Scope placeScope;
};
