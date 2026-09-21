#pragma once
#include <d3d11.h>
#include <string>
#include <wrl/client.h>

namespace TextureLoader
{
    /**
     * Loads an image as RGBA8 (UNORM, colour values are used as stored) and builds the full mip
     * chain on the GPU. Returns false and leaves outSRV empty on failure.
     */
    bool LoadTexture2D(ID3D11Device* device,
                       ID3D11DeviceContext* context,
                       const std::string& path,
                       bool flipVertically,
                       Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV,
                       int* outWidth = nullptr,
                       int* outHeight = nullptr);

    /** Linear/mip-linear sampler with the given addressing. */
    bool CreateLinearSampler(ID3D11Device* device,
                             D3D11_TEXTURE_ADDRESS_MODE addressUV,
                             Microsoft::WRL::ComPtr<ID3D11SamplerState>& outSampler);
}
