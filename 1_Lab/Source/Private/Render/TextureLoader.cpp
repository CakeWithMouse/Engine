#include "../../Public/Render/TextureLoader.h"

#include <iostream>

// UTF-8 file names are opened with _wfopen (paths may contain non-ASCII characters).
#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace TextureLoader
{
    bool LoadTexture2D(ID3D11Device* device,
                       ID3D11DeviceContext* context,
                       const std::string& path,
                       bool flipVertically,
                       Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV,
                       int* outWidth,
                       int* outHeight)
    {
        outSRV.Reset();
        if (device == nullptr || context == nullptr)
        {
            std::cout << "Texture load skipped (no device): " << path << std::endl;
            return false;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
        unsigned char* imageData = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (!imageData)
        {
            std::cout << "Failed to load texture: " << path << std::endl;
            return false;
        }

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = static_cast<UINT>(width);
        texDesc.Height = static_cast<UINT>(height);
        texDesc.MipLevels = 0; // full chain
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, texture.GetAddressOf());
        if (SUCCEEDED(hr))
        {
            context->UpdateSubresource(texture.Get(), 0, nullptr, imageData, static_cast<UINT>(width * 4), 0);
            hr = device->CreateShaderResourceView(texture.Get(), nullptr, outSRV.GetAddressOf());
        }
        stbi_image_free(imageData);

        if (FAILED(hr))
        {
            std::cout << "Failed to create texture resources: " << path << std::endl;
            outSRV.Reset();
            return false;
        }

        context->GenerateMips(outSRV.Get());

        if (outWidth) *outWidth = width;
        if (outHeight) *outHeight = height;
        std::cout << "Texture loaded: " << path << " (" << width << "x" << height << ")" << std::endl;
        return true;
    }

    bool CreateLinearSampler(ID3D11Device* device,
                             D3D11_TEXTURE_ADDRESS_MODE addressUV,
                             Microsoft::WRL::ComPtr<ID3D11SamplerState>& outSampler)
    {
        if (device == nullptr)
        {
            return false;
        }

        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = addressUV;
        sampDesc.AddressV = addressUV;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
        return SUCCEEDED(device->CreateSamplerState(&sampDesc, outSampler.ReleaseAndGetAddressOf()));
    }
}
