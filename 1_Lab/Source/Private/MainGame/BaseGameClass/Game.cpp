#include "../../../Public/MainGame/BaseGameClass/Game.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <iostream>
#include "../../../Public/Components/GameComponents.h"
#include "../../../Public/Components/Light/PointLightComponent.h"
#include "../../../Public/Components/SpecificComponents/FBXComponent.h"
#include "stb_image.h"
#define FIXED_FPS true

namespace
{
    //std::wstring shaderPath = L"Source/Shaders/BaseShader.hlsl"; //Base
    const std::wstring shaderPath = L"Source/Shaders/RotatedFigure.hlsl"; // Rotated
    const std::string shaderColor = "float4(1.0f, 1.0f, 0.0f, 1.0f)";
    const std::string vs_additional = "VSMainvs_5_0";
    const std::string ps_additional = "PSMainps_5_0";
    const std::string variant_default = "VARIANT_DEFAULT";
    const std::string variant_deferred_gbuffer = "VARIANT_DEFERRED_GBUFFER";
    const std::string variant_deferred_lighting = "VARIANT_DEFERRED_LIGHTING";

    const D3D_SHADER_MACRO shaderDefinesDeferredGBuffer[] =
    {
        {"DEFERRED_GBUFFER", "1"},
        {nullptr, nullptr}
    };

    const D3D_SHADER_MACRO shaderDefinesDeferredLighting[] =
    {
        {"DEFERRED_LIGHTING", "1"},
        {nullptr, nullptr}
    };

    std::string GetVariantTag(const ShaderCompileVariant variant)
    {
        switch (variant)
        {
        case ShaderCompileVariant::DeferredGBuffer:
            return variant_deferred_gbuffer;
        case ShaderCompileVariant::DeferredLighting:
            return variant_deferred_lighting;
        case ShaderCompileVariant::Default:
        default:
            return variant_default;
        }
    }

    const D3D_SHADER_MACRO* GetVariantDefines(const ShaderCompileVariant variant)
    {
        switch (variant)
        {
        case ShaderCompileVariant::DeferredGBuffer:
            return shaderDefinesDeferredGBuffer;
        case ShaderCompileVariant::DeferredLighting:
            return shaderDefinesDeferredLighting;
        case ShaderCompileVariant::Default:
        default:
            return nullptr;
        }
    }

    std::string MakeShaderCacheKey(const std::string& shaderName,
                                   const std::string& stageTag,
                                   const ShaderCompileVariant variant)
    {
        return shaderName + "|" + stageTag + "|" + GetVariantTag(variant);
    }

    uint32_t PackColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
    {
        return static_cast<uint32_t>(r) |
               (static_cast<uint32_t>(g) << 8) |
               (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(a) << 24);
    }

    float Hash01(int x, int y, int faceIndex)
    {
        const float value = std::sin(
            static_cast<float>(x * 12 + y * 78 + faceIndex * 193) * 0.131f
        ) * 43758.5453f;
        return value - std::floor(value);
    }

    float Clamp01(float value)
    {
        return std::max(0.0f, std::min(1.0f, value));
    }

    DirectX::XMFLOAT4X4 Identity4x4()
    {
        return DirectX::XMFLOAT4X4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }

    DirectX::XMVECTOR LoadVec3As4(const DirectX::XMFLOAT4& vector4)
    {
        return DirectX::XMVectorSet(vector4.x, vector4.y, vector4.z, 0.0f);
    }

    DirectX::XMFLOAT3 NormalizeFloat3(const DirectX::XMFLOAT4& value)
    {
        using namespace DirectX;
        const XMVECTOR vector = XMVector3Normalize(LoadVec3As4(value));
        XMFLOAT3 result = {};
        XMStoreFloat3(&result, vector);
        return result;
    }

    float DistanceToPointLight(const PointLightInfo& light, const DirectX::XMFLOAT3& position)
    {
        const float dx = light.Position.x - position.x;
        const float dy = light.Position.y - position.y;
        const float dz = light.Position.z - position.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    std::vector<uint32_t> CreateProceduralFace(CubeMapPreset preset, int faceIndex, int faceSize)
    {
        std::vector<uint32_t> pixels(faceSize * faceSize);

        for (int y = 0; y < faceSize; ++y)
        {
            for (int x = 0; x < faceSize; ++x)
            {
                const float u = static_cast<float>(x) / static_cast<float>(std::max(faceSize - 1, 1));
                const float v = static_cast<float>(y) / static_cast<float>(std::max(faceSize - 1, 1));

                float r = 0.0f;
                float g = 0.0f;
                float b = 0.0f;

                if (preset == CubeMapPreset::Space)
                {
                    const float nebula = 0.25f + 0.75f * std::pow(1.0f - v, 1.5f);
                    const float band = 0.5f + 0.5f * std::sin((u + faceIndex * 0.17f) * 8.0f + v * 4.0f);
                    r = 0.02f + 0.10f * nebula + 0.08f * band;
                    g = 0.03f + 0.12f * nebula + 0.05f * (1.0f - band);
                    b = 0.08f + 0.35f * nebula;

                    const float star = Hash01(x, y, faceIndex);
                    if (star > 0.9965f)
                    {
                        const float glow = std::min(1.0f, (star - 0.9965f) * 250.0f);
                        r += 0.6f * glow;
                        g += 0.65f * glow;
                        b += 0.8f * glow;
                    }
                }
                else
                {
                    const float skyBlend = std::pow(1.0f - v, 1.7f);
                    const float floorBlend = std::pow(v, 1.2f);
                    r = 0.16f + 0.22f * skyBlend + 0.12f * floorBlend;
                    g = 0.18f + 0.20f * skyBlend + 0.10f * floorBlend;
                    b = 0.20f + 0.18f * skyBlend + 0.08f * floorBlend;

                    const float lightBand = std::exp(-18.0f * ((u - 0.5f) * (u - 0.5f) + (v - 0.3f) * (v - 0.3f)));
                    r += 0.35f * lightBand;
                    g += 0.28f * lightBand;
                    b += 0.18f * lightBand;
                }

                r = Clamp01(r);
                g = Clamp01(g);
                b = Clamp01(b);

                pixels[y * faceSize + x] = PackColor(
                    static_cast<unsigned char>(r * 255.0f),
                    static_cast<unsigned char>(g * 255.0f),
                    static_cast<unsigned char>(b * 255.0f),
                    255
                );
            }
        }

        return pixels;
    }
    
    
}

void Game::CreateDepthBuffer(Microsoft::WRL::ComPtr<ID3D11Device> device, int width, int height)
{
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;

    HRESULT hr = device->CreateTexture2D(&depthDesc, nullptr, depthStencilBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create depth buffer texture\n");
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = device->CreateDepthStencilView(depthStencilBuffer.Get(), &dsvDesc, depthStencilView.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create depth stencil view\n");
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
    depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MostDetailedMip = 0;
    depthSrvDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(depthStencilBuffer.Get(), &depthSrvDesc, depthStencilSRV.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create depth SRV\n");
    }

    D3D11_DEPTH_STENCIL_DESC depthStateDesc = {};
    depthStateDesc.DepthEnable = TRUE;
    depthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
    
    depthStateDesc.StencilEnable = FALSE;
    depthStateDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    depthStateDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    
    depthStateDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    depthStateDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStateDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStateDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    
    depthStateDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    depthStateDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStateDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStateDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

    hr = device->CreateDepthStencilState(&depthStateDesc, depthStencilState.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create depth stencil state\n");
    }
    Context->OMSetDepthStencilState(depthStencilState.Get(), 0);
    Context->OMSetRenderTargets(1, &RenderTargetView, depthStencilView.Get());

}


Game::~Game()
{
    for (auto& shader : vertexShaderCache)
    {
        if (shader.second)
        {
            shader.second->Release();
        }
    }
    for (auto& shader : pixelShaderCache)
    {
        if (shader.second)
        {
            shader.second->Release();
        }
    }
    
    delete InputDevicePtr;
    delete DisplayPtr;
    delete FirstPlayer;
}

void Game::Initialize()
{
    DisplayPtr = new Display();
    DisplayPtr->SetGamePointer(this);
    FirstPlayer = new Player();
    FirstPlayer->SetGamePointer(this);
    InputDevicePtr = new InputDevice(this);
    //init window
    const WNDCLASSEX WinClass = DisplayPtr->GetWinClass();
    RegisterClassEx(&WinClass);

    //init rect
    RECT WindowRect = DisplayPtr->GetWinRect();
    AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

    //init hWnd
    HWND hWnd = DisplayPtr->GetHwnd();
    ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);
    SetFocus(hWnd);

    ShowCursor(true);

    InitSwapChainDesc(DisplayPtr->GetWinRect());
    HRESULT res = CreateDeviceAndSwapChain();
    if (FAILED(res))
    {
        std::cout << "Well, that was unexpected" << '\n';
    }
    res = InitRenderTarget();
    if (FAILED(res))
    {
        std::cout << "Error in creature RenderTargetView!\n";
        return;
    }
    if (GetFileAttributesW(shaderPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(DisplayPtr->GetHwnd(), L"Shader file not found!", L"Error", MB_OK);
        return;
    }

    CreateBackBuffer();
    CreateBlendStates();
    InitDeferredResources();
    InitShadowResources();
    AfterInitialize();
}

void Game::StartGame()
{
    Run();
}

bool Game::RegisterComponent(std::string Name, GameComponent* GameComponent,std::string PShaderName, std::string VShaderName)
{
    if (!Device) 
    {
        return false;
    }
    
    GameComponent->SetGame(this);
    PointLightComponent* pointLight = dynamic_cast<PointLightComponent*>(GameComponent);
    if (pointLight)
    {
        PointLights.push_back(pointLight);
    }
    else
    {
        const ShaderCompileVariant shaderVariant = ShouldUseDeferredGeometryVariant(GameComponent)
                                                       ? ShaderCompileVariant::DeferredGBuffer
                                                       : ShaderCompileVariant::Default;
        const std::string defaultShaderPath(shaderPath.begin(), shaderPath.end());

        if (VShaderName.empty())
        {
            if (shaderVariant == ShaderCompileVariant::Default)
            {
                GameComponent->SetVertexShader(GetVertexShader());
            }
            else
            {
                GameComponent->SetVertexShader(GetVertexShader(defaultShaderPath, shaderVariant));
            }
        }
        else
        {
            GameComponent->SetVertexShader(GetVertexShader(VShaderName, shaderVariant));
        }

        if (PShaderName.empty())
        {
            if (shaderVariant == ShaderCompileVariant::Default)
            {
                GameComponent->SetPixelShader(GetPixelShader());
            }
            else
            {
                GameComponent->SetPixelShader(GetPixelShader(defaultShaderPath, shaderVariant));
            }
        }
        else
        {
            GameComponent->SetPixelShader(GetPixelShader(PShaderName, shaderVariant));
        }

        GameComponent->CreateBuffers(Device);
    }
    
    Components.emplace(Name, GameComponent);
    return true;
}

std::vector<PointLightInfo> Game::GetPointLights(size_t maxLights) const
{
    std::vector<PointLightInfo> result;
    result.reserve(std::min(maxLights, PointLights.size()));
    
    for (PointLightComponent* pointLight : PointLights)
    {
        if (!pointLight)
        {
            continue;
        }
        
        PointLightInfo info;
        info.Position = pointLight->GetCenter();
        info.Color = pointLight->GetLightColor();
        info.Intensity = pointLight->GetIntensity();
        info.Radius = pointLight->GetRadius();
        info.bEnabled = pointLight->IsEnabled();
        result.push_back(info);
        
        if (result.size() >= maxLights)
        {
            break;
        }
    }
    
    return result;
}

void Game::SetShadowSettings(bool enabled, float shadowDistance)
{
    bShadowsEnabled = enabled;
    ShadowDistance = std::max(100.0f, shadowDistance);
    const float cascade0 = ShadowDistance * 0.08f;
    const float cascade1 = ShadowDistance * 0.28f;
    const float cascade2 = ShadowDistance * 1.0f;
    CascadeSplits = DirectX::XMFLOAT4(cascade0, cascade1, cascade2, 0.0f);
    ShadowParams.x = enabled ? 1.0f : 0.0f;
}

void Game::SetDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity)
{
    SetDirectionalLightDirection(direction);
    SetDirectionalLightColor(color);
    SetDirectionalLightIntensity(intensity);
}

void Game::SetDirectionalLightDirection(const glm::vec3& direction)
{
    const float len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (len > 0.0001f)
    {
        DirectionalLightDirection = DirectX::XMFLOAT4(direction.x / len, direction.y / len, direction.z / len, 0.0f);
    }
}

void Game::SetDirectionalLightColor(const glm::vec3& color)
{
    DirectionalLightColorIntensity.x = std::max(0.0f, color.x);
    DirectionalLightColorIntensity.y = std::max(0.0f, color.y);
    DirectionalLightColorIntensity.z = std::max(0.0f, color.z);
}

void Game::SetDirectionalLightIntensity(float intensity)
{
    DirectionalLightColorIntensity.w = std::max(0.0f, intensity);
}

bool Game::ShouldUseDeferredGeometryVariant(GameComponent* Component) const
{
    if (RenderingType != Deffered || Component == nullptr)
    {
        return false;
    }

    if (Component->IsSkybox() || Component->HasOpacity())
    {
        return false;
    }

    return true;
}

bool Game::InitDeferredResources()
{
    if (Device.Get() == nullptr || DisplayPtr == nullptr)
    {
        return false;
    }

    const int width = std::max(DisplayPtr->GetWidth(), 1);
    const int height = std::max(DisplayPtr->GetHeight(), 1);
    if (deferredBufferWidth == width && deferredBufferHeight == height &&
        gBufferAlbedoRTV && gBufferNormalRTV && gBufferMaterialRTV &&
        gBufferAlbedoSRV && gBufferNormalSRV && gBufferMaterialSRV &&
        deferredLightingCB)
    {
        return true;
    }

    auto createGBufferTarget = [&](DXGI_FORMAT format,
                                   Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
                                   Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv,
                                   Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv) -> bool
    {
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = format;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = Device->CreateTexture2D(&texDesc, nullptr, texture.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        hr = Device->CreateRenderTargetView(texture.Get(), nullptr, rtv.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        hr = Device->CreateShaderResourceView(texture.Get(), nullptr, srv.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        return true;
    };

    if (!createGBufferTarget(DXGI_FORMAT_R8G8B8A8_UNORM, gBufferAlbedoTexture, gBufferAlbedoRTV, gBufferAlbedoSRV))
    {
        std::cout << "Failed to create deferred albedo target." << std::endl;
        return false;
    }
    if (!createGBufferTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, gBufferNormalTexture, gBufferNormalRTV, gBufferNormalSRV))
    {
        std::cout << "Failed to create deferred normal target." << std::endl;
        return false;
    }
    if (!createGBufferTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, gBufferMaterialTexture, gBufferMaterialRTV, gBufferMaterialSRV))
    {
        std::cout << "Failed to create deferred material target." << std::endl;
        return false;
    }

    if (!gBufferSamplerState)
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        HRESULT hr = Device->CreateSamplerState(&samplerDesc, gBufferSamplerState.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            std::cout << "Failed to create deferred sampler." << std::endl;
            return false;
        }
    }

    if (!deferredLightingCB)
    {
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(DeferredLightingBufferData);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = 0;

        HRESULT hr = Device->CreateBuffer(&bufferDesc, nullptr, deferredLightingCB.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            std::cout << "Failed to create deferred lighting constant buffer." << std::endl;
            return false;
        }
    }

    deferredBufferWidth = width;
    deferredBufferHeight = height;

    return true;
}

void Game::UpdateDeferredLightingBuffer()
{
    using namespace DirectX;

    if (deferredLightingCB.Get() == nullptr || FirstPlayer == nullptr)
    {
        return;
    }

    DeferredLightingBufferData data = {};
    data.worldMatrix = Identity4x4();
    data.viewMatrix = FirstPlayer->GetViewMatrix();
    data.projectionMatrix = FirstPlayer->GetProjectionMatrix();
    {
        const XMMATRIX viewStored = XMLoadFloat4x4(&data.viewMatrix);
        const XMMATRIX projStored = XMLoadFloat4x4(&data.projectionMatrix);
        const XMMATRIX viewOriginal = XMMatrixTranspose(viewStored);
        const XMMATRIX projOriginal = XMMatrixTranspose(projStored);
        const XMMATRIX invViewOriginal = XMMatrixInverse(nullptr, viewOriginal);
        const XMMATRIX invProjOriginal = XMMatrixInverse(nullptr, projOriginal);
        XMStoreFloat4x4(&data.invViewMatrix, XMMatrixTranspose(invViewOriginal));
        XMStoreFloat4x4(&data.invProjectionMatrix, XMMatrixTranspose(invProjOriginal));
    }
    data.ObjectColor = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
    data.UVOffset = DirectX::XMFLOAT2(0.0f, 0.0f);
    data.HasTexture = 0.0f;
    data.padding = 0.0f;

    const glm::vec3 cameraPosition = FirstPlayer->GetPosition();
    data.CameraPosition = DirectX::XMFLOAT4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f);

    for (int i = 0; i < 8; ++i)
    {
        data.LightPositions[i] = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        data.LightColors[i] = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        data.LightParams[i] = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
    }

    const std::vector<PointLightInfo> pointLights = GetPointLights(8);
    const int lightCount = static_cast<int>(std::min<size_t>(pointLights.size(), 8));
    // Deferred scene looks significantly darker than forward because it lacks
    // the small amount of extra indirect light the forward shaders effectively get.
    // Raise ambient here without affecting forward rendering.
    data.LightMeta = DirectX::XMFLOAT4(static_cast<float>(lightCount), 0.22f, 32.0f, 0.0f);

    for (int i = 0; i < lightCount; ++i)
    {
        const PointLightInfo& light = pointLights[i];
        data.LightPositions[i] = DirectX::XMFLOAT4(light.Position.x, light.Position.y, light.Position.z, 1.0f);
        data.LightColors[i] = DirectX::XMFLOAT4(light.Color.x, light.Color.y, light.Color.z, 1.0f);
        data.LightParams[i] = DirectX::XMFLOAT4(light.Intensity, light.Radius, light.bEnabled ? 1.0f : 0.0f, 0.0f);
    }

    data.ReflectionData = DirectX::XMFLOAT4(0.0f, 0.0f, 5.0f, 0.0f);
    data.CascadeSplits = CascadeSplits;
    data.ShadowParams = ShadowParams;
    data.LightDirection = DirectionalLightDirection;
    data.DirectionalLightColorIntensity = DirectionalLightColorIntensity;

    for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
    {
        data.LightViewProjection[cascadeIndex] = CascadeLightViewProjection[cascadeIndex];
    }

    Context->UpdateSubresource(deferredLightingCB.Get(), 0, nullptr, &data, 0, 0);
}

void Game::RenderDeferredLightingPass(ID3D11RasterizerState* RasterState)
{
    if (!gBufferAlbedoSRV || !gBufferNormalSRV || !gBufferMaterialSRV || !deferredLightingCB)
    {
        return;
    }

    const std::string deferredShaderPath(shaderPath.begin(), shaderPath.end());
    ID3D11VertexShader* deferredVS = GetVertexShader(deferredShaderPath, ShaderCompileVariant::DeferredLighting);
    ID3D11PixelShader* deferredPS = GetPixelShader(deferredShaderPath, ShaderCompileVariant::DeferredLighting);
    if (deferredVS == nullptr || deferredPS == nullptr)
    {
        return;
    }

    UpdateDeferredLightingBuffer();

    Context->RSSetState(RasterState);
    Context->IASetInputLayout(nullptr);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->OMSetRenderTargets(1, &RenderTargetView, nullptr);

    ID3D11ShaderResourceView* gBufferSRVs[4] =
    {
        gBufferAlbedoSRV.Get(),
        gBufferNormalSRV.Get(),
        gBufferMaterialSRV.Get(),
        depthStencilSRV.Get()
    };

    Context->VSSetShader(deferredVS, nullptr, 0);
    Context->PSSetShader(deferredPS, nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, deferredLightingCB.GetAddressOf());
    Context->PSSetConstantBuffers(0, 1, deferredLightingCB.GetAddressOf());
    Context->PSSetShaderResources(0, 4, gBufferSRVs);

    if (gBufferSamplerState)
    {
        Context->PSSetSamplers(0, 1, gBufferSamplerState.GetAddressOf());
    }

    if (IsShadowEnabled())
    {
        ID3D11ShaderResourceView* shadowMapSRV = GetShadowMapSRV();
        ID3D11SamplerState* shadowMapSampler = GetShadowSampler();
        if (shadowMapSRV)
        {
            Context->PSSetShaderResources(4, 1, &shadowMapSRV);
        }
        if (shadowMapSampler)
        {
            Context->PSSetSamplers(4, 1, &shadowMapSampler);
        }
    }

    Context->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRVs[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    Context->PSSetShaderResources(0, 5, nullSRVs);
}

void Game::RenderDeferredDebugOverlay(ID3D11RasterizerState* RasterState)
{
    using namespace DirectX;

    if (!gBufferAlbedoSRV || !gBufferNormalSRV || !gBufferMaterialSRV || !deferredLightingCB)
    {
        return;
    }

    const std::string deferredShaderPath(shaderPath.begin(), shaderPath.end());
    ID3D11VertexShader* deferredVS = GetVertexShader(deferredShaderPath, ShaderCompileVariant::DeferredLighting);
    ID3D11PixelShader* deferredPS = GetPixelShader(deferredShaderPath, ShaderCompileVariant::DeferredLighting);
    if (deferredVS == nullptr || deferredPS == nullptr)
    {
        return;
    }

    Context->RSSetState(RasterState);
    Context->IASetInputLayout(nullptr);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->OMSetRenderTargets(1, &RenderTargetView, nullptr);

    ID3D11ShaderResourceView* gBufferSRVs[4] =
    {
        gBufferAlbedoSRV.Get(),
        gBufferNormalSRV.Get(),
        gBufferMaterialSRV.Get(),
        depthStencilSRV.Get()
    };

    Context->VSSetShader(deferredVS, nullptr, 0);
    Context->PSSetShader(deferredPS, nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, deferredLightingCB.GetAddressOf());
    Context->PSSetConstantBuffers(0, 1, deferredLightingCB.GetAddressOf());
    Context->PSSetShaderResources(0, 4, gBufferSRVs);

    if (gBufferSamplerState)
    {
        Context->PSSetSamplers(0, 1, gBufferSamplerState.GetAddressOf());
    }

    D3D11_VIEWPORT debugViewports[4] = {};
    const float debugWidth = std::max(160.0f, static_cast<float>(DisplayPtr->GetWidth()) * 0.22f);
    const float debugHeight = std::max(90.0f, static_cast<float>(DisplayPtr->GetHeight()) * 0.22f);
    const float startX = static_cast<float>(DisplayPtr->GetWidth()) - debugWidth * 2.0f - 24.0f;
    const float startY = 24.0f;

    for (int i = 0; i < 4; ++i)
    {
        debugViewports[i].Width = debugWidth;
        debugViewports[i].Height = debugHeight;
        debugViewports[i].MinDepth = 0.0f;
        debugViewports[i].MaxDepth = 1.0f;
    }

    debugViewports[0].TopLeftX = startX;
    debugViewports[0].TopLeftY = startY;
    debugViewports[1].TopLeftX = startX + debugWidth + 8.0f;
    debugViewports[1].TopLeftY = startY;
    debugViewports[2].TopLeftX = startX;
    debugViewports[2].TopLeftY = startY + debugHeight + 8.0f;
    debugViewports[3].TopLeftX = startX + debugWidth + 8.0f;
    debugViewports[3].TopLeftY = startY + debugHeight + 8.0f;

    for (int debugMode = 1; debugMode <= 4; ++debugMode)
    {
        DeferredLightingBufferData debugData = {};
        debugData.worldMatrix = Identity4x4();
        debugData.viewMatrix = FirstPlayer->GetViewMatrix();
        debugData.projectionMatrix = FirstPlayer->GetProjectionMatrix();
        {
            using DirectX::XMMATRIX;
            const XMMATRIX viewStored = XMLoadFloat4x4(&debugData.viewMatrix);
            const XMMATRIX projStored = XMLoadFloat4x4(&debugData.projectionMatrix);
            const XMMATRIX viewOriginal = XMMatrixTranspose(viewStored);
            const XMMATRIX projOriginal = XMMatrixTranspose(projStored);
            const XMMATRIX invViewOriginal = XMMatrixInverse(nullptr, viewOriginal);
            const XMMATRIX invProjOriginal = XMMatrixInverse(nullptr, projOriginal);
            XMStoreFloat4x4(&debugData.invViewMatrix, XMMatrixTranspose(invViewOriginal));
            XMStoreFloat4x4(&debugData.invProjectionMatrix, XMMatrixTranspose(invProjOriginal));
        }
        debugData.ObjectColor = DirectX::XMFLOAT4(static_cast<float>(debugMode), 0.0f, 1.0f, 1.0f);
        debugData.UVOffset = DirectX::XMFLOAT2(0.0f, 0.0f);
        debugData.HasTexture = 0.0f;
        debugData.padding = 0.0f;
        const glm::vec3 debugCameraPosition = FirstPlayer->GetPosition();
        // Пересечения с буфером глубины частиц
        debugData.CameraPosition = DirectX::XMFLOAT4(debugCameraPosition.x, debugCameraPosition.y, debugCameraPosition.z, 1.0f);
        debugData.LightMeta = DirectX::XMFLOAT4(0.0f, 0.06f, 32.0f, 0.0f);
        debugData.ReflectionData = DirectX::XMFLOAT4(0.0f, 0.0f, 5.0f, 0.0f);
        debugData.CascadeSplits = CascadeSplits;
        debugData.ShadowParams = ShadowParams;
        if (RenderingType == Deffered)
        {
            debugData.ShadowParams.x = 0.0f;
        }
        debugData.LightDirection = DirectionalLightDirection;
        debugData.DirectionalLightColorIntensity = DirectionalLightColorIntensity;
        for (int i = 0; i < 8; ++i)
        {
            debugData.LightPositions[i] = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            debugData.LightColors[i] = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            debugData.LightParams[i] = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
        }
        const std::vector<PointLightInfo> debugPointLights = GetPointLights(8);
        const int debugLightCount = static_cast<int>(std::min<size_t>(debugPointLights.size(), 8));
        debugData.LightMeta.x = static_cast<float>(debugLightCount);
        for (int i = 0; i < 8; ++i)
        {
            if (i >= debugLightCount)
            {
                continue;
            }
            const PointLightInfo& light = debugPointLights[i];
            debugData.LightPositions[i] = DirectX::XMFLOAT4(light.Position.x, light.Position.y, light.Position.z, 1.0f);
            debugData.LightColors[i] = DirectX::XMFLOAT4(light.Color.x, light.Color.y, light.Color.z, 1.0f);
            debugData.LightParams[i] = DirectX::XMFLOAT4(light.Intensity, light.Radius, light.bEnabled ? 1.0f : 0.0f, 0.0f);
        }
        for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
        {
            debugData.LightViewProjection[cascadeIndex] = CascadeLightViewProjection[cascadeIndex];
        }

        Context->RSSetViewports(1, &debugViewports[debugMode - 1]);
        Context->UpdateSubresource(deferredLightingCB.Get(), 0, nullptr, &debugData, 0, 0);
        Context->Draw(3, 0);
    }

    D3D11_VIEWPORT fullViewport = {};
    fullViewport.Width = static_cast<float>(DisplayPtr->GetWidth());
    fullViewport.Height = static_cast<float>(DisplayPtr->GetHeight());
    fullViewport.TopLeftX = 0.0f;
    fullViewport.TopLeftY = 0.0f;
    fullViewport.MinDepth = 0.0f;
    fullViewport.MaxDepth = 1.0f;
    Context->RSSetViewports(1, &fullViewport);

    ID3D11ShaderResourceView* nullSRVs[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    Context->PSSetShaderResources(0, 5, nullSRVs);
}

bool Game::CreateProceduralCubeMap(const std::string& cubeMapName, CubeMapPreset preset, int faceSize)
{
    if (!Device)
    {
        return false;
    }

    auto existing = cubeMapCache.find(cubeMapName);
    if (existing != cubeMapCache.end())
    {
        return true;
    }

    std::array<std::vector<uint32_t>, 6> facePixels;
    std::array<D3D11_SUBRESOURCE_DATA, 6> subresources = {};

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        facePixels[faceIndex] = CreateProceduralFace(preset, faceIndex, faceSize);
        subresources[faceIndex].pSysMem = facePixels[faceIndex].data();
        subresources[faceIndex].SysMemPitch = faceSize * sizeof(uint32_t);
        subresources[faceIndex].SysMemSlicePitch = 0;
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = faceSize;
    textureDesc.Height = faceSize;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 6;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    std::unique_ptr<CubeMapResource> cubeMap(new CubeMapResource());
    cubeMap->Name = cubeMapName;

    HRESULT hr = Device->CreateTexture2D(&textureDesc, subresources.data(), cubeMap->Texture.GetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create cube map texture: " << cubeMapName << std::endl;
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;
    hr = Device->CreateShaderResourceView(cubeMap->Texture.Get(), &srvDesc, cubeMap->ShaderResourceView.GetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create cube map SRV: " << cubeMapName << std::endl;
        return false;
    }

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = Device->CreateSamplerState(&samplerDesc, cubeMap->SamplerState.GetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create cube map sampler: " << cubeMapName << std::endl;
        return false;
    }

    cubeMapCache.emplace(cubeMapName, std::move(cubeMap));
    return true;
}

bool Game::CreateCubeMapFromFiles(const std::string& cubeMapName, const std::vector<std::string>& facePaths)
{
    if (!Device || facePaths.size() != 6)
    {
        return false;
    }

    auto existing = cubeMapCache.find(cubeMapName);
    if (existing != cubeMapCache.end())
    {
        return true;
    }

    struct LoadedFaceData
    {
        int Width = 0;
        int Height = 0;
        std::vector<unsigned char> Pixels;
    };

    std::array<LoadedFaceData, 6> loadedFaces;
    std::array<D3D11_SUBRESOURCE_DATA, 6> subresources = {};
    int faceWidth = 0;
    int faceHeight = 0;

    stbi_set_flip_vertically_on_load(false);

    for (size_t faceIndex = 0; faceIndex < facePaths.size(); ++faceIndex)
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* imageData = stbi_load(facePaths[faceIndex].c_str(), &width, &height, &channels, 4);
        if (!imageData)
        {
            std::cout << "Failed to load cube map face: " << facePaths[faceIndex] << std::endl;
            return false;
        }

        std::cout << "Cube face loaded: " << facePaths[faceIndex]
                  << " (" << width << "x" << height << ", src channels: " << channels << ")" << std::endl;

        if (faceIndex == 0)
        {
            faceWidth = width;
            faceHeight = height;
        }
        else if (width != faceWidth || height != faceHeight)
        {
            std::cout << "Cube map faces must have the same size: " << cubeMapName << std::endl;
            stbi_image_free(imageData);
            return false;
        }

        if (width != height)
        {
            std::cout << "Cube map face must be square for Direct3D texture cube: "
                      << facePaths[faceIndex] << " (" << width << "x" << height << ")" << std::endl;
            stbi_image_free(imageData);
            return false;
        }

        loadedFaces[faceIndex].Width = width;
        loadedFaces[faceIndex].Height = height;
        loadedFaces[faceIndex].Pixels.assign(imageData, imageData + width * height * 4);
        stbi_image_free(imageData);

        subresources[faceIndex].pSysMem = loadedFaces[faceIndex].Pixels.data();
        subresources[faceIndex].SysMemPitch = width * 4;
        subresources[faceIndex].SysMemSlicePitch = 0;
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = faceWidth;
    textureDesc.Height = faceHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 6;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    std::unique_ptr<CubeMapResource> cubeMap(new CubeMapResource());
    cubeMap->Name = cubeMapName;

    HRESULT hr = Device->CreateTexture2D(&textureDesc, subresources.data(), cubeMap->Texture.GetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create cube map texture from files: " << cubeMapName
                  << ", HRESULT=0x" << std::hex << hr << std::dec
                  << ", faceSize=" << faceWidth << "x" << faceHeight << std::endl;
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;
    hr = Device->CreateShaderResourceView(cubeMap->Texture.Get(), &srvDesc, cubeMap->ShaderResourceView.GetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create cube map SRV from files: " << cubeMapName << std::endl;
        return false;
    }

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = Device->CreateSamplerState(&samplerDesc, cubeMap->SamplerState.GetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create cube map sampler from files: " << cubeMapName << std::endl;
        return false;
    }

    cubeMapCache.emplace(cubeMapName, std::move(cubeMap));
    return true;
}

CubeMapResource* Game::GetCubeMap(const std::string& cubeMapName) const
{
    auto it = cubeMapCache.find(cubeMapName);
    if (it == cubeMapCache.end())
    {
        return nullptr;
    }

    return it->second.get();
}

void Game::CreateBlendStates()
{
    D3D11_BLEND_DESC transparentDesc = {};
    transparentDesc.AlphaToCoverageEnable = FALSE;
    transparentDesc.IndependentBlendEnable = FALSE;
    
    transparentDesc.RenderTarget[0].BlendEnable = TRUE;
    transparentDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    transparentDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    transparentDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    
    transparentDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    transparentDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    transparentDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    
    transparentDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    HRESULT hr = Device->CreateBlendState(&transparentDesc, &transparentBlendState);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create transparent blend state\n");
    }
    
    D3D11_BLEND_DESC opaqueDesc = {};
    opaqueDesc.AlphaToCoverageEnable = FALSE;
    opaqueDesc.IndependentBlendEnable = FALSE;
    opaqueDesc.RenderTarget[0].BlendEnable = FALSE;
    opaqueDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    hr = Device->CreateBlendState(&opaqueDesc, &opaqueBlendState);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create opaque blend state\n");
    }
}

bool Game::InitShadowResources()
{
    if (Device.Get() == nullptr || Context == nullptr)
    {
        return false;
    }

    ShadowParams.y = static_cast<float>(MaxShadowCascades);
    ShadowParams.w = 1.0f / static_cast<float>(ShadowMapSize);

    D3D11_TEXTURE2D_DESC shadowDesc = {};
    shadowDesc.Width = ShadowMapSize;
    shadowDesc.Height = ShadowMapSize;
    shadowDesc.MipLevels = 1;
    shadowDesc.ArraySize = MaxShadowCascades;
    shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    shadowDesc.SampleDesc.Count = 1;
    shadowDesc.Usage = D3D11_USAGE_DEFAULT;
    shadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Device->CreateTexture2D(&shadowDesc, nullptr, shadowTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create shadow texture." << std::endl;
        return false;
    }

    for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.ArraySize = 1;
        dsvDesc.Texture2DArray.FirstArraySlice = cascadeIndex;
        dsvDesc.Texture2DArray.MipSlice = 0;

        hr = Device->CreateDepthStencilView(shadowTexture.Get(), &dsvDesc, shadowDSVs[cascadeIndex].ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            std::cout << "Failed to create cascade shadow DSV: " << cascadeIndex << std::endl;
            return false;
        }
        CascadeLightViewProjection[cascadeIndex] = Identity4x4();
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.ArraySize = MaxShadowCascades;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.MostDetailedMip = 0;

    hr = Device->CreateShaderResourceView(shadowTexture.Get(), &srvDesc, shadowSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create shadow SRV." << std::endl;
        return false;
    }

    D3D11_SAMPLER_DESC shadowSamplerDesc = {};
    shadowSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    shadowSamplerDesc.BorderColor[0] = 1.0f;
    shadowSamplerDesc.BorderColor[1] = 1.0f;
    shadowSamplerDesc.BorderColor[2] = 1.0f;
    shadowSamplerDesc.BorderColor[3] = 1.0f;
    shadowSamplerDesc.MinLOD = 0.0f;
    shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = Device->CreateSamplerState(&shadowSamplerDesc, shadowSampler.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create shadow sampler." << std::endl;
        return false;
    }

    ID3DBlob* shadowVSBlobRaw = nullptr;
    hr = CreateShader(DisplayPtr->GetHwnd(), nullptr, L"Source/Shaders/ShadowDepth.hlsl", "VSMain", "vs_5_0", &shadowVSBlobRaw);
    if (FAILED(hr) || shadowVSBlobRaw == nullptr)
    {
        std::cout << "Failed to compile shadow VS." << std::endl;
        return false;
    }
    shadowVertexShaderBlob.Attach(shadowVSBlobRaw);

    hr = Device->CreateVertexShader(
        shadowVertexShaderBlob->GetBufferPointer(),
        shadowVertexShaderBlob->GetBufferSize(),
        nullptr,
        shadowVertexShader.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr))
    {
        std::cout << "Failed to create shadow VS." << std::endl;
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC primitiveLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    hr = Device->CreateInputLayout(
        primitiveLayoutDesc,
        ARRAYSIZE(primitiveLayoutDesc),
        shadowVertexShaderBlob->GetBufferPointer(),
        shadowVertexShaderBlob->GetBufferSize(),
        shadowInputLayoutPrimitive.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr))
    {
        std::cout << "Failed to create shadow primitive input layout." << std::endl;
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC meshLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    hr = Device->CreateInputLayout(
        meshLayoutDesc,
        ARRAYSIZE(meshLayoutDesc),
        shadowVertexShaderBlob->GetBufferPointer(),
        shadowVertexShaderBlob->GetBufferSize(),
        shadowInputLayoutMesh.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr))
    {
        std::cout << "Failed to create shadow mesh input layout." << std::endl;
        return false;
    }

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.ByteWidth = sizeof(ShadowPassBufferData);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = Device->CreateBuffer(&cbDesc, nullptr, shadowPassCB.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create shadow pass constant buffer." << std::endl;
        return false;
    }

    D3D11_RASTERIZER_DESC shadowRasterDesc = {};
    shadowRasterDesc.FillMode = D3D11_FILL_SOLID;
    shadowRasterDesc.CullMode = D3D11_CULL_BACK;
    shadowRasterDesc.DepthClipEnable = TRUE;
    shadowRasterDesc.DepthBias = 2000;
    shadowRasterDesc.SlopeScaledDepthBias = 2.0f;
    shadowRasterDesc.DepthBiasClamp = 0.0f;
    hr = Device->CreateRasterizerState(&shadowRasterDesc, shadowRasterState.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create shadow rasterizer state." << std::endl;
        return false;
    }

    return true;
}

void Game::CreateBackBuffer()
{
    if (FAILED(InitShaderBuffers()))
    {
        std::cout << "Failed to Shader Buffers!\n";
        return;
    }
}

void Game::UpdateShadowCascades()
{
    using namespace DirectX;

    if (!bShadowsEnabled || !FirstPlayer || !DisplayPtr)
    {
        return;
    }

    const float nearPlane = 0.1f;
    const float farPlane = ShadowDistance;
    const float aspect = DisplayPtr->GetWidth() / static_cast<float>(std::max(DisplayPtr->GetHeight(), 1));
    const float fovY = XMConvertToRadians(60.0f);
    const float tanHalfFovY = std::tan(fovY * 0.5f);

    const glm::vec3 cameraPosition = FirstPlayer->GetPosition();
    glm::vec3 cameraForward = FirstPlayer->GetForward();
    glm::vec3 cameraRight = FirstPlayer->GetRight();
    glm::vec3 cameraUp = FirstPlayer->GetUp();

    if (FirstPlayer->GetCameraMode() == CameraMode::Orbital)
    {
        cameraForward = FirstPlayer->GetOrbitForward();
        cameraRight = FirstPlayer->GetOrbitRight();
        cameraUp = FirstPlayer->GetOrbitUp();
    }

    if (glm::length(cameraForward) < 0.001f || glm::length(cameraRight) < 0.001f || glm::length(cameraUp) < 0.001f)
    {
        cameraForward = glm::vec3(0.0f, 0.0f, 1.0f);
        cameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    const XMFLOAT3 lightDirection = NormalizeFloat3(DirectionalLightDirection);
    XMVECTOR lightDirV = XMVectorSet(lightDirection.x, lightDirection.y, lightDirection.z, 0.0f);
    lightDirV = XMVector3Normalize(lightDirV);

    const float splitDistances[MaxShadowCascades + 1] = {
        nearPlane,
        std::max(CascadeSplits.x, nearPlane + 1.0f),
        std::max(CascadeSplits.y, nearPlane + 2.0f),
        std::min(std::max(CascadeSplits.z, nearPlane + 3.0f), farPlane)
    };

    for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
    {
        const float cascadeNear = splitDistances[cascadeIndex];
        const float cascadeFar = splitDistances[cascadeIndex + 1];

        std::array<XMVECTOR, 8> frustumCorners = {};
        int cornerIndex = 0;

        for (int depthIndex = 0; depthIndex < 2; ++depthIndex)
        {
            const float depth = (depthIndex == 0) ? cascadeNear : cascadeFar;
            const float halfHeight = depth * tanHalfFovY;
            const float halfWidth = halfHeight * aspect;
            const glm::vec3 center = cameraPosition + cameraForward * depth;

            const glm::vec3 offsets[4] = {
                -cameraRight * halfWidth + cameraUp * halfHeight,
                cameraRight * halfWidth + cameraUp * halfHeight,
                -cameraRight * halfWidth - cameraUp * halfHeight,
                cameraRight * halfWidth - cameraUp * halfHeight
            };

            for (const glm::vec3& offset : offsets)
            {
                const glm::vec3 corner = center + offset;
                frustumCorners[cornerIndex++] = XMVectorSet(corner.x, corner.y, corner.z, 1.0f);
            }
        }

        XMVECTOR centroid = XMVectorZero();
        for (const XMVECTOR& corner : frustumCorners)
        {
            centroid = XMVectorAdd(centroid, corner);
        }
        centroid = XMVectorScale(centroid, 1.0f / static_cast<float>(frustumCorners.size()));

        float radius = 0.0f;
        for (const XMVECTOR& corner : frustumCorners)
        {
            const XMVECTOR toCorner = XMVectorSubtract(corner, centroid);
            radius = std::max(radius, XMVectorGetX(XMVector3Length(toCorner)));
        }
        radius = std::max(radius, 1.0f);

        const XMVECTOR lightPosition = XMVectorSubtract(centroid, XMVectorScale(lightDirV, radius * 2.2f + 120.0f));
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        if (std::abs(XMVectorGetX(XMVector3Dot(up, lightDirV))) > 0.95f)
        {
            up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        }

        const XMMATRIX lightView = XMMatrixLookAtLH(lightPosition, centroid, up);

        float minX = FLT_MAX;
        float maxX = -FLT_MAX;
        float minY = FLT_MAX;
        float maxY = -FLT_MAX;
        float minZ = FLT_MAX;
        float maxZ = -FLT_MAX;

        for (const XMVECTOR& corner : frustumCorners)
        {
            const XMVECTOR cornerLS = XMVector3TransformCoord(corner, lightView);
            const float x = XMVectorGetX(cornerLS);
            const float y = XMVectorGetY(cornerLS);
            const float z = XMVectorGetZ(cornerLS);
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
            minZ = std::min(minZ, z);
            maxZ = std::max(maxZ, z);
        }

        const float depthPadding = std::max(80.0f, radius * 0.45f);
        minZ -= depthPadding;
        maxZ += depthPadding;

        const XMMATRIX lightProjection = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);
        const XMMATRIX lightViewProjection = XMMatrixMultiply(lightView, lightProjection);
        XMStoreFloat4x4(&CascadeLightViewProjection[cascadeIndex], XMMatrixTranspose(lightViewProjection));
    }
}

void Game::RenderShadowMaps()
{
    if (!bShadowsEnabled ||
        shadowVertexShader.Get() == nullptr ||
        shadowPassCB.Get() == nullptr ||
        shadowRasterState.Get() == nullptr ||
        shadowSRV.Get() == nullptr)
    {
        return;
    }

    UpdateShadowCascades();

    ID3D11ShaderResourceView* nullShadowSRV = nullptr;
    Context->PSSetShaderResources(4, 1, &nullShadowSRV);

    D3D11_VIEWPORT shadowViewport = {};
    shadowViewport.TopLeftX = 0.0f;
    shadowViewport.TopLeftY = 0.0f;
    shadowViewport.Width = static_cast<float>(ShadowMapSize);
    shadowViewport.Height = static_cast<float>(ShadowMapSize);
    shadowViewport.MinDepth = 0.0f;
    shadowViewport.MaxDepth = 1.0f;

    Context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->RSSetState(shadowRasterState.Get());
    Context->RSSetViewports(1, &shadowViewport);

    for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
    {
        ID3D11DepthStencilView* cascadeDSV = shadowDSVs[cascadeIndex].Get();
        Context->OMSetRenderTargets(0, nullptr, cascadeDSV);
        Context->ClearDepthStencilView(cascadeDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

        for (auto& componentPair : Components)
        {
            GameComponent* component = componentPair.second;
            if (component == nullptr || component->IsSkybox())
            {
                continue;
            }

            component->RenderShadow(
                Context,
                shadowVertexShader.Get(),
                shadowPassCB.Get(),
                shadowInputLayoutPrimitive.Get(),
                shadowInputLayoutMesh.Get(),
                CascadeLightViewProjection[cascadeIndex]
            );
        }
    }

    Context->OMSetRenderTargets(1, &RenderTargetView, depthStencilView.Get());
}

void Game::RegisterShaders(std::string ShaderName, std::string AdditionalAttributeToName, ShaderCompileVariant variant)
{
    std::string cacheKey = MakeShaderCacheKey(ShaderName, AdditionalAttributeToName, variant);
    if (shaderCache.find(cacheKey) != shaderCache.end())
    {
        return;
    }
    bool isVertexShader = (AdditionalAttributeToName == vs_additional);
    bool isPixelShader = (AdditionalAttributeToName == ps_additional);
    if (!isVertexShader && !isPixelShader)
    {
        std::cout << "Unknown shader type for: " << ShaderName << std::endl;
        return;
    }
    LPCSTR entryPoint = isVertexShader ? "VSMain" : "PSMain";
    LPCSTR target = isVertexShader ? "vs_5_0" : "ps_5_0";
    std::wstring wideShaderPath = std::wstring(ShaderName.begin(), ShaderName.end());
    ID3DBlob* shaderBlob = nullptr;
    const D3D_SHADER_MACRO* compileDefines = GetVariantDefines(variant);
    HRESULT res = CreateShader(DisplayPtr->GetHwnd(), compileDefines, wideShaderPath.c_str(),
                               entryPoint, target, &shaderBlob);
    if (FAILED(res))
    {
        std::cout << "Failed to compile shader: " << ShaderName << std::endl;
        return;
    }
    shaderCache.emplace(cacheKey, shaderBlob);
    if (isVertexShader)
    {
        ID3D11VertexShader* vertexShader = nullptr;
        res = Device->CreateVertexShader(shaderBlob->GetBufferPointer(), 
                                         shaderBlob->GetBufferSize(), 
                                         nullptr, &vertexShader);
        if (FAILED(res))
        {
            std::cout << "Failed to create vertex shader: " << ShaderName << std::endl;
            return;
        }
        vertexShaderCache.emplace(cacheKey, vertexShader);
    }
    else if (isPixelShader)
    {
        ID3D11PixelShader* pixelShader = nullptr;
        res = Device->CreatePixelShader(shaderBlob->GetBufferPointer(), 
                                        shaderBlob->GetBufferSize(), 
                                        nullptr, &pixelShader);
        if (FAILED(res))
        {
            std::cout << "Failed to create pixel shader: " << ShaderName << std::endl;
            return;
        }
        pixelShaderCache.emplace(cacheKey, pixelShader);
    }
    
    std::cout << "Successfully registered shader: " << ShaderName << std::endl;
}

void Game::Run()
{
    //ToDo: maybe 2 rasters? 3d and 2d
    // Инициализация растеризатора
    CD3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.CullMode = D3D11_CULL_NONE; // Мешает кубик смотреть
    //rastDesc.CullMode = D3D11_CULL_BACK;
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.FrontCounterClockwise = false;
    rastDesc.DepthBias = 0;
    rastDesc.DepthBiasClamp = 0.0f;
    rastDesc.SlopeScaledDepthBias = 0.0f;
    rastDesc.DepthClipEnable = true;   
    rastDesc.ScissorEnable = false;
    rastDesc.MultisampleEnable = false;
    rastDesc.AntialiasedLineEnable = false;
    ID3D11RasterizerState* rastState;
    HRESULT res = Device->CreateRasterizerState(&rastDesc, &rastState);
    Context->RSSetState(rastState);

    auto prevTime = std::chrono::high_resolution_clock::now();

    MSG msg = {};
    while (true)
    {
        // Handle the windows messages.
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
            {
                break;
            }
        }
        // Расчет времени кадра
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - prevTime).count();
        prevTime = currentTime;

        if (FIXED_FPS && deltaTime > 0.1f)
        {
            deltaTime = 0.1f;
        }
        Update(deltaTime);
        float clearColor[] = {0.0f, 0.0f, 0.2f, 1.0f};
        Context->ClearRenderTargetView(RenderTargetView, clearColor);
        Context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        switch (RenderingType)
        {
        case Forward :
            {
               DrawForward(rastState);
               break;
            }
        case Deffered:
            {
                DrawDeffered(rastState);
                break;
            }
        case Custom :
            {
                Draw(rastState);
                break;
            }
        default:
            {
                DrawForward(rastState);
                break;
            }
        }
        
    }
}


void Game::Update(float deltaTime)
{
    TotalTime += deltaTime;
    ProcessInput(deltaTime);

    FrameCount++;
    static float fpsUpdateTime = 0;
    fpsUpdateTime += deltaTime;

    if (fpsUpdateTime >= 0.5f) // Update FPS every 0.5 seconds
    {
        float Fps = FrameCount / fpsUpdateTime;
        WCHAR text[256];
        swprintf_s(text, TEXT("FPS: %.1f | Objects: %zu"), Fps, Components.size());
        SetWindowText(DisplayPtr->GetHwnd(), text);

        FrameCount = 0;
        fpsUpdateTime = 0;
    }
}

void Game::EndFrame()
{
    SwapChain->Present(1, 0);
}

void Game::Exit()
{
    DestroyResources();
    PostQuitMessage(0);
}

ID3D11VertexShader* Game::GetVertexShader(const std::string& VertexShaderName, ShaderCompileVariant variant)
{
    const std::string cacheKey = MakeShaderCacheKey(VertexShaderName, vs_additional, variant);
    auto it = vertexShaderCache.find(cacheKey);
    if (it != vertexShaderCache.end())
    {
        return it->second;
    }
    
    RegisterShaders(VertexShaderName, vs_additional, variant);
    
    it = vertexShaderCache.find(cacheKey);
    if (it != vertexShaderCache.end())
    {
        return it->second;
    }
    
    std::cout << "Warning: Failed to get vertex shader: " << VertexShaderName 
              << ", using base shader" << std::endl;
    return BaseVertexShader;
}

ID3D11PixelShader* Game::GetPixelShader(const std::string& PixelShaderName, ShaderCompileVariant variant)
{
    const std::string cacheKey = MakeShaderCacheKey(PixelShaderName, ps_additional, variant);
    auto it = pixelShaderCache.find(cacheKey);
    if (it != pixelShaderCache.end())
    {
        return it->second;
    }
    
    RegisterShaders(PixelShaderName, ps_additional, variant);
    
    it = pixelShaderCache.find(cacheKey);
    if (it != pixelShaderCache.end())
    {
        return it->second;
    }
    
    std::cout << "Warning: Failed to get pixel shader: " << PixelShaderName 
              << ", using base shader" << std::endl;
    return BasePixelShader;
}

void Game::ProcessInput(float deltaTime)
{
    assert(InputDevicePtr);
}

void Game::MessageHandler()
{
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (msg.message == WM_QUIT)
        {
            Exit();
        }
    }
}

void Game::InitSwapChainDesc(const RECT& WindowRect)
{
    HWND hWnd = DisplayPtr->GetHwnd();
    
    if (!hWnd || !IsWindow(hWnd))
    {
        std::cout << "ERROR: Invalid window handle in InitSwapChainDesc!" << std::endl;
        return;
    }
    
    int width = WindowRect.right - WindowRect.left;
    int height = WindowRect.bottom - WindowRect.top;
    
    if (width <= 0 || height <= 0)
    {
        width = 800;
        height = 600;
    }
    
    ZeroMemory(&SwapChainDescription, sizeof(DXGI_SWAP_CHAIN_DESC));
    
    SwapChainDescription.BufferCount = 1;
    SwapChainDescription.BufferDesc.Width = width;
    SwapChainDescription.BufferDesc.Height = height;
    SwapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDescription.BufferDesc.RefreshRate.Numerator = 0;
    SwapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
    SwapChainDescription.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    SwapChainDescription.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDescription.OutputWindow = hWnd;
    SwapChainDescription.Windowed = TRUE;
    SwapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDescription.Flags = 0;
    SwapChainDescription.SampleDesc.Count = 1;
    SwapChainDescription.SampleDesc.Quality = 0;
    
    std::cout << "SwapChainDesc initialized: " << width << "x" << height << ", HWND: " << hWnd << std::endl;
}

HRESULT Game::CreateDeviceAndSwapChain()
{
    // Правильный массив уровней — несколько уровней для обратной совместимости
    D3D_FEATURE_LEVEL FeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };
    UINT numFeatureLevels = ARRAYSIZE(FeatureLevels);
    D3D_FEATURE_LEVEL selectedFeatureLevel;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    // Создаем устройство и цепочку свопов
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        FeatureLevels,
        numFeatureLevels,
        D3D11_SDK_VERSION,
        &SwapChainDescription,
        &SwapChain,
        &Device,
        &selectedFeatureLevel,
        &Context);
    
    // Если не получилось с DEBUG флагом, пробуем без него
    if (FAILED(hr))
    {
        std::cout << "First attempt failed with: 0x" << std::hex << hr << std::dec << std::endl;
        
        // Пробуем без DEBUG флага
        createDeviceFlags = 0;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            FeatureLevels,
            numFeatureLevels,
            D3D11_SDK_VERSION,
            &SwapChainDescription,
            &SwapChain,
            &Device,
            &selectedFeatureLevel,
            &Context);
    }
    
    if (FAILED(hr))
    {
        std::cout << "D3D11CreateDeviceAndSwapChain failed with error: 0x" << std::hex << hr << std::dec << std::endl;
        
        // Расшифровка ошибки
        switch (hr)
        {
        case DXGI_ERROR_UNSUPPORTED:
            std::cout << "ERROR: DXGI_ERROR_UNSUPPORTED - The requested feature level is not supported." << std::endl;
            break;
        case DXGI_ERROR_INVALID_CALL:
            std::cout << "ERROR: DXGI_ERROR_INVALID_CALL - Invalid parameters provided." << std::endl;
            std::cout << "  Check: SwapChainDescription structure, especially SampleDesc and BufferDesc" << std::endl;
            break;
        case E_INVALIDARG:
            std::cout << "ERROR: E_INVALIDARG - Invalid argument passed." << std::endl;
            break;
        default:
            std::cout << "ERROR: Unknown error code." << std::endl;
            break;
        }
        
        return hr;
    }

    std::cout << "Successfully created device with feature level: ";
    switch (selectedFeatureLevel)
    {
    case D3D_FEATURE_LEVEL_11_1: std::cout << "11.1"; break;
    case D3D_FEATURE_LEVEL_11_0: std::cout << "11.0"; break;
    case D3D_FEATURE_LEVEL_10_1: std::cout << "10.1"; break;
    case D3D_FEATURE_LEVEL_10_0: std::cout << "10.0"; break;
    case D3D_FEATURE_LEVEL_9_3: std::cout << "9.3"; break;
    case D3D_FEATURE_LEVEL_9_2: std::cout << "9.2"; break;
    case D3D_FEATURE_LEVEL_9_1: std::cout << "9.1"; break;
    default: std::cout << "Unknown"; break;
    }
    std::cout << std::endl;
    const auto WindowRect = DisplayPtr->GetWinRect();
    const int ScreenW = WindowRect.right - WindowRect.left;
    const int ScreenH = WindowRect.bottom - WindowRect.top;
    CreateDepthBuffer(Device, ScreenW, ScreenH);
    
    return S_OK;
}

HRESULT Game::InitRenderTarget()
{
    const HRESULT res = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackTexture);
    return FAILED(res) ? res: Device->CreateRenderTargetView(BackTexture, nullptr,&RenderTargetView);
}

HRESULT Game::InitShaderBuffers()
{
    //VertexShader
    HRESULT res = CreateShader(DisplayPtr->GetHwnd(), nullptr, shaderPath.c_str(), "VSMain", "vs_5_0", &vertexBC);
    if (FAILED(res))
    {
        return res;
    }
    std::string ShaderPath(shaderPath.begin(), shaderPath.end());
    std::string cacheVertexKey = ShaderPath + "VSMain" + "vs_5_0";
    shaderCache.emplace(cacheVertexKey, vertexBC);

    //PixelShader
    res = CreateShader(DisplayPtr->GetHwnd(), nullptr, shaderPath.c_str(), "PSMain", "ps_5_0", &pixelBC);
    if (FAILED(res))
    {
        return res;
    }
    std::string cachePixelKey = ShaderPath + "PSMain" + "ps_5_0";
    shaderCache.emplace(cachePixelKey, pixelBC);

    HRESULT CreateVertexShaderResult = Device->CreateVertexShader(
        vertexBC->GetBufferPointer(),
        vertexBC->GetBufferSize(),
        nullptr, &BaseVertexShader);

    if (FAILED(CreateVertexShaderResult))
    {
        std::cout << "Failed to create vertex shader!\n";
        return CreateVertexShaderResult;
    }
    HRESULT CreatePixelShaderResult = Device->CreatePixelShader(
        pixelBC->GetBufferPointer(),
        pixelBC->GetBufferSize(),
        nullptr, &BasePixelShader);
    if (FAILED(CreatePixelShaderResult))
    {
        std::cout << "Failed to create pixel shader!\n";
        return CreatePixelShaderResult;
    }

    D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        D3D11_INPUT_ELEMENT_DESC{
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            0,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
        D3D11_INPUT_ELEMENT_DESC{
            "COLOR",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        }
    };
    
    Device->CreateInputLayout(
        inputElements,
        2,
        vertexBC->GetBufferPointer(),
        vertexBC->GetBufferSize(),
        &layout);
}

HRESULT Game::CreateShader(HWND hWnd,CONST D3D_SHADER_MACRO* pDefines, LPCWSTR FileName, LPCSTR pEntrypoint,
                           LPCSTR pTarget, ID3DBlob** Buffer)
{
    ID3DBlob* errorVertexCode = nullptr;
    const HRESULT res = D3DCompileFromFile(FileName,
                                           pDefines /*macros*/,
                                           nullptr /*include*/,
                                           pEntrypoint,
                                           pTarget,
                                           D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
                                           0,
                                           Buffer,
                                           &errorVertexCode);


    if (FAILED(res))
    {
        if (errorVertexCode)
        {
            char* compileErrors = (char*)(errorVertexCode->GetBufferPointer());

            std::cout << compileErrors << std::endl;
        }
        else
        {
            MessageBox(hWnd, L"MyVeryFirstShader.hlsl", L"Missing Shader File", MB_OK);
        }

        return res;
    }

    return S_OK;
}
