#pragma once
#include "../../BaseGameClass/Game.h"

/** Second lab: walls/rackets named "1".."N" plus "PongBall", no depth testing. */
class PongGame : public Game
{
public:
    PongGame() : Game() { RenderingType = Custom; }
    void AddScore(glm::vec2 ScoreAdded);

protected:
    void TickComponents(float deltaTime) override;
    virtual void Draw() override;
    std::array<float, 4> GetClearColor() const override { return {0.05f, 0.1f, 0.05f, 1.0f}; }
    void printScore();
    glm::vec2 Score{0, 0};
};
