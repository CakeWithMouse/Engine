#pragma once

#include <Engine/Resources/Paths.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ShadowCascadeSelectionTests
{
    using Microsoft::WRL::ComPtr;

    inline void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    // D3DCompile's standard include handler resolves relative to the process working directory.
    // Route the shared production helper include through EnginePaths so the test works from the
    // normal EngineTests launch directory too.
    class ProductionShaderInclude final : public ID3DInclude
    {
        std::string lastError;
    public:
        const std::string& LastError() const { return lastError; }

        HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR fileName, LPCVOID,
                                       LPCVOID* data, UINT* bytes) override
        {
            if (!fileName || std::string(fileName) != "ShadowDepth.hlsli" || !data || !bytes)
            {
                lastError = "Unexpected production shader include request";
                return E_FAIL;
            }
            std::ifstream stream(EnginePaths::Shader("ShadowDepth.hlsli"), std::ios::binary);
            if (!stream)
            {
                lastError = "Cannot open production include at " + EnginePaths::Shader("ShadowDepth.hlsli").string();
                return E_FAIL;
            }
            std::vector<char> contents((std::istreambuf_iterator<char>(stream)),
                                       std::istreambuf_iterator<char>());
            if (contents.empty()) return E_FAIL;
            char* allocation = new char[contents.size()];
            std::copy(contents.begin(), contents.end(), allocation);
            *data = allocation;
            *bytes = static_cast<UINT>(contents.size());
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Close(LPCVOID data) override
        {
            delete[] static_cast<const char*>(data);
            return S_OK;
        }
    };

    /**
     * Compile and draw a tiny pixel shader which includes the production ShadowDepth.hlsli.
     * Pixel 0 is an on-axis view-space point (0,0,90); pixel 1 is off-axis (60,0,90).
     * Pixel 2 is world (100,0,70) viewed from (10,0,0) along +X, also view depth 90.
     * All must select cascade 0 for splits 100/200/300. The old radial-distance behavior
     * instead puts the second point at 108.17 (cascade 1).
     */
    inline void OffAxisAndOnAxisSelectByViewDepth(ID3D11Device* device,
                                                 ID3D11DeviceContext* context)
    {
        Require(device && context, "Shadow cascade selection test needs a D3D11 device/context");
        static const char shaderSource[] = R"hlsl(
#include "ShadowDepth.hlsli"
struct VSOut { float4 position : SV_POSITION; };
VSOut VSMain(uint vertexId : SV_VertexID)
{
    VSOut output;
    float2 p = vertexId == 0 ? float2(-1.0, 1.0) :
               (vertexId == 1 ? float2(3.0, 1.0) : float2(-1.0, -3.0));
    output.position = float4(p, 0.0, 1.0);
    return output;
}
float4 PSMain(VSOut input) : SV_TARGET
{
    float4x4 identityView = float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
    float4x4 translatedRotatedView = float4x4(0,0,1,0, 0,1,0,0, -1,0,0,0, 0,0,-10,1);
    float4 splits = float4(100.0, 200.0, 300.0, 0.0);
    float3 worldPoint = input.position.x < 1.0 ? float3(0.0,0.0,90.0) :
                        (input.position.x < 2.0 ? float3(60.0,0.0,90.0) : float3(100.0,0.0,70.0));
    float depth = input.position.x < 2.0 ? ShadowViewDepth(worldPoint, identityView) :
                                           ShadowViewDepth(worldPoint, translatedRotatedView);
    int cascade = SelectShadowCascade(depth, splits);
    return float4((cascade + 1) / 255.0, 0.0, 0.0, 1.0);
}
)hlsl";

        ProductionShaderInclude include;
        ComPtr<ID3DBlob> vertexBlob, pixelBlob, errors;
        HRESULT hr = D3DCompile(shaderSource, sizeof(shaderSource) - 1, "ShadowCascadeSelectionTest.hlsl",
                                nullptr, &include, "VSMain", "vs_5_0", 0, 0,
                                vertexBlob.GetAddressOf(), errors.GetAddressOf());
        if (FAILED(hr))
        {
            std::ostringstream message;
            message << "Compile shadow-selection VS failed (HRESULT 0x" << std::hex << hr << ")";
            if (errors) message << ": " << static_cast<const char*>(errors->GetBufferPointer());
            if (!include.LastError().empty()) message << " [" << include.LastError() << "]";
            throw std::runtime_error(message.str());
        }
        errors.Reset();
        hr = D3DCompile(shaderSource, sizeof(shaderSource) - 1, "ShadowCascadeSelectionTest.hlsl",
                        nullptr, &include, "PSMain", "ps_5_0", 0, 0,
                        pixelBlob.GetAddressOf(), errors.GetAddressOf());
        if (FAILED(hr))
        {
            std::ostringstream message;
            message << "Compile shadow-selection PS failed (HRESULT 0x" << std::hex << hr << ")";
            if (errors) message << ": " << static_cast<const char*>(errors->GetBufferPointer());
            if (!include.LastError().empty()) message << " [" << include.LastError() << "]";
            throw std::runtime_error(message.str());
        }

        ComPtr<ID3D11VertexShader> vertexShader;
        ComPtr<ID3D11PixelShader> pixelShader;
        Require(SUCCEEDED(device->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
                                                     nullptr, vertexShader.GetAddressOf())),
                "Create shadow-selection VS failed");
        Require(SUCCEEDED(device->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(),
                                                    nullptr, pixelShader.GetAddressOf())),
                "Create shadow-selection PS failed");

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = 3; desc.Height = 1; desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target;
        ComPtr<ID3D11RenderTargetView> rtv;
        Require(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, target.GetAddressOf())) &&
                SUCCEEDED(device->CreateRenderTargetView(target.Get(), nullptr, rtv.GetAddressOf())),
                "Create shadow-selection render target failed");

        context->ClearState();
        const float clear[4] = {};
        context->ClearRenderTargetView(rtv.Get(), clear);
        context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
        const D3D11_VIEWPORT viewport{0.0f, 0.0f, 3.0f, 1.0f, 0.0f, 1.0f};
        context->RSSetViewports(1, &viewport);
        D3D11_RASTERIZER_DESC rasterDesc{};
        rasterDesc.FillMode = D3D11_FILL_SOLID;
        rasterDesc.CullMode = D3D11_CULL_NONE;
        rasterDesc.DepthClipEnable = TRUE;
        ComPtr<ID3D11RasterizerState> rasterState;
        Require(SUCCEEDED(device->CreateRasterizerState(&rasterDesc, rasterState.GetAddressOf())),
                "Create shadow-selection rasterizer state failed");
        context->RSSetState(rasterState.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertexShader.Get(), nullptr, 0);
        context->PSSetShader(pixelShader.Get(), nullptr, 0);
        context->Draw(3, 0);

        desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;
        Require(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())),
                "Create shadow-selection readback texture failed");
        context->CopyResource(staging.Get(), target.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Require(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)),
                "Map shadow-selection readback texture failed");
        const auto* row = static_cast<const uint8_t*>(mapped.pData);
        const uint8_t onAxis = row[0];
        const uint8_t offAxis = row[4];
        const uint8_t translatedRotated = row[8];
        context->Unmap(staging.Get(), 0);
        context->ClearState();

        Require(onAxis == 1, "On-axis z=90 point did not select cascade 0");
        Require(offAxis == 1, "Off-axis x=60,z=90 point selected by radial distance instead of view depth");
        Require(translatedRotated == 1,
                "Translated, rotated camera point with view depth 90 did not select cascade 0");
    }
}
