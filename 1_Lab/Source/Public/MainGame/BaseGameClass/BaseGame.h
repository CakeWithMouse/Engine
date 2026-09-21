#pragma once
#include "Game.h"

/** First lab: components named "1".."N" drawn in order without depth testing. */
class BaseGame : public Game
{
public:
    BaseGame() : Game() { RenderingType = Custom; }

protected:
    virtual void Draw() override;
    std::array<float, 4> GetClearColor() const override { return {0.0f, 0.0f, 0.0f, 1.0f}; }
};
