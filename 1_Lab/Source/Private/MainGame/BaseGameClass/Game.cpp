#include "../../../Public/MainGame/BaseGameClass/Game.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <iostream>
#include "../../../Public/Components/GameComponents.h"
#include "../../../Public/Components/Light/PointLightComponent.h"
#include "../../../Public/Render/ShaderCompiler.h"
#include "stb_image.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

namespace
{
    const std::wstring shaderPath = L"Source/Shaders/RotatedFigure.hlsl";
    const std::wstring shadowShaderPath = L"Source/Shaders/ShadowDepth.hlsl";
    const std::string vs_additional = "VSMainvs_5_0";
    const std::string ps_additional = "PSMainps_5_0";
    const std::string variant_default = "VARIANT_DEFAULT";
    const std::string variant_deferred_gbuffer = "VARIANT_DEFERRED_GBUFFER";
    const std::string variant_deferred_lighting = "VARIANT_DEFERRED_LIGHTING";

    // Largest real time step fed into the simulation (prevents huge jumps after stalls).
    constexpr float MaxFrameDeltaSeconds = 0.1f;

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

    std::string DefaultShaderPath()
    {
        return "Source/Shaders/RotatedFigure.hlsl";
    }

    std::wstring ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return std::wstring();
        }
        const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring result(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), length);
        return result;
    }

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
        return shaderName + "|" + stageTag + "|" + GetVariantTag(variant) + "|" + ShaderCompiler::GetPolicyTag();
    }

    UINT CountRenderTargetOutputs(ID3DBlob* pixelShaderBlob)
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;
        if (pixelShaderBlob == nullptr ||
            FAILED(D3DReflect(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(),
                              IID_ID3D11ShaderReflection, reinterpret_cast<void**>(reflection.GetAddressOf()))))
        {
            return 0;
        }

        D3D11_SHADER_DESC desc = {};
        reflection->GetDesc(&desc);
        UINT targets = 0;
        for (UINT i = 0; i < desc.OutputParameters; ++i)
        {
            D3D11_SIGNATURE_PARAMETER_DESC parameter = {};
            reflection->GetOutputParameterDesc(i, &parameter);
            if (parameter.SystemValueType == D3D_NAME_TARGET)
            {
                ++targets;
            }
        }
        return targets;
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

    DirectX::XMFLOAT3 NormalizeFloat3(const DirectX::XMFLOAT4& value)
    {
        using namespace DirectX;
        const XMVECTOR vector = XMVector3Normalize(XMVectorSet(value.x, value.y, value.z, 0.0f));
        XMFLOAT3 result = {};
        XMStoreFloat3(&result, vector);
        return result;
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

    // Tiny vertex shaders whose only purpose is to provide input signatures for the shared layouts.
    const char* PrimitiveSignatureShader = R"(
        struct VS_IN { float4 pos : POSITION; float4 col : COLOR; };
        float4 VSMain(VS_IN input) : SV_Position { return input.pos + input.col * 1e-9f; }
    )";

    const char* MeshSignatureShader = R"(
        struct VS_IN { float4 pos : POSITION; float4 normal : NORMAL; float2 uv : TEXCOORD; };
        float4 VSMain(VS_IN input) : SV_Position { return input.pos + input.normal * 1e-9f + float4(input.uv, 0.0f, 0.0f) * 1e-9f; }
    )";

    bool CompileSignatureShader(const char* source, Microsoft::WRL::ComPtr<ID3DBlob>& blob)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
                                      "VSMain", "vs_5_0", 0, 0, blob.GetAddressOf(), errors.GetAddressOf());
        if (FAILED(hr) && errors)
        {
            std::cout << static_cast<const char*>(errors->GetBufferPointer()) << std::endl;
        }
        return SUCCEEDED(hr);
    }
}

const char* ToString(RegisterResult result)
{
    switch (result)
    {
    case RegisterResult::Ok: return "Ok";
    case RegisterResult::NullComponent: return "NullComponent";
    case RegisterResult::EmptyName: return "EmptyName";
    case RegisterResult::DuplicateName: return "DuplicateName";
    case RegisterResult::DeviceNotReady: return "DeviceNotReady";
    default: return "Unknown";
    }
}

// ---------------------------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------------------------

Game::Game() = default;

Game::~Game()
{
    DestroyResources();
}

void Game::DestroyResources()
{
    if (Context)
    {
        Context->ClearState();
        Context->Flush();
    }

    // Components first: they observe cube maps and shaders owned by the caches below.
    PointLights.clear();
    Components.clear();
    cubeMapCache.clear();

    vertexShaderCache.clear();
    pixelShaderCache.clear();
    pixelShaderTargetCount.clear();
    shaderCache.clear();
    BaseVertexShader.Reset();
    BasePixelShader.Reset();
    layout.Reset();
    meshLayout.Reset();

    gBufferAlbedoTexture.Reset();
    gBufferAlbedoRTV.Reset();
    gBufferAlbedoSRV.Reset();
    gBufferNormalTexture.Reset();
    gBufferNormalRTV.Reset();
    gBufferNormalSRV.Reset();
    gBufferWorldPositionTexture.Reset();
    gBufferWorldPositionRTV.Reset();
    gBufferWorldPositionSRV.Reset();
    gBufferSamplerState.Reset();
    deferredLightingCB.Reset();

    shadowTexture.Reset();
    shadowSRV.Reset();
    for (auto& dsv : shadowDSVs)
    {
        dsv.Reset();
    }
    shadowSampler.Reset();
    shadowVertexShader.Reset();
    shadowInstancedVertexShader.Reset();
    shadowVertexShaderBlob.Reset();
    shadowInputLayoutPrimitive.Reset();
    shadowInputLayoutMesh.Reset();
    shadowPassCB.Reset();
    shadowRasterState.Reset();
    bShadowResourcesReady = false;

    for (int i = 0; i < GpuTimerLatency; ++i)
    {
        GpuTimerDisjoint[i].Reset();
        GpuTimerBegin[i].Reset();
        GpuTimerEnd[i].Reset();
    }

    transparentBlendState.Reset();
    opaqueBlendState.Reset();
    depthStencilState.Reset();
    depthStencilStateReadOnly.Reset();
    depthStencilStateSkybox.Reset();
    DefaultRasterState.Reset();
    ReleaseSwapChainResources();

    if (SwapChain)
    {
        SwapChain->SetFullscreenState(FALSE, nullptr);
    }
    SwapChain.Reset();
    Context.Reset();
    Device.Reset();

    // The input device and the window go last: the swap chain referenced the window.
    InputDevicePtr.reset();
    FirstPlayer.reset();
    DisplayPtr.reset();
    bInitialized = false;
}

// ---------------------------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------------------------

bool Game::Initialize()
{
    if (bInitialized)
    {
        return true;
    }

    auto fail = [this](const char* reason) -> bool
    {
        std::cout << "Initialization failed: " << reason << std::endl;
        DestroyResources();
        return false;
    };

    DisplayPtr = std::make_unique<Display>();
    if (!DisplayPtr->GetHwnd())
    {
        return fail("window creation");
    }
    DisplayPtr->SetGamePointer(this);

    FirstPlayer = std::make_unique<Player>();
    FirstPlayer->SetGamePointer(this);
    InputDevicePtr = std::make_unique<InputDevice>(this);

    const HWND hWnd = DisplayPtr->GetHwnd();
    ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);
    SetFocus(hWnd);
    ShowCursor(true);

    const int width = std::max(DisplayPtr->GetWidth(), 1);
    const int height = std::max(DisplayPtr->GetHeight(), 1);
    InitSwapChainDesc(width, height);

    if (!CreateDeviceAndSwapChain())
    {
        return fail("Direct3D 11 device (feature level 11_0 is required)");
    }
    if (!CreateSwapChainResources(width, height))
    {
        return fail("back buffer / depth buffer");
    }
    if (GetFileAttributesW(shaderPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(hWnd, L"Source/Shaders/RotatedFigure.hlsl was not found. Run from the project directory.",
                   L"Missing shader", MB_OK);
        return fail("shader files not found");
    }
    if (!InitShaderBuffers())
    {
        return fail("base shaders");
    }
    if (!CreateInputLayouts())
    {
        return fail("input layouts");
    }
    if (!CreateRenderStates())
    {
        return fail("render states");
    }
    CreateGpuTimer();

    // A resize that arrived while the window was being shown is already reflected in the sizes above.
    bResizePending = false;
    bInitialized = true;
    AfterInitialize();
    return true;
}

void Game::InitSwapChainDesc(int width, int height)
{
    ZeroMemory(&SwapChainDescription, sizeof(DXGI_SWAP_CHAIN_DESC));

    SwapChainDescription.BufferCount = 1;
    SwapChainDescription.BufferDesc.Width = static_cast<UINT>(width);
    SwapChainDescription.BufferDesc.Height = static_cast<UINT>(height);
    SwapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDescription.BufferDesc.RefreshRate.Numerator = 0;
    SwapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
    SwapChainDescription.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    SwapChainDescription.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDescription.OutputWindow = DisplayPtr->GetHwnd();
    SwapChainDescription.Windowed = TRUE;
    SwapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDescription.Flags = 0;
    SwapChainDescription.SampleDesc.Count = 1;
    SwapChainDescription.SampleDesc.Quality = 0;
}

bool Game::CreateDeviceAndSwapChain()
{
    // The renderer needs SM5 and compute shaders, so lower feature levels are not accepted.
    const D3D_FEATURE_LEVEL FeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    D3D_FEATURE_LEVEL selectedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    auto tryCreate = [&](UINT flags, const D3D_FEATURE_LEVEL* levels, UINT levelCount) -> HRESULT
    {
        SwapChain.Reset();
        Context.Reset();
        Device.Reset();
        return D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            levels,
            levelCount,
            D3D11_SDK_VERSION,
            &SwapChainDescription,
            SwapChain.GetAddressOf(),
            Device.GetAddressOf(),
            &selectedFeatureLevel,
            Context.GetAddressOf());
    };

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = tryCreate(createDeviceFlags, FeatureLevels, ARRAYSIZE(FeatureLevels));
    if (hr == E_INVALIDARG)
    {
        // Runtimes without 11.1 reject the whole array; retry with 11.0 only.
        hr = tryCreate(createDeviceFlags, &FeatureLevels[1], 1);
    }
    if (FAILED(hr) && (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG))
    {
        std::cout << "Debug layer unavailable (0x" << std::hex << hr << std::dec << "), retrying without it." << std::endl;
        hr = tryCreate(0, FeatureLevels, ARRAYSIZE(FeatureLevels));
        if (hr == E_INVALIDARG)
        {
            hr = tryCreate(0, &FeatureLevels[1], 1);
        }
    }

    if (FAILED(hr))
    {
        std::cout << "D3D11CreateDeviceAndSwapChain failed with error: 0x" << std::hex << hr << std::dec << std::endl;
        SwapChain.Reset();
        Context.Reset();
        Device.Reset();
        return false;
    }

    std::cout << "Created device with feature level "
              << (selectedFeatureLevel == D3D_FEATURE_LEVEL_11_1 ? "11.1" : "11.0") << std::endl;
    return true;
}

bool Game::CreateSwapChainResources(int width, int height)
{
    HRESULT hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(BackTexture.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        std::cout << "Failed to get the swap chain back buffer." << std::endl;
        return false;
    }

    hr = Device->CreateRenderTargetView(BackTexture.Get(), nullptr, RenderTargetView.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create the back buffer render target view." << std::endl;
        return false;
    }

    return CreateDepthBuffer(width, height);
}

void Game::ReleaseSwapChainResources()
{
    RenderTargetView.Reset();
    BackTexture.Reset();
    depthStencilView.Reset();
    depthStencilSRV.Reset();
    depthStencilBuffer.Reset();

    // Screen-sized G-buffer is recreated on the next deferred frame.
    gBufferAlbedoTexture.Reset();
    gBufferAlbedoRTV.Reset();
    gBufferAlbedoSRV.Reset();
    gBufferNormalTexture.Reset();
    gBufferNormalRTV.Reset();
    gBufferNormalSRV.Reset();
    gBufferWorldPositionTexture.Reset();
    gBufferWorldPositionRTV.Reset();
    gBufferWorldPositionSRV.Reset();
    deferredBufferWidth = 0;
    deferredBufferHeight = 0;
}

bool Game::CreateDepthBuffer(int width, int height)
{
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = static_cast<UINT>(width);
    depthDesc.Height = static_cast<UINT>(height);
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> newBuffer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> newDSV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> newSRV;

    HRESULT hr = Device->CreateTexture2D(&depthDesc, nullptr, newBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create depth buffer texture." << std::endl;
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    hr = Device->CreateDepthStencilView(newBuffer.Get(), &dsvDesc, newDSV.GetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create depth stencil view." << std::endl;
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
    depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MostDetailedMip = 0;
    depthSrvDesc.Texture2D.MipLevels = 1;
    hr = Device->CreateShaderResourceView(newBuffer.Get(), &depthSrvDesc, newSRV.GetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create depth SRV." << std::endl;
        return false;
    }

    // Publish only after every view was created.
    depthStencilBuffer = newBuffer;
    depthStencilView = newDSV;
    depthStencilSRV = newSRV;
    return true;
}

bool Game::CreateRenderStates()
{
    // Culling stays disabled: the procedural meshes do not share a consistent winding yet.
    CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.FrontCounterClockwise = FALSE;
    rastDesc.DepthClipEnable = TRUE;
    if (FAILED(Device->CreateRasterizerState(&rastDesc, DefaultRasterState.ReleaseAndGetAddressOf())))
    {
        std::cout << "Failed to create rasterizer state." << std::endl;
        return false;
    }

    D3D11_BLEND_DESC transparentDesc = {};
    transparentDesc.RenderTarget[0].BlendEnable = TRUE;
    transparentDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    transparentDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    transparentDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    transparentDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    transparentDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    transparentDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    transparentDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    D3D11_BLEND_DESC opaqueDesc = {};
    opaqueDesc.RenderTarget[0].BlendEnable = FALSE;
    opaqueDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    if (FAILED(Device->CreateBlendState(&transparentDesc, transparentBlendState.ReleaseAndGetAddressOf())) ||
        FAILED(Device->CreateBlendState(&opaqueDesc, opaqueBlendState.ReleaseAndGetAddressOf())))
    {
        std::cout << "Failed to create blend states." << std::endl;
        return false;
    }

    CD3D11_DEPTH_STENCIL_DESC opaqueDepth(D3D11_DEFAULT); // LESS, write all
    CD3D11_DEPTH_STENCIL_DESC readOnlyDepth(D3D11_DEFAULT);
    readOnlyDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    CD3D11_DEPTH_STENCIL_DESC skyboxDepth(D3D11_DEFAULT);
    skyboxDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    skyboxDepth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    if (FAILED(Device->CreateDepthStencilState(&opaqueDepth, depthStencilState.ReleaseAndGetAddressOf())) ||
        FAILED(Device->CreateDepthStencilState(&readOnlyDepth, depthStencilStateReadOnly.ReleaseAndGetAddressOf())) ||
        FAILED(Device->CreateDepthStencilState(&skyboxDepth, depthStencilStateSkybox.ReleaseAndGetAddressOf())))
    {
        std::cout << "Failed to create depth stencil states." << std::endl;
        return false;
    }

    return true;
}

bool Game::CreateInputLayouts()
{
    Microsoft::WRL::ComPtr<ID3DBlob> primitiveBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> meshBlob;
    if (!CompileSignatureShader(PrimitiveSignatureShader, primitiveBlob) ||
        !CompileSignatureShader(MeshSignatureShader, meshBlob))
    {
        std::cout << "Failed to compile input signature shaders." << std::endl;
        return false;
    }

    const D3D11_INPUT_ELEMENT_DESC primitiveElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    const D3D11_INPUT_ELEMENT_DESC meshElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    if (FAILED(Device->CreateInputLayout(primitiveElements, ARRAYSIZE(primitiveElements),
                                         primitiveBlob->GetBufferPointer(), primitiveBlob->GetBufferSize(),
                                         layout.ReleaseAndGetAddressOf())) ||
        FAILED(Device->CreateInputLayout(meshElements, ARRAYSIZE(meshElements),
                                         meshBlob->GetBufferPointer(), meshBlob->GetBufferSize(),
                                         meshLayout.ReleaseAndGetAddressOf())))
    {
        std::cout << "Failed to create input layouts." << std::endl;
        return false;
    }

    return true;
}

ID3D11InputLayout* Game::GetInputLayout(VertexFormat format) const
{
    return format == VertexFormat::Mesh ? meshLayout.Get() : layout.Get();
}

bool Game::InitShaderBuffers()
{
    const std::string defaultPath = DefaultShaderPath();
    RegisterShaders(defaultPath, vs_additional, ShaderCompileVariant::Default);
    RegisterShaders(defaultPath, ps_additional, ShaderCompileVariant::Default);

    auto vs = vertexShaderCache.find(MakeShaderCacheKey(defaultPath, vs_additional, ShaderCompileVariant::Default));
    auto ps = pixelShaderCache.find(MakeShaderCacheKey(defaultPath, ps_additional, ShaderCompileVariant::Default));
    if (vs == vertexShaderCache.end() || ps == pixelShaderCache.end())
    {
        return false;
    }

    BaseVertexShader = vs->second;
    BasePixelShader = ps->second;
    return true;
}

HRESULT Game::CreateShader(HWND hWnd, const D3D_SHADER_MACRO* pDefines, LPCWSTR FileName, LPCSTR pEntrypoint,
                           LPCSTR pTarget, ID3DBlob** Buffer)
{
    std::string errors;
    const HRESULT res = ShaderCompiler::CompileFromFile(FileName, pDefines, pEntrypoint, pTarget, Buffer, &errors);
    if (FAILED(res))
    {
        std::wcout << L"Shader compilation failed: " << FileName << std::endl;
        std::cout << errors << std::endl;
        if (GetFileAttributesW(FileName) == INVALID_FILE_ATTRIBUTES)
        {
            MessageBox(hWnd, FileName, L"Missing Shader File", MB_OK);
        }
    }
    return res;
}

// ---------------------------------------------------------------------------------------------
// Scene registry
// ---------------------------------------------------------------------------------------------

RegisterResult Game::RegisterComponent(const std::string& Name, GameComponent* Component,
                                       const std::string& PShaderName, const std::string& VShaderName)
{
    // Every check happens before any side effect, so a failed registration leaves no trace.
    RegisterResult result = RegisterResult::Ok;
    if (Component == nullptr)
    {
        result = RegisterResult::NullComponent;
    }
    else if (Name.empty())
    {
        result = RegisterResult::EmptyName;
    }
    else if (!Device)
    {
        result = RegisterResult::DeviceNotReady;
    }
    else if (Components.find(Name) != Components.end())
    {
        result = RegisterResult::DuplicateName;
    }

    if (result != RegisterResult::Ok)
    {
        std::cout << "RegisterComponent('" << Name << "') failed: " << ToString(result) << std::endl;
        return result;
    }

    Component->SetGame(this);
    PointLightComponent* pointLight = dynamic_cast<PointLightComponent*>(Component);
    if (pointLight == nullptr)
    {
        Component->SetShaderNames(VShaderName, PShaderName);
        Component->CreateBuffers(Device.Get());
        // Compile the forward variant now instead of on the first frame.
        BindComponentShaders(Component, ShaderCompileVariant::Default);
    }

    Components.emplace(Name, std::unique_ptr<GameComponent>(Component));
    if (pointLight != nullptr)
    {
        PointLights.push_back(pointLight);
    }
    return RegisterResult::Ok;
}

bool Game::UnregisterComponent(const std::string& Name)
{
    auto it = Components.find(Name);
    if (it == Components.end())
    {
        return false;
    }

    GameComponent* removed = it->second.get();
    PointLights.erase(std::remove(PointLights.begin(), PointLights.end(), removed), PointLights.end());
    for (auto& pair : Components)
    {
        pair.second->ClearParentReferences(removed);
    }

    Components.erase(it);
    return true;
}

GameComponent* Game::FindComponent(const std::string& Name) const
{
    auto it = Components.find(Name);
    return it != Components.end() ? it->second.get() : nullptr;
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
        info.Position = pointLight->GetWorldPosition();
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

void Game::CountDraw(uint32_t indexCount, uint32_t instanceCount)
{
    ++Stats.DrawCalls;
    Stats.InstancesDrawn += instanceCount;
    Stats.IndicesDrawn += static_cast<uint64_t>(indexCount) * instanceCount;
}

// ---------------------------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------------------------

void Game::SetShadowSettings(bool enabled, float shadowDistance)
{
    bShadowsEnabled = enabled;
    ShadowDistance = std::max(100.0f, shadowDistance);
    const float cascade0 = ShadowDistance * 0.08f;
    const float cascade1 = ShadowDistance * 0.28f;
    const float cascade2 = ShadowDistance * 1.0f;
    CascadeSplits = DirectX::XMFLOAT4(cascade0, cascade1, cascade2, 0.0f);
    ShadowParams.x = enabled ? 1.0f : 0.0f;
    if (enabled)
    {
        bShadowInitFailed = false;
    }
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

// ---------------------------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------------------------

void Game::RegisterShaders(const std::string& ShaderName, const std::string& AdditionalAttributeToName,
                           ShaderCompileVariant variant)
{
    const std::string cacheKey = MakeShaderCacheKey(ShaderName, AdditionalAttributeToName, variant);
    if (shaderCache.find(cacheKey) != shaderCache.end())
    {
        return;
    }
    const bool isVertexShader = (AdditionalAttributeToName == vs_additional);
    const bool isPixelShader = (AdditionalAttributeToName == ps_additional);
    if (!isVertexShader && !isPixelShader)
    {
        std::cout << "Unknown shader type for: " << ShaderName << std::endl;
        return;
    }

    // A failed key is remembered (null blob) so a broken shader is not recompiled every frame.
    shaderCache.emplace(cacheKey, nullptr);

    const LPCSTR entryPoint = isVertexShader ? "VSMain" : "PSMain";
    const LPCSTR target = isVertexShader ? "vs_5_0" : "ps_5_0";
    const std::wstring wideShaderPath = ToWide(ShaderName);
    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
    const HRESULT res = CreateShader(DisplayPtr ? DisplayPtr->GetHwnd() : nullptr, GetVariantDefines(variant),
                                     wideShaderPath.c_str(), entryPoint, target, shaderBlob.GetAddressOf());
    if (FAILED(res))
    {
        std::cout << "Failed to compile shader: " << ShaderName << std::endl;
        return;
    }

    if (isVertexShader)
    {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
        if (FAILED(Device->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(),
                                              nullptr, vertexShader.GetAddressOf())))
        {
            std::cout << "Failed to create vertex shader: " << ShaderName << std::endl;
            return;
        }
        vertexShaderCache.emplace(cacheKey, vertexShader);
    }
    else
    {
        Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
        if (FAILED(Device->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(),
                                             nullptr, pixelShader.GetAddressOf())))
        {
            std::cout << "Failed to create pixel shader: " << ShaderName << std::endl;
            return;
        }
        pixelShaderCache.emplace(cacheKey, pixelShader);
        pixelShaderTargetCount[cacheKey] = CountRenderTargetOutputs(shaderBlob.Get());
    }

    shaderCache[cacheKey] = shaderBlob;
    std::cout << "Successfully registered shader: " << ShaderName << " [" << GetVariantTag(variant) << "]" << std::endl;
}

ID3D11VertexShader* Game::GetVertexShader(const std::string& VertexShaderName, ShaderCompileVariant variant)
{
    const std::string cacheKey = MakeShaderCacheKey(VertexShaderName, vs_additional, variant);
    auto it = vertexShaderCache.find(cacheKey);
    if (it == vertexShaderCache.end())
    {
        RegisterShaders(VertexShaderName, vs_additional, variant);
        it = vertexShaderCache.find(cacheKey);
    }
    if (it != vertexShaderCache.end())
    {
        return it->second.Get();
    }

    std::cout << "Warning: Failed to get vertex shader: " << VertexShaderName << ", using base shader" << std::endl;
    return BaseVertexShader.Get();
}

ID3D11PixelShader* Game::GetPixelShader(const std::string& PixelShaderName, ShaderCompileVariant variant)
{
    const std::string cacheKey = MakeShaderCacheKey(PixelShaderName, ps_additional, variant);
    auto it = pixelShaderCache.find(cacheKey);
    if (it == pixelShaderCache.end())
    {
        RegisterShaders(PixelShaderName, ps_additional, variant);
        it = pixelShaderCache.find(cacheKey);
    }
    if (it != pixelShaderCache.end())
    {
        return it->second.Get();
    }

    std::cout << "Warning: Failed to get pixel shader: " << PixelShaderName << ", using base shader" << std::endl;
    return BasePixelShader.Get();
}

ComponentShaderVariant& Game::ResolveComponentShaders(GameComponent* Component, ShaderCompileVariant variant)
{
    ComponentShaderVariant& slot = Component->GetShaderVariant(variant);
    if (slot.bResolved)
    {
        return slot;
    }

    const std::string defaultPath = DefaultShaderPath();
    const std::string& vsName = Component->GetVertexShaderName().empty() ? defaultPath : Component->GetVertexShaderName();
    const std::string& psName = Component->GetPixelShaderName().empty() ? defaultPath : Component->GetPixelShaderName();

    slot.VertexShader = GetVertexShader(vsName, variant);
    slot.PixelShader = GetPixelShader(psName, variant);
    if (variant == ShaderCompileVariant::DeferredGBuffer)
    {
        // A define does not create MRT outputs: only a shader that really writes the G-buffer qualifies.
        auto targets = pixelShaderTargetCount.find(MakeShaderCacheKey(psName, ps_additional, variant));
        slot.bWritesGBuffer = targets != pixelShaderTargetCount.end() && targets->second >= 3;
    }
    slot.bResolved = true;
    return slot;
}

bool Game::BindComponentShaders(GameComponent* Component, ShaderCompileVariant variant)
{
    if (Component == nullptr)
    {
        return false;
    }
    if (variant == ShaderCompileVariant::DeferredLighting)
    {
        variant = ShaderCompileVariant::Default;
    }

    ComponentShaderVariant& slot = ResolveComponentShaders(Component, variant);
    if (variant == ShaderCompileVariant::DeferredGBuffer && !slot.bWritesGBuffer)
    {
        return false;
    }
    if (slot.VertexShader == nullptr || slot.PixelShader == nullptr)
    {
        return false;
    }

    Component->SetVertexShader(slot.VertexShader);
    Component->SetPixelShader(slot.PixelShader);
    return true;
}

bool Game::SupportsDeferredGeometry(GameComponent* Component)
{
    if (Component == nullptr || Component->IsSkybox() || Component->HasOpacity())
    {
        return false;
    }
    return ResolveComponentShaders(Component, ShaderCompileVariant::DeferredGBuffer).bWritesGBuffer;
}

// ---------------------------------------------------------------------------------------------
// Cube maps
// ---------------------------------------------------------------------------------------------

bool Game::CreateProceduralCubeMap(const std::string& cubeMapName, CubeMapPreset preset, int faceSize)
{
    if (!Device)
    {
        return false;
    }

    if (cubeMapCache.find(cubeMapName) != cubeMapCache.end())
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

    std::unique_ptr<CubeMapResource> cubeMap = std::make_unique<CubeMapResource>();
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

    if (cubeMapCache.find(cubeMapName) != cubeMapCache.end())
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

    std::unique_ptr<CubeMapResource> cubeMap = std::make_unique<CubeMapResource>();
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
    return it == cubeMapCache.end() ? nullptr : it->second.get();
}

// ---------------------------------------------------------------------------------------------
// Deferred renderer resources and passes
// ---------------------------------------------------------------------------------------------

bool Game::InitDeferredResources()
{
    if (!Device || !DisplayPtr)
    {
        return false;
    }

    const int width = std::max(DisplayPtr->GetWidth(), 1);
    const int height = std::max(DisplayPtr->GetHeight(), 1);
    if (deferredBufferWidth == width && deferredBufferHeight == height &&
        gBufferAlbedoRTV && gBufferNormalRTV && gBufferWorldPositionRTV &&
        gBufferAlbedoSRV && gBufferNormalSRV && gBufferWorldPositionSRV &&
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

        return SUCCEEDED(Device->CreateTexture2D(&texDesc, nullptr, texture.ReleaseAndGetAddressOf())) &&
               SUCCEEDED(Device->CreateRenderTargetView(texture.Get(), nullptr, rtv.ReleaseAndGetAddressOf())) &&
               SUCCEEDED(Device->CreateShaderResourceView(texture.Get(), nullptr, srv.ReleaseAndGetAddressOf()));
    };

    // Albedo.rgb + specular weight | normal.xyz + ambient model | world position (half float).
    if (!createGBufferTarget(DXGI_FORMAT_R8G8B8A8_UNORM, gBufferAlbedoTexture, gBufferAlbedoRTV, gBufferAlbedoSRV) ||
        !createGBufferTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, gBufferNormalTexture, gBufferNormalRTV, gBufferNormalSRV) ||
        !createGBufferTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, gBufferWorldPositionTexture, gBufferWorldPositionRTV, gBufferWorldPositionSRV))
    {
        std::cout << "Failed to create G-buffer targets." << std::endl;
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

        if (FAILED(Device->CreateSamplerState(&samplerDesc, gBufferSamplerState.ReleaseAndGetAddressOf())))
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

        if (FAILED(Device->CreateBuffer(&bufferDesc, nullptr, deferredLightingCB.ReleaseAndGetAddressOf())))
        {
            std::cout << "Failed to create deferred lighting constant buffer." << std::endl;
            return false;
        }
    }

    // The lighting shaders must exist before the resources are reported as ready.
    const std::string defaultPath = DefaultShaderPath();
    RegisterShaders(defaultPath, vs_additional, ShaderCompileVariant::DeferredLighting);
    RegisterShaders(defaultPath, ps_additional, ShaderCompileVariant::DeferredLighting);
    if (vertexShaderCache.find(MakeShaderCacheKey(defaultPath, vs_additional, ShaderCompileVariant::DeferredLighting)) == vertexShaderCache.end() ||
        pixelShaderCache.find(MakeShaderCacheKey(defaultPath, ps_additional, ShaderCompileVariant::DeferredLighting)) == pixelShaderCache.end())
    {
        std::cout << "Deferred lighting shaders are unavailable." << std::endl;
        return false;
    }

    deferredBufferWidth = width;
    deferredBufferHeight = height;
    return true;
}

void Game::FillFrameConstantBuffer(ConstantBufferData& data) const
{
    data = ConstantBufferData{};
    data.worldMatrix = Identity4x4();
    data.normalMatrix = Identity4x4();
    ApplyFrameConstants(data, CurrentFrame);
    data.ObjectColor = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
    data.ReflectionData = DirectX::XMFLOAT4(0.0f, 0.0f, 5.0f, 0.0f);
}

void Game::RenderDeferredLightingPass()
{
    if (!gBufferAlbedoSRV || !gBufferNormalSRV || !gBufferWorldPositionSRV || !deferredLightingCB)
    {
        return;
    }

    const std::string defaultPath = DefaultShaderPath();
    ID3D11VertexShader* deferredVS = GetVertexShader(defaultPath, ShaderCompileVariant::DeferredLighting);
    ID3D11PixelShader* deferredPS = GetPixelShader(defaultPath, ShaderCompileVariant::DeferredLighting);
    if (deferredVS == nullptr || deferredPS == nullptr)
    {
        return;
    }

    // Same ambient/lights/shadows as the forward materials (no deferred-only compensation).
    DeferredLightingBufferData data;
    FillFrameConstantBuffer(data);
    Context->UpdateSubresource(deferredLightingCB.Get(), 0, nullptr, &data, 0, 0);

    const float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    Context->RSSetState(DefaultRasterState.Get());
    Context->IASetInputLayout(nullptr);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->OMSetRenderTargets(1, RenderTargetView.GetAddressOf(), nullptr);
    Context->OMSetBlendState(opaqueBlendState.Get(), blendFactor, 0xffffffff);

    ID3D11ShaderResourceView* gBufferSRVs[4] =
    {
        gBufferAlbedoSRV.Get(),
        gBufferNormalSRV.Get(),
        gBufferWorldPositionSRV.Get(),
        depthStencilSRV.Get()
    };

    Context->VSSetShader(deferredVS, nullptr, 0);
    Context->PSSetShader(deferredPS, nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, deferredLightingCB.GetAddressOf());
    Context->PSSetConstantBuffers(0, 1, deferredLightingCB.GetAddressOf());
    Context->PSSetShaderResources(0, 4, gBufferSRVs);
    Context->PSSetSamplers(0, 1, gBufferSamplerState.GetAddressOf());

    if (AreShadowsActive())
    {
        ID3D11ShaderResourceView* shadowMapSRV = shadowSRV.Get();
        Context->PSSetShaderResources(4, 1, &shadowMapSRV);
        Context->PSSetSamplers(4, 1, shadowSampler.GetAddressOf());
    }

    Context->Draw(3, 0);
    CountDraw(3);

    ID3D11ShaderResourceView* nullSRVs[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    Context->PSSetShaderResources(0, 5, nullSRVs);
}

void Game::RenderDeferredDebugOverlay()
{
    if (!gBufferAlbedoSRV || !gBufferNormalSRV || !gBufferWorldPositionSRV || !deferredLightingCB)
    {
        return;
    }

    const std::string defaultPath = DefaultShaderPath();
    ID3D11VertexShader* deferredVS = GetVertexShader(defaultPath, ShaderCompileVariant::DeferredLighting);
    ID3D11PixelShader* deferredPS = GetPixelShader(defaultPath, ShaderCompileVariant::DeferredLighting);
    if (deferredVS == nullptr || deferredPS == nullptr)
    {
        return;
    }

    const float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    Context->RSSetState(DefaultRasterState.Get());
    Context->IASetInputLayout(nullptr);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->OMSetRenderTargets(1, RenderTargetView.GetAddressOf(), nullptr);
    Context->OMSetBlendState(opaqueBlendState.Get(), blendFactor, 0xffffffff);

    ID3D11ShaderResourceView* gBufferSRVs[4] =
    {
        gBufferAlbedoSRV.Get(),
        gBufferNormalSRV.Get(),
        gBufferWorldPositionSRV.Get(),
        depthStencilSRV.Get()
    };

    Context->VSSetShader(deferredVS, nullptr, 0);
    Context->PSSetShader(deferredPS, nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, deferredLightingCB.GetAddressOf());
    Context->PSSetConstantBuffers(0, 1, deferredLightingCB.GetAddressOf());
    Context->PSSetShaderResources(0, 4, gBufferSRVs);
    Context->PSSetSamplers(0, 1, gBufferSamplerState.GetAddressOf());

    const float screenWidth = static_cast<float>(DisplayPtr->GetWidth());
    const float screenHeight = static_cast<float>(DisplayPtr->GetHeight());
    const float debugWidth = std::max(160.0f, screenWidth * 0.22f);
    const float debugHeight = std::max(90.0f, screenHeight * 0.22f);
    const float startX = screenWidth - debugWidth * 2.0f - 24.0f;
    const float startY = 24.0f;

    // The debug views only differ in the mode selector, so the frame data is prepared once.
    DeferredLightingBufferData debugData;
    FillFrameConstantBuffer(debugData);

    for (int debugMode = 1; debugMode <= 4; ++debugMode)
    {
        D3D11_VIEWPORT viewport = {};
        viewport.Width = debugWidth;
        viewport.Height = debugHeight;
        viewport.MaxDepth = 1.0f;
        viewport.TopLeftX = startX + ((debugMode - 1) % 2) * (debugWidth + 8.0f);
        viewport.TopLeftY = startY + ((debugMode - 1) / 2) * (debugHeight + 8.0f);

        debugData.ObjectColor.x = static_cast<float>(debugMode);
        Context->RSSetViewports(1, &viewport);
        Context->UpdateSubresource(deferredLightingCB.Get(), 0, nullptr, &debugData, 0, 0);
        Context->Draw(3, 0);
        CountDraw(3);
    }

    SetFullViewport();

    ID3D11ShaderResourceView* nullSRVs[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    Context->PSSetShaderResources(0, 5, nullSRVs);
}

// ---------------------------------------------------------------------------------------------
// Shadows
// ---------------------------------------------------------------------------------------------

bool Game::InitShadowResources()
{
    if (!Device || !Context)
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

    const HWND hWnd = DisplayPtr ? DisplayPtr->GetHwnd() : nullptr;
    hr = CreateShader(hWnd, nullptr, shadowShaderPath.c_str(), "VSMain", "vs_5_0", shadowVertexShaderBlob.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to compile shadow VS." << std::endl;
        return false;
    }

    hr = Device->CreateVertexShader(shadowVertexShaderBlob->GetBufferPointer(), shadowVertexShaderBlob->GetBufferSize(),
                                    nullptr, shadowVertexShader.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create shadow VS." << std::endl;
        return false;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> instancedBlob;
    hr = CreateShader(hWnd, nullptr, shadowShaderPath.c_str(), "VSMainInstanced", "vs_5_0", instancedBlob.GetAddressOf());
    if (FAILED(hr) ||
        FAILED(Device->CreateVertexShader(instancedBlob->GetBufferPointer(), instancedBlob->GetBufferSize(),
                                          nullptr, shadowInstancedVertexShader.ReleaseAndGetAddressOf())))
    {
        std::cout << "Failed to create instanced shadow VS." << std::endl;
        return false;
    }

    const D3D11_INPUT_ELEMENT_DESC primitiveLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    hr = Device->CreateInputLayout(primitiveLayoutDesc, ARRAYSIZE(primitiveLayoutDesc),
                                   shadowVertexShaderBlob->GetBufferPointer(), shadowVertexShaderBlob->GetBufferSize(),
                                   shadowInputLayoutPrimitive.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cout << "Failed to create shadow primitive input layout." << std::endl;
        return false;
    }

    const D3D11_INPUT_ELEMENT_DESC meshLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    hr = Device->CreateInputLayout(meshLayoutDesc, ARRAYSIZE(meshLayoutDesc),
                                   shadowVertexShaderBlob->GetBufferPointer(), shadowVertexShaderBlob->GetBufferSize(),
                                   shadowInputLayoutMesh.ReleaseAndGetAddressOf());
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

void Game::UpdateShadowCascades()
{
    using namespace DirectX;

    if (!AreShadowsActive() || !FirstPlayer || !DisplayPtr)
    {
        return;
    }

    const float nearPlane = FirstPlayer->GetNearPlane();
    const float farPlane = ShadowDistance;
    const float aspect = DisplayPtr->GetWidth() / static_cast<float>(std::max(DisplayPtr->GetHeight(), 1));
    const float tanHalfFovY = std::tan(FirstPlayer->GetFovY() * 0.5f);

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
    const XMVECTOR lightDirV = XMVector3Normalize(XMVectorSet(lightDirection.x, lightDirection.y, lightDirection.z, 0.0f));

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
            radius = std::max(radius, XMVectorGetX(XMVector3Length(XMVectorSubtract(corner, centroid))));
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
            minX = std::min(minX, XMVectorGetX(cornerLS));
            maxX = std::max(maxX, XMVectorGetX(cornerLS));
            minY = std::min(minY, XMVectorGetY(cornerLS));
            maxY = std::max(maxY, XMVectorGetY(cornerLS));
            minZ = std::min(minZ, XMVectorGetZ(cornerLS));
            maxZ = std::max(maxZ, XMVectorGetZ(cornerLS));
        }

        const float depthPadding = std::max(80.0f, radius * 0.45f);
        minZ -= depthPadding;
        maxZ += depthPadding;

        const XMMATRIX lightProjection = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);
        XMStoreFloat4x4(&CascadeLightViewProjection[cascadeIndex], XMMatrixTranspose(XMMatrixMultiply(lightView, lightProjection)));
    }
}

void Game::RenderShadowMaps()
{
    if (!AreShadowsActive())
    {
        return;
    }

    ID3D11ShaderResourceView* nullShadowSRV = nullptr;
    Context->PSSetShaderResources(4, 1, &nullShadowSRV);

    D3D11_VIEWPORT shadowViewport = {};
    shadowViewport.Width = static_cast<float>(ShadowMapSize);
    shadowViewport.Height = static_cast<float>(ShadowMapSize);
    shadowViewport.MaxDepth = 1.0f;

    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->RSSetState(shadowRasterState.Get());
    Context->RSSetViewports(1, &shadowViewport);
    Context->OMSetDepthStencilState(depthStencilState.Get(), 0);

    ShadowPassContext shadowPass;
    shadowPass.VertexShader = shadowVertexShader.Get();
    shadowPass.InstancedVertexShader = shadowInstancedVertexShader.Get();
    shadowPass.ConstantBuffer = shadowPassCB.Get();
    shadowPass.PrimitiveLayout = shadowInputLayoutPrimitive.Get();
    shadowPass.MeshLayout = shadowInputLayoutMesh.Get();

    for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
    {
        ID3D11DepthStencilView* cascadeDSV = shadowDSVs[cascadeIndex].Get();
        Context->OMSetRenderTargets(0, nullptr, cascadeDSV);
        Context->ClearDepthStencilView(cascadeDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
        shadowPass.LightViewProjection = CascadeLightViewProjection[cascadeIndex];

        for (auto& componentPair : Components)
        {
            GameComponent* component = componentPair.second.get();
            if (!component->IsRenderable() || component->IsSkybox())
            {
                continue;
            }
            component->RenderShadow(Context.Get(), shadowPass);
        }
    }

    Context->RSSetState(DefaultRasterState.Get());
    SetFullViewport();
    Context->OMSetRenderTargets(1, RenderTargetView.GetAddressOf(), depthStencilView.Get());
}

// ---------------------------------------------------------------------------------------------
// Frame loop
// ---------------------------------------------------------------------------------------------

int Game::StartGame()
{
    if (!bInitialized)
    {
        std::cout << "StartGame called without a successful Initialize." << std::endl;
        return 1;
    }

    Run();
    return ExitCode;
}

bool Game::PumpMessages()
{
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            ExitCode = static_cast<int>(msg.wParam);
            bRunning = false;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return bRunning;
}

void Game::Run()
{
    using Clock = std::chrono::steady_clock;

    bRunning = true;
    auto prevTime = Clock::now();

    while (bRunning)
    {
        // A quit request ends the loop before any further update, draw or present.
        if (!PumpMessages())
        {
            break;
        }

        ApplyPendingResize();
        if (!bRunning)
        {
            break;
        }
        if (bMinimized)
        {
            WaitMessage();
            prevTime = Clock::now();
            continue;
        }

        const auto frameStart = Clock::now();
        float deltaTime = std::chrono::duration<float>(frameStart - prevTime).count();
        prevTime = frameStart;
        deltaTime = std::min(deltaTime, MaxFrameDeltaSeconds);

        ++FrameIndex;
        Stats.Reset();

        UpdateFrame(deltaTime);

        BeginGpuTimer();
        RenderFrame();
        EndGpuTimer();

        CpuFrameMsAccumulator += std::chrono::duration<double, std::milli>(Clock::now() - frameStart).count();
        EndFrame();

        if (InputDevicePtr)
        {
            InputDevicePtr->EndFrame();
        }
        LastFrameStats = Stats;
        UpdateWindowTitle(deltaTime);
    }

    bRunning = false;
    if (Context)
    {
        Context->ClearState();
        Context->Flush();
    }
}

void Game::OnWindowResized(int clientWidth, int clientHeight, bool minimized)
{
    bMinimized = minimized || clientWidth <= 0 || clientHeight <= 0;
    if (!bMinimized)
    {
        PendingWidth = clientWidth;
        PendingHeight = clientHeight;
        bResizePending = true;
    }
}

void Game::OnFocusLost()
{
    if (InputDevicePtr)
    {
        InputDevicePtr->ClearPressedKeys();
    }
}

void Game::ApplyPendingResize()
{
    if (!bResizePending || !SwapChain || !DisplayPtr)
    {
        return;
    }
    bResizePending = false;

    if (PendingWidth == DisplayPtr->GetWidth() && PendingHeight == DisplayPtr->GetHeight() && RenderTargetView)
    {
        return;
    }

    // Every view of the old back buffer must be released before ResizeBuffers.
    Context->ClearState();
    ReleaseSwapChainResources();
    Context->Flush();

    const HRESULT hr = SwapChain->ResizeBuffers(0, static_cast<UINT>(PendingWidth), static_cast<UINT>(PendingHeight),
                                                DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
    {
        std::cout << "ResizeBuffers failed: 0x" << std::hex << hr << std::dec << std::endl;
        bRunning = false;
        ExitCode = 3;
        return;
    }

    DisplayPtr->SetClientSize(PendingWidth, PendingHeight);
    if (!CreateSwapChainResources(PendingWidth, PendingHeight))
    {
        bRunning = false;
        ExitCode = 3;
    }
}

void Game::ProcessHotkeys()
{
    if (!InputDevicePtr)
    {
        return;
    }

    // F3: deferred debug overlay, F4: VSync, F5: forward <-> deferred.
    const bool overlayDown = InputDevicePtr->IsKeyDown(Keys::F3);
    if (overlayDown && !bOverlayKeyWasDown)
    {
        bShowDeferredDebugOverlay = !bShowDeferredDebugOverlay;
    }
    bOverlayKeyWasDown = overlayDown;

    const bool vsyncDown = InputDevicePtr->IsKeyDown(Keys::F4);
    if (vsyncDown && !bVSyncKeyWasDown)
    {
        bVSync = !bVSync;
    }
    bVSyncKeyWasDown = vsyncDown;

    const bool renderTypeDown = InputDevicePtr->IsKeyDown(Keys::F5);
    if (renderTypeDown && !bRenderTypeKeyWasDown && RenderingType != Custom)
    {
        RenderingType = RenderingType == Forward ? Deffered : Forward;
    }
    bRenderTypeKeyWasDown = renderTypeDown;
}

void Game::UpdateFrame(float realDeltaTime)
{
    const float deltaTime = realDeltaTime * SimulationTimeScale;
    TotalTime += deltaTime;

    ProcessHotkeys();

    // camera -> world transforms -> lights/cascades -> frame constants -> object constants
    PreUpdate(deltaTime);
    FirstPlayer->UpdateCamera(deltaTime);
    TickComponents(deltaTime);

    if (bShadowsEnabled && !bShadowResourcesReady && !bShadowInitFailed)
    {
        bShadowResourcesReady = InitShadowResources();
        bShadowInitFailed = !bShadowResourcesReady;
    }
    UpdateShadowCascades();
    BuildFrameConstants();

    for (auto& pair : Components)
    {
        pair.second->Update();
    }
}

void Game::TickComponents(float deltaTime)
{
    const bool bPaused = InputDevicePtr && InputDevicePtr->IsKeyDown(Keys::Space);
    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second.get();
        Component->Tick(bPaused && !Component->IsSkybox() ? 0.0f : deltaTime);
    }
}

void Game::BuildFrameConstants()
{
    using namespace DirectX;

    FrameConstants& frame = CurrentFrame;
    frame.viewMatrix = FirstPlayer->GetViewMatrix();
    frame.projectionMatrix = FirstPlayer->GetProjectionMatrix();

    const XMMATRIX view = XMMatrixTranspose(XMLoadFloat4x4(&frame.viewMatrix));
    const XMMATRIX projection = XMMatrixTranspose(XMLoadFloat4x4(&frame.projectionMatrix));
    XMStoreFloat4x4(&frame.invViewMatrix, XMMatrixTranspose(XMMatrixInverse(nullptr, view)));
    XMStoreFloat4x4(&frame.invProjectionMatrix, XMMatrixTranspose(XMMatrixInverse(nullptr, projection)));

    const glm::vec3 cameraPosition = FirstPlayer->GetPosition();
    frame.CameraPosition = XMFLOAT4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f);

    for (int i = 0; i < MaxPointLights; ++i)
    {
        frame.LightPositions[i] = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        frame.LightColors[i] = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        frame.LightParams[i] = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
    }
    const std::vector<PointLightInfo> pointLights = GetPointLights(MaxPointLights);
    const int lightCount = static_cast<int>(pointLights.size());
    for (int i = 0; i < lightCount; ++i)
    {
        const PointLightInfo& light = pointLights[i];
        frame.LightPositions[i] = XMFLOAT4(light.Position.x, light.Position.y, light.Position.z, 1.0f);
        frame.LightColors[i] = XMFLOAT4(light.Color.x, light.Color.y, light.Color.z, 1.0f);
        frame.LightParams[i] = XMFLOAT4(light.Intensity, light.Radius, light.bEnabled ? 1.0f : 0.0f, 0.0f);
    }
    frame.LightMeta = XMFLOAT4(static_cast<float>(lightCount), AmbientIntensity, SpecularShininess, 0.0f);

    // The directional light does not depend on shadows; shadow data does.
    frame.LightDirection = DirectionalLightDirection;
    frame.DirectionalLightColorIntensity = DirectionalLightColorIntensity;
    const bool bShadows = AreShadowsActive();
    for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
    {
        frame.LightViewProjection[cascadeIndex] = bShadows ? CascadeLightViewProjection[cascadeIndex] : Identity4x4();
    }
    frame.CascadeSplits = bShadows ? CascadeSplits : XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    frame.ShadowParams = bShadows ? ShadowParams : XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    frame.ShadowParams.x = bShadows ? 1.0f : 0.0f;

    // Frustum planes of clip = v * (V * P): columns combined, normalised.
    const XMMATRIX viewProjection = XMMatrixTranspose(XMMatrixMultiply(view, projection));
    const XMVECTOR column0 = viewProjection.r[0];
    const XMVECTOR column1 = viewProjection.r[1];
    const XMVECTOR column2 = viewProjection.r[2];
    const XMVECTOR column3 = viewProjection.r[3];
    const XMVECTOR planes[6] = {
        XMVectorAdd(column3, column0),      // left
        XMVectorSubtract(column3, column0), // right
        XMVectorAdd(column3, column1),      // bottom
        XMVectorSubtract(column3, column1), // top
        column2,                            // near (D3D clip z >= 0)
        XMVectorSubtract(column3, column2)  // far
    };
    for (int i = 0; i < 6; ++i)
    {
        XMStoreFloat4(&frame.FrustumPlanes[i], XMPlaneNormalize(planes[i]));
    }
}

void Game::RenderFrame()
{
    switch (RenderingType)
    {
    case Deffered:
        DrawDeffered();
        break;
    case Custom:
        Draw();
        break;
    case Forward:
    default:
        DrawForward();
        break;
    }
}

void Game::EndFrame()
{
    if (!SwapChain)
    {
        return;
    }

    const HRESULT hr = SwapChain->Present(bVSync ? 1 : 0, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        const HRESULT reason = Device ? Device->GetDeviceRemovedReason() : hr;
        std::cout << "GPU device lost (0x" << std::hex << reason << std::dec << "), stopping." << std::endl;
        bRunning = false;
        ExitCode = 2;
    }
    else if (FAILED(hr))
    {
        std::cout << "Present failed: 0x" << std::hex << hr << std::dec << std::endl;
        bRunning = false;
        ExitCode = 2;
    }
}

void Game::UpdateWindowTitle(float realDeltaTime)
{
    TitleUpdateAccumulator += realDeltaTime;
    ++TitleFrameCount;
    if (TitleUpdateAccumulator < 0.5f || !DisplayPtr)
    {
        return;
    }

    size_t instanceCount = 0;
    for (const auto& pair : Components)
    {
        instanceCount += pair.second->GetInstanceCount();
    }

    const float fps = TitleFrameCount / TitleUpdateAccumulator;
    const double cpuMs = CpuFrameMsAccumulator / std::max<uint32_t>(TitleFrameCount, 1);
    const wchar_t* renderName = RenderingType == Deffered ? L"Deferred" : (RenderingType == Custom ? L"Custom" : L"Forward");

    WCHAR text[256];
    swprintf_s(text, L"%s | FPS: %.1f | CPU %.2f ms | GPU %.2f ms | Objects: %zu (+%zu inst) | Draws: %u | Culled: %u%s",
               renderName, fps, cpuMs, LastGpuFrameMs, Components.size(), instanceCount,
               LastFrameStats.DrawCalls, LastFrameStats.CulledObjects, bVSync ? L" | VSync" : L"");
    SetWindowText(DisplayPtr->GetHwnd(), text);

    TitleUpdateAccumulator = 0.0f;
    TitleFrameCount = 0;
    CpuFrameMsAccumulator = 0.0;
}

// ---------------------------------------------------------------------------------------------
// Render passes
// ---------------------------------------------------------------------------------------------

void Game::SetFullViewport()
{
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(DisplayPtr->GetWidth());
    viewport.Height = static_cast<float>(DisplayPtr->GetHeight());
    viewport.MaxDepth = 1.0f;
    Context->RSSetViewports(1, &viewport);
}

void Game::BeginMainPass(bool bUseDepth)
{
    // The renderer is the single owner of clears: nothing else clears the back buffer or depth.
    Context->ClearState();
    Context->RSSetState(DefaultRasterState.Get());
    SetFullViewport();
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const std::array<float, 4> clearColor = GetClearColor();
    Context->ClearRenderTargetView(RenderTargetView.Get(), clearColor.data());
    if (bUseDepth)
    {
        Context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        Context->OMSetRenderTargets(1, RenderTargetView.GetAddressOf(), depthStencilView.Get());
        Context->OMSetDepthStencilState(depthStencilState.Get(), 0);
    }
    else
    {
        Context->OMSetRenderTargets(1, RenderTargetView.GetAddressOf(), nullptr);
    }

    const float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    Context->OMSetBlendState(opaqueBlendState.Get(), blendFactor, 0xffffffff);
}

void Game::Draw()
{
    DrawForward();
}

void Game::DrawForward()
{
    BeginMainPass(true);
    RenderShadowMaps();
    RenderOpaque(OpaqueFilter::All, ShaderCompileVariant::Default);
    RenderSkybox();
    DispatchComputePhase();
    RenderTransparent();
}

void Game::DrawDeffered()
{
    if (!InitDeferredResources())
    {
        // Shader variants are chosen per pass, so the forward path is a valid fallback.
        if (!bDeferredFailureReported)
        {
            std::cout << "Deferred renderer is unavailable, rendering forward instead." << std::endl;
            bDeferredFailureReported = true;
        }
        DrawForward();
        return;
    }

    Context->ClearState();
    Context->RSSetState(DefaultRasterState.Get());
    SetFullViewport();
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    RenderShadowMaps();

    // 1. Geometry pass: only materials that really write the G-buffer.
    ID3D11RenderTargetView* gBufferTargets[3] =
    {
        gBufferAlbedoRTV.Get(),
        gBufferNormalRTV.Get(),
        gBufferWorldPositionRTV.Get()
    };
    Context->OMSetRenderTargets(3, gBufferTargets, depthStencilView.Get());

    const float clearAlbedo[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float clearNormal[4] = {0.5f, 0.5f, 1.0f, 0.0f};
    const float clearWorldPosition[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    Context->ClearRenderTargetView(gBufferAlbedoRTV.Get(), clearAlbedo);
    Context->ClearRenderTargetView(gBufferNormalRTV.Get(), clearNormal);
    Context->ClearRenderTargetView(gBufferWorldPositionRTV.Get(), clearWorldPosition);
    Context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    RenderOpaque(OpaqueFilter::DeferredCapable, ShaderCompileVariant::DeferredGBuffer);

    // 2. Lighting into the back buffer; background pixels keep the clear colour.
    const std::array<float, 4> clearColor = GetClearColor();
    Context->ClearRenderTargetView(RenderTargetView.Get(), clearColor.data());
    RenderDeferredLightingPass();

    // 3. Forward-only opaque materials (reflective, emissive, custom) against the G-buffer depth.
    Context->RSSetState(DefaultRasterState.Get());
    SetFullViewport();
    Context->OMSetRenderTargets(1, RenderTargetView.GetAddressOf(), depthStencilView.Get());
    RenderOpaque(OpaqueFilter::ForwardOnly, ShaderCompileVariant::Default);

    // 4. Skybox, GPU simulation, transparency.
    RenderSkybox();
    DispatchComputePhase();
    RenderTransparent();

    if (bShowDeferredDebugOverlay)
    {
        RenderDeferredDebugOverlay();
    }
}

bool Game::IsVisible(GameComponent* Component)
{
    if (!bFrustumCulling || !Component->SupportsFrustumCulling())
    {
        ++Stats.VisibleObjects;
        return true;
    }

    const float radius = Component->GetWorldBoundingRadius();
    if (radius <= 0.0f)
    {
        ++Stats.VisibleObjects;
        return true;
    }

    const glm::vec3 center = Component->GetWorldPosition();
    for (const DirectX::XMFLOAT4& plane : CurrentFrame.FrustumPlanes)
    {
        if (plane.x * center.x + plane.y * center.y + plane.z * center.z + plane.w < -radius)
        {
            ++Stats.CulledObjects;
            return false;
        }
    }

    ++Stats.VisibleObjects;
    return true;
}

void Game::RenderOpaque(OpaqueFilter filter, ShaderCompileVariant variant)
{
    const float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    Context->OMSetDepthStencilState(depthStencilState.Get(), 0);
    Context->OMSetBlendState(opaqueBlendState.Get(), blendFactor, 0xffffffff);

    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second.get();
        if (!Component->IsRenderable() || Component->IsSkybox() || Component->HasOpacity())
        {
            continue;
        }

        if (filter != OpaqueFilter::All)
        {
            const bool bDeferredCapable = SupportsDeferredGeometry(Component);
            if ((filter == OpaqueFilter::DeferredCapable) != bDeferredCapable)
            {
                continue;
            }
        }

        if (!IsVisible(Component) || !BindComponentShaders(Component, variant))
        {
            continue;
        }
        Component->Render(Context.Get());
    }
}

void Game::RenderSkybox()
{
    // Skybox is drawn after opaque geometry at the far plane: it only fills empty pixels.
    Context->OMSetDepthStencilState(depthStencilStateSkybox.Get(), 0);
    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second.get();
        if (!Component->IsSkybox() || !Component->IsRenderable())
        {
            continue;
        }
        if (BindComponentShaders(Component, ShaderCompileVariant::Default))
        {
            Component->Render(Context.Get());
        }
    }
    Context->OMSetDepthStencilState(depthStencilState.Get(), 0);
}

void Game::DispatchComputePhase()
{
    for (auto& pair : Components)
    {
        pair.second->DispatchCompute(Context.Get());
    }

    // Compute work may unbind the output merger to read the depth buffer.
    Context->OMSetRenderTargets(1, RenderTargetView.GetAddressOf(), depthStencilView.Get());
}

void Game::RenderTransparent()
{
    struct SortedDraw
    {
        float DistanceSq;
        GameComponent* Component;
    };

    std::vector<SortedDraw> transparentDraws;
    const glm::vec3 cameraPosition(CurrentFrame.CameraPosition.x, CurrentFrame.CameraPosition.y, CurrentFrame.CameraPosition.z);
    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second.get();
        if (!Component->IsRenderable() || Component->IsSkybox() || !Component->HasOpacity() || !IsVisible(Component))
        {
            continue;
        }
        const glm::vec3 toCamera = Component->GetWorldPosition() - cameraPosition;
        transparentDraws.push_back({glm::dot(toCamera, toCamera), Component});
    }

    // Back to front; ties keep registry order. Intersecting transparent meshes are not resolved.
    std::stable_sort(transparentDraws.begin(), transparentDraws.end(),
                     [](const SortedDraw& a, const SortedDraw& b) { return a.DistanceSq > b.DistanceSq; });

    const float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    Context->OMSetDepthStencilState(depthStencilStateReadOnly.Get(), 0);
    Context->OMSetBlendState(transparentBlendState.Get(), blendFactor, 0xffffffff);

    for (const SortedDraw& draw : transparentDraws)
    {
        if (BindComponentShaders(draw.Component, ShaderCompileVariant::Default))
        {
            draw.Component->Render(Context.Get());
        }
    }

    Context->OMSetBlendState(opaqueBlendState.Get(), blendFactor, 0xffffffff);
    Context->OMSetDepthStencilState(depthStencilState.Get(), 0);
}

// ---------------------------------------------------------------------------------------------
// GPU timing
// ---------------------------------------------------------------------------------------------

void Game::CreateGpuTimer()
{
    D3D11_QUERY_DESC disjointDesc = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    D3D11_QUERY_DESC timestampDesc = {D3D11_QUERY_TIMESTAMP, 0};
    for (int i = 0; i < GpuTimerLatency; ++i)
    {
        if (FAILED(Device->CreateQuery(&disjointDesc, GpuTimerDisjoint[i].ReleaseAndGetAddressOf())) ||
            FAILED(Device->CreateQuery(&timestampDesc, GpuTimerBegin[i].ReleaseAndGetAddressOf())) ||
            FAILED(Device->CreateQuery(&timestampDesc, GpuTimerEnd[i].ReleaseAndGetAddressOf())))
        {
            // Timing is diagnostic only; the renderer works without it.
            for (int j = 0; j < GpuTimerLatency; ++j)
            {
                GpuTimerDisjoint[j].Reset();
                GpuTimerBegin[j].Reset();
                GpuTimerEnd[j].Reset();
            }
            return;
        }
    }
}

void Game::BeginGpuTimer()
{
    GpuTimerSlot = static_cast<int>(FrameIndex % GpuTimerLatency);
    const int slot = GpuTimerSlot;
    if (!GpuTimerDisjoint[slot])
    {
        return;
    }

    // Results of the frame issued GpuTimerLatency frames ago; never wait for the GPU.
    if (GpuTimerIssued[slot])
    {
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = {};
        UINT64 begin = 0;
        UINT64 end = 0;
        if (Context->GetData(GpuTimerDisjoint[slot].Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
            Context->GetData(GpuTimerBegin[slot].Get(), &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
            Context->GetData(GpuTimerEnd[slot].Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
            !disjoint.Disjoint && disjoint.Frequency > 0 && end >= begin)
        {
            LastGpuFrameMs = static_cast<float>(static_cast<double>(end - begin) * 1000.0 / static_cast<double>(disjoint.Frequency));
        }
        GpuTimerIssued[slot] = false;
    }

    Context->Begin(GpuTimerDisjoint[slot].Get());
    Context->End(GpuTimerBegin[slot].Get());
}

void Game::EndGpuTimer()
{
    const int slot = GpuTimerSlot;
    if (!GpuTimerDisjoint[slot])
    {
        return;
    }

    Context->End(GpuTimerEnd[slot].Get());
    Context->End(GpuTimerDisjoint[slot].Get());
    GpuTimerIssued[slot] = true;
}
