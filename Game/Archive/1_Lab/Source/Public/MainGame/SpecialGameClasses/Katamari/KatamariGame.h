#pragma once
#include "../../BaseGameClass/Game.h"

/** Katamari scene: the ball collects objects with collision. Rendering is shared with Game. */
class KatamariGame : public Game
{
public:
    KatamariGame() {}
    virtual void AfterInitialize() override;

protected:
    void PreUpdate(float deltaTime) override;
    std::array<float, 4> GetClearColor() const override { return {0.201f, 0.201f, 0.901f, 1.0f}; }

    std::vector<GameComponent*> HasCollision;
};
