#include "../../../Public/MainGame/BaseGameClass/BaseGame.h"
#include "../../../Public/Components/GameComponents.h"

void BaseGame::Draw(ID3D11RasterizerState* RasterState)
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
    float color[] = {0.f, 0.f, 0.f, 1.0f};
    Context->ClearRenderTargetView(RenderTargetView, color);

    FirstPlayer->UpdateCamera(0.13f);
    for (int i = 0; i < Components.size(); ++i)
    {
        GameComponent* Component = Components[std::to_string(i + 1)];
        if (Component == nullptr)
        {
            continue;
        }
        Component->Tick(0.13f);
        Component->Update();
        Component->Render(Context);
    }

    Context->OMSetRenderTargets(0, nullptr, nullptr);
    EndFrame();
}
