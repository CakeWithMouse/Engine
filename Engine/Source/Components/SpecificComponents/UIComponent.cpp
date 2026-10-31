#include <Engine/Components/SpecificComponents/UIComponent.h>

#include <algorithm>
#include <iostream>
#include <Engine/Display/Display.h>
#include <Engine/Resources/Paths.h>

namespace
{
    constexpr UINT UIVertexStride = sizeof(UIVertex);
}

UIComponent::UIComponent()
{
    const std::string shader = EnginePaths::Shader("UI.hlsl").generic_u8string();
    SetShaderNames(shader, shader);
}

UIComponent::~UIComponent() = default;

UIPanel* UIComponent::AddPanel(const UIRect& rect, const glm::vec4& color)
{
    auto panel = std::make_unique<UIPanel>();
    panel->Rect = rect;
    panel->Color = color;
    return Add(std::move(panel));
}

UILabel* UIComponent::AddLabel(const UIRect& rect, const std::string& utf8, const glm::vec4& color)
{
    auto label = std::make_unique<UILabel>();
    label->Rect = rect;
    label->Color = color;
    label->SetText(utf8);
    return Add(std::move(label));
}

UIBar* UIComponent::AddBar(const UIRect& rect)
{
    auto bar = std::make_unique<UIBar>();
    bar->Rect = rect;
    return Add(std::move(bar));
}

void UIComponent::SetFont(const FontAtlasSettings& settings)
{
    FontSettings = settings;
    bFontBuildFailed = false;
}

bool UIComponent::EnsureFont(ID3D11Device* device)
{
    if (Font.IsValid())
    {
        return true;
    }
    if (bFontBuildFailed || device == nullptr)
    {
        return false;
    }
    if (!Font.Build(device, FontSettings))
    {
        std::cout << "UIComponent: no font, the interface will not be drawn." << std::endl;
        bFontBuildFailed = true;
        return false;
    }
    return true;
}

void UIComponent::CreateBuffers(ID3D11Device* Device)
{
    if (Device == nullptr)
    {
        return;
    }

    EnsureFont(Device);

    if (!ConstantBuffer)
    {
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = sizeof(UIConstants);
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Device->CreateBuffer(&desc, nullptr, ConstantBuffer.GetAddressOf());
    }

    if (!Sampler)
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        Device->CreateSamplerState(&desc, Sampler.GetAddressOf());
    }

    if (!BlendState)
    {
        D3D11_BLEND_DESC desc = {};
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        Device->CreateBlendState(&desc, BlendState.GetAddressOf());
    }

    if (!DepthState)
    {
        // The interface sits on top of the finished frame: no test, no write.
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = FALSE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        Device->CreateDepthStencilState(&desc, DepthState.GetAddressOf());
    }

    if (!RasterState)
    {
        CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
        desc.CullMode = D3D11_CULL_NONE;
        desc.DepthClipEnable = FALSE;
        Device->CreateRasterizerState(&desc, RasterState.GetAddressOf());
    }
}

void UIComponent::BuildVertices(float screenWidth, float screenHeight)
{
    DrawList.Vertices.clear();
    DrawList.Font = &Font;
    DrawList.ScreenWidth = screenWidth;
    DrawList.ScreenHeight = screenHeight;

    for (const std::unique_ptr<UIElement>& element : Elements)
    {
        if (element)
        {
            element->Build(DrawList);
        }
    }
}

bool UIComponent::UploadVertices(ID3D11DeviceContext* context, ID3D11Device* device)
{
    const size_t vertexCount = DrawList.Vertices.size();
    if (vertexCount == 0)
    {
        return false;
    }

    if (!VertexBuffer || VertexCapacity < vertexCount)
    {
        // Grow geometrically: a HUD changes its text every frame but rarely its size.
        VertexCapacity = std::max<size_t>(vertexCount, VertexCapacity * 2);
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = static_cast<UINT>(sizeof(UIVertex) * VertexCapacity);
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateBuffer(&desc, nullptr, VertexBuffer.ReleaseAndGetAddressOf())))
        {
            VertexCapacity = 0;
            return false;
        }
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return false;
    }
    std::memcpy(mapped.pData, DrawList.Vertices.data(), sizeof(UIVertex) * vertexCount);
    context->Unmap(VertexBuffer.Get(), 0);
    return true;
}

void UIComponent::Render(ID3D11DeviceContext* context)
{
    Game* game = GetGame();
    if (context == nullptr || game == nullptr || game->GetDisplay() == nullptr || Elements.empty())
    {
        return;
    }

    ID3D11Device* device = game->GetDevice();
    if (!EnsureFont(device))
    {
        return;
    }
    CreateBuffers(device);

    const float screenWidth = static_cast<float>(game->GetDisplay()->GetWidth());
    const float screenHeight = static_cast<float>(game->GetDisplay()->GetHeight());
    BuildVertices(screenWidth, screenHeight);
    if (!UploadVertices(context, device))
    {
        return;
    }

    UIConstants constants;
    constants.ScreenSize = DirectX::XMFLOAT4(screenWidth, screenHeight, 0.0f, 0.0f);
    context->UpdateSubresource(ConstantBuffer.Get(), 0, nullptr, &constants, 0, 0);

    // Game::BindComponentShaders only records the pair on the component; putting them into the
    // context is the component's own job. Without this the interface is drawn with whatever
    // material the previous pass left bound.
    if (GetVertexShader() == nullptr || GetPixelShader() == nullptr)
    {
        return;
    }
    context->VSSetShader(GetVertexShader(), nullptr, 0);
    context->PSSetShader(GetPixelShader(), nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);

    const float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context->OMSetBlendState(BlendState.Get(), blendFactor, 0xffffffff);
    context->OMSetDepthStencilState(DepthState.Get(), 0);
    context->RSSetState(RasterState.Get());

    context->IASetInputLayout(game->GetInputLayout(VertexFormat::Primitive));
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* buffers[] = {VertexBuffer.Get()};
    const UINT stride = UIVertexStride;
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, buffers, &stride, &offset);
    context->VSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());
    ID3D11ShaderResourceView* atlas = Font.GetSRV();
    context->PSSetShaderResources(0, 1, &atlas);
    context->PSSetSamplers(0, 1, Sampler.GetAddressOf());

    context->Draw(static_cast<UINT>(DrawList.Vertices.size()), 0);
    game->CountDraw(static_cast<uint32_t>(DrawList.Vertices.size()));
}
