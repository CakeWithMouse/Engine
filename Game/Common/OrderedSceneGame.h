#pragma once
#include <Engine/MainGame/BaseGameClass/Game.h>

/** Shared examples: components named "1".."N" drawn in order without depth testing. */
class OrderedSceneGame : public Game
{
public:
    OrderedSceneGame() : Game() { RenderingType = Custom; }

protected:
    virtual void Draw() override;
    std::array<float, 4> GetClearColor() const override { return {0.0f, 0.0f, 0.0f, 1.0f}; }
};
