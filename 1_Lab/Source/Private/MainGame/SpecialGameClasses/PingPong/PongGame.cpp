#include "../../../../Public/MainGame/SpecialGameClasses/PingPong/PongGame.h"
#include "../../../../Public/Components/GameComponents.h"
#include "../../../../Public/MainGame/SpecialGameClasses/PingPong/Ball.h"

void PongGame::AddScore(glm::vec2 ScoreAdded)
{
    Score += ScoreAdded;
    printScore();
}

void PongGame::TickComponents(float deltaTime)
{
    // Walls and rackets move first, then the ball reacts to their new positions.
    std::vector<GameComponent*> HasCollision;
    for (size_t i = 0; i < Components.size(); ++i)
    {
        GameComponent* Component = FindComponent(std::to_string(i + 1));
        if (Component == nullptr)
        {
            continue;
        }
        if (Component->HasCollison())
        {
            HasCollision.push_back(Component);
        }
        Component->Tick(deltaTime);
    }

    BallComponent* Ball = dynamic_cast<BallComponent*>(FindComponent("PongBall"));
    if (Ball != nullptr)
    {
        Ball->CheckCollisions(HasCollision);
        Ball->Tick(deltaTime);
    }
}

void PongGame::Draw()
{
    BeginMainPass(false);

    auto drawComponent = [this](GameComponent* Component)
    {
        if (Component != nullptr && BindComponentShaders(Component, ShaderCompileVariant::Default))
        {
            Component->Render(Context.Get());
        }
    };

    for (size_t i = 0; i < Components.size(); ++i)
    {
        drawComponent(FindComponent(std::to_string(i + 1)));
    }
    drawComponent(FindComponent("PongBall"));
}

void PongGame::printScore()
{
    std::cout << "┌─────────────────┐" << std::endl;
    std::cout << "│    SCORE        │" << std::endl;
    std::cout << "│  " << (int)Score.x << "  :  " << (int)Score.y << "     │" << std::endl;
    std::cout << "└─────────────────┘" << std::endl;
}
