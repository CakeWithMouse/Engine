#pragma once
#include "../../BaseGameClass/Game.h"

/** Solar system scene. Uses the shared forward/deferred renderer of Game. */
class SunGame : public Game
{
public:
    SunGame() : Game() {}

protected:
    std::array<float, 4> GetClearColor() const override { return {0.001f, 0.001f, 0.001f, 1.0f}; }
};
