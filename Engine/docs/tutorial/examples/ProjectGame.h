#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/MainGame/BaseGameClass/Game.h>

ENGINE_GAME("ProjectGame")
class ProjectGame : public Game
{
public:
    void AfterInitialize() override;
    void PreUpdate(float deltaTime) override;
    void TickComponents(float deltaTime) override;
};
