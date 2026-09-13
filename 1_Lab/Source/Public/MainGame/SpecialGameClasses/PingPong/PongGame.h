#pragma once
#include "../../BaseGameClass/Game.h"

class PongGame : public Game
{
public:
    PongGame() : Game()
    {
    };
    void AddScore(glm::vec2 ScoreAdded);

protected:
    virtual void Draw(ID3D11RasterizerState* RasterState) override;
    void printScore();
    glm::vec2 Score{0, 0};
};
