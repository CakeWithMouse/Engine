#include "../../../../Public/MainGame/SpecialGameClasses/PingPong/PongGame.h"
#include "../../../../Public/Components/GameComponents.h"
#include "../../../../Public/MainGame/SpecialGameClasses/PingPong/Ball.h"

void PongGame::AddScore(glm::vec2 ScoreAdded)
{
    Score += ScoreAdded;
    printScore();
}

void PongGame::Draw(ID3D11RasterizerState* RasterState)
{
    Context->ClearState();
    Context->RSSetState(RasterState);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(DisplayPtr->GetWidth());
    viewport.Height = static_cast<float>(DisplayPtr->GetHeight());
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1.0f;

    Context->RSSetViewports(1, &viewport);
    Context->IASetInputLayout(layout);
    Context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set render target
    Context->OMSetRenderTargets(1, &RenderTargetView, nullptr);

    //Change back layout
    static constexpr int SecondsToChangeColor = 10;
    //float AddedRed = 0.7f * (static_cast<int>(TotalTime) % SecondsToChangeColor) / SecondsToChangeColor;
    float color[] = {0.05f, 0.1f, 0.05f, 1.0f};
    Context->ClearRenderTargetView(RenderTargetView, color);

    //UpdateConstantBuffer();
    std::vector<GameComponent*> HasCollision;
    FirstPlayer->UpdateCamera(0.13f);
    for (int i = 0; i < Components.size(); ++i)
    {
        const auto KeyVal = Components.find(std::to_string(i + 1));
        if (KeyVal == Components.end())
        {
            continue;
        }
        GameComponent* Component = KeyVal->second;
        if (Component == nullptr)
        {
            continue;
        }
        if (Component->HasCollison())
        {
            HasCollision.push_back(Component);
        }
        Component->Tick(0.13f);
        Component->Update();
        Component->Render(Context);
    }
    //BallZone
    const auto KeyVal = Components.find("PongBall");
    BallComponent* Component = static_cast<BallComponent*>(KeyVal->second);
    Component->CheckCollisions(HasCollision);
    Component->Tick(0.13f);
    Component->Update();
    Component->Render(Context);

    Context->OMSetRenderTargets(0, nullptr, nullptr);
    EndFrame();
}

void PongGame::printScore()
{
    std::cout << "┌─────────────────┐" << std::endl;
    std::cout << "│    SCORE        │" << std::endl;
    std::cout << "│  " << (int)Score.x << "  :  " << (int)Score.y << "     │" << std::endl;
    std::cout << "└─────────────────┘" << std::endl;
}
