#include "../../../../Public/MainGame/SpecialGameClasses/SunSystem/SunGame.h"
#include "../../../../Public/Components/GameComponents.h"
#include "../../../../Public/MainGame/SpecialGameClasses/PingPong/Ball.h"

void SunGame::Draw(ID3D11RasterizerState* RasterState)
{
    DrawForward(RasterState);
}

void SunGame::DrawForward(ID3D11RasterizerState* RasterState)
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
    Context->OMSetRenderTargets(1, &RenderTargetView, depthStencilView.Get());

    //Change back layout
    static constexpr int SecondsToChangeColor = 10;
    //float AddedRed = 0.7f * (static_cast<int>(TotalTime) % SecondsToChangeColor) / SecondsToChangeColor;
    float color[] = {0.001f, 0.001f, 0.001f, 1.0f};
    Context->ClearRenderTargetView(RenderTargetView, color);

    std::vector<GameComponent*> HasCollision;
    FirstPlayer->UpdateCamera(0.13f);

    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second;
        if (Component == nullptr)
        {
            continue;
        }

        if (!Component->IsSkybox() && Component->HasCollison())
        {
            HasCollision.push_back(Component);
        }

        if (!Component->IsSkybox() && InputDevicePtr->IsKeyDown(Keys::Space))
        {
            Component->Tick(0.f);
        }
        else
        {
            Component->Tick(0.13f);
        }
        Component->Update();
    }

    RenderShadowMaps();
    Context->RSSetState(RasterState);
    Context->RSSetViewports(1, &viewport);
    Context->OMSetRenderTargets(1, &RenderTargetView, depthStencilView.Get());

    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second;
        if (Component == nullptr || !Component->IsSkybox())
        {
            continue;
        }

        Component->Render(Context);
    }

    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second;
        if (Component == nullptr || Component->IsSkybox())
        {
            continue;
        }

        float blendFactor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        if (Component->HasOpacity())
        {
            Context->OMSetBlendState(transparentBlendState.Get(), blendFactor, 0xffffffff);
        }

        Component->Render(Context);

        if (Component->HasOpacity())
        {
            Context->OMSetBlendState(opaqueBlendState.Get(), blendFactor, 0xffffffff);
        }
    }

    Context->OMSetRenderTargets(0, nullptr, depthStencilView.Get());
    EndFrame();
    
    if (InputDevicePtr)
    {
        InputDevicePtr->MouseOffset = glm::vec2(0.0f, 0.0f);
    }
}

void SunGame::DrawDeffered(ID3D11RasterizerState* RasterState)
{
    if (!InitDeferredResources())
    {
        DrawForward(RasterState);
        return;
    }

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
    Context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    std::vector<GameComponent*> HasCollision;
    FirstPlayer->UpdateCamera(0.13f);

    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second;
        if (Component == nullptr)
        {
            continue;
        }

        if (!Component->IsSkybox() && Component->HasCollison())
        {
            HasCollision.push_back(Component);
        }

        if (!Component->IsSkybox() && InputDevicePtr->IsKeyDown(Keys::Space))
        {
            Component->Tick(0.f);
        }
        else
        {
            Component->Tick(0.13f);
        }
        Component->Update();
    }

    RenderShadowMaps();
    Context->RSSetState(RasterState);
    Context->RSSetViewports(1, &viewport);

    ID3D11RenderTargetView* gBufferTargets[3] =
    {
        gBufferAlbedoRTV.Get(),
        gBufferNormalRTV.Get(),
        gBufferMaterialRTV.Get()
    };
    Context->OMSetRenderTargets(3, gBufferTargets, depthStencilView.Get());

    const float clearAlbedo[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float clearNormal[4] = {0.5f, 0.5f, 1.0f, 1.0f};
    const float clearMaterial[4] = {0.5f, 0.0f, 0.0f, 1.0f};
    Context->ClearRenderTargetView(gBufferAlbedoRTV.Get(), clearAlbedo);
    Context->ClearRenderTargetView(gBufferNormalRTV.Get(), clearNormal);
    Context->ClearRenderTargetView(gBufferMaterialRTV.Get(), clearMaterial);
    Context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    Context->IASetInputLayout(layout);
    float blendFactor[] = {0.0f, 0.0f, 0.0f, 0.0f};
    Context->OMSetBlendState(opaqueBlendState.Get(), blendFactor, 0xffffffff);

    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second;
        if (Component == nullptr || Component->IsSkybox() || Component->HasOpacity())
        {
            continue;
        }

        Component->Render(Context);
    }

    Context->OMSetRenderTargets(1, &RenderTargetView, depthStencilView.Get());
    Context->ClearRenderTargetView(RenderTargetView, clearAlbedo);
    RenderDeferredLightingPass(RasterState);

    Context->RSSetState(RasterState);
    Context->RSSetViewports(1, &viewport);
    Context->IASetInputLayout(layout);
    Context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->OMSetRenderTargets(1, &RenderTargetView, depthStencilView.Get());

    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second;
        if (Component == nullptr || !Component->IsSkybox())
        {
            continue;
        }

        Component->Render(Context);
    }

    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second;
        if (Component == nullptr || Component->IsSkybox() || !Component->HasOpacity())
        {
            continue;
        }

        Context->OMSetBlendState(transparentBlendState.Get(), blendFactor, 0xffffffff);
        Component->Render(Context);
        Context->OMSetBlendState(opaqueBlendState.Get(), blendFactor, 0xffffffff);
    }

    RenderDeferredDebugOverlay(RasterState);

    Context->OMSetRenderTargets(0, nullptr, depthStencilView.Get());
    EndFrame();

    if (InputDevicePtr)
    {
        InputDevicePtr->MouseOffset = glm::vec2(0.0f, 0.0f);
    }
}
