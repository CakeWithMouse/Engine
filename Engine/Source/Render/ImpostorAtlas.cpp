#include <Engine/Render/ImpostorAtlas.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <iostream>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Render/TextureLoader.h>
#include <Engine/Resources/Paths.h>

namespace
{
    struct ImpostorBakeConstants
    {
        DirectX::XMFLOAT4X4 ViewProjection;
        DirectX::XMFLOAT4 BaseColor;
        DirectX::XMFLOAT4 Flags;
    };

    constexpr UINT MeshVertexStride = sizeof(MeshVertex);

    // A sprite is only a few pixels wide, so it must not point-sample a full-size tile: without
    // mips the surviving texels change with the distance to the camera and the field gets rings.
    // Mips stop at this tile size, below which the silhouette would dissolve into one average.
    constexpr int SmallestTileTexels = 16;

    int CountMipLevels(int tileSize)
    {
        int levels = 1;
        int size = tileSize;
        while (size > SmallestTileTexels)
        {
            size /= 2;
            ++levels;
        }
        return levels;
    }
}

MeshBounds ComputeMeshBounds(const MeshLodData& mesh)
{
    MeshBounds bounds;
    if (mesh.Vertices.empty())
    {
        return bounds;
    }

    DirectX::XMFLOAT3 minimum{FLT_MAX, FLT_MAX, FLT_MAX};
    DirectX::XMFLOAT3 maximum{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (const MeshVertex& vertex : mesh.Vertices)
    {
        minimum.x = std::min(minimum.x, vertex.Position.x);
        minimum.y = std::min(minimum.y, vertex.Position.y);
        minimum.z = std::min(minimum.z, vertex.Position.z);
        maximum.x = std::max(maximum.x, vertex.Position.x);
        maximum.y = std::max(maximum.y, vertex.Position.y);
        maximum.z = std::max(maximum.z, vertex.Position.z);
    }

    bounds.Min = minimum;
    bounds.Max = maximum;
    bounds.Center = DirectX::XMFLOAT3((minimum.x + maximum.x) * 0.5f,
                                      (minimum.y + maximum.y) * 0.5f,
                                      (minimum.z + maximum.z) * 0.5f);

    float maxRadiusSq = 0.0f;
    for (const MeshVertex& vertex : mesh.Vertices)
    {
        const float dx = vertex.Position.x - bounds.Center.x;
        const float dy = vertex.Position.y - bounds.Center.y;
        const float dz = vertex.Position.z - bounds.Center.z;
        maxRadiusSq = std::max(maxRadiusSq, dx * dx + dy * dy + dz * dz);
    }
    bounds.Radius = std::sqrt(maxRadiusSq);
    return bounds;
}

bool ImpostorAtlas::Build(Game& game,
                          const MeshLodData& mesh,
                          ID3D11ShaderResourceView* modelTexture,
                          const glm::vec4& baseColor,
                          const ImpostorAtlasSettings& settings)
{
    using namespace DirectX;

    ID3D11Device* device = game.GetDevice();
    ID3D11DeviceContext* context = game.GetContext();
    if (device == nullptr || context == nullptr || mesh.Vertices.empty() || mesh.Indices.empty())
    {
        return false;
    }

    Settings = settings;
    Settings.YawCount = std::max(4, Settings.YawCount);
    Settings.PitchCount = std::max(1, Settings.PitchCount);
    Settings.TileSize = std::max(32, Settings.TileSize);
    Bounds = ComputeMeshBounds(mesh);
    if (Bounds.Radius <= 0.0f)
    {
        return false;
    }
    MipLevels = CountMipLevels(Settings.TileSize);

    const UINT atlasWidth = static_cast<UINT>(Settings.TileSize * Settings.YawCount);
    const UINT atlasHeight = static_cast<UINT>(Settings.TileSize * Settings.PitchCount);

    auto createTarget = [&](DXGI_FORMAT format,
                            Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
                            Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv,
                            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv) -> bool
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = atlasWidth;
        desc.Height = atlasHeight;
        desc.MipLevels = static_cast<UINT>(MipLevels);
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        // Every tile starts at a multiple of its size, so a texel of any of these mips stays
        // inside one tile and the chain never mixes two views of the model.
        CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc(D3D11_RTV_DIMENSION_TEXTURE2D, format, 0);
        return SUCCEEDED(device->CreateTexture2D(&desc, nullptr, texture.ReleaseAndGetAddressOf())) &&
               SUCCEEDED(device->CreateRenderTargetView(texture.Get(), &rtvDesc, rtv.ReleaseAndGetAddressOf())) &&
               SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, srv.ReleaseAndGetAddressOf()));
    };

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> albedoRTV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> normalRTV;
    if (!createTarget(DXGI_FORMAT_R8G8B8A8_UNORM, AlbedoTexture, albedoRTV, AlbedoSRV) ||
        !createTarget(DXGI_FORMAT_R8G8B8A8_UNORM, NormalTexture, normalRTV, NormalSRV))
    {
        std::cout << "ImpostorAtlas: failed to create atlas targets." << std::endl;
        AlbedoSRV.Reset();
        NormalSRV.Reset();
        return false;
    }

    // Depth is only needed while baking, so it is released with this function.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView;
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = atlasWidth;
    depthDesc.Height = atlasHeight;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(device->CreateTexture2D(&depthDesc, nullptr, depthTexture.GetAddressOf())) ||
        FAILED(device->CreateDepthStencilView(depthTexture.Get(), nullptr, depthView.GetAddressOf())))
    {
        std::cout << "ImpostorAtlas: failed to create bake depth buffer." << std::endl;
        AlbedoSRV.Reset();
        NormalSRV.Reset();
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    D3D11_BUFFER_DESC vertexDesc = {};
    vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDesc.ByteWidth = static_cast<UINT>(sizeof(MeshVertex) * mesh.Vertices.size());
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = mesh.Vertices.data();

    D3D11_BUFFER_DESC indexDesc = {};
    indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh.Indices.size());
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = mesh.Indices.data();

    D3D11_BUFFER_DESC constantDesc = {};
    constantDesc.Usage = D3D11_USAGE_DEFAULT;
    constantDesc.ByteWidth = sizeof(ImpostorBakeConstants);
    constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    if (FAILED(device->CreateBuffer(&vertexDesc, &vertexData, vertexBuffer.GetAddressOf())) ||
        FAILED(device->CreateBuffer(&indexDesc, &indexData, indexBuffer.GetAddressOf())) ||
        FAILED(device->CreateBuffer(&constantDesc, nullptr, constantBuffer.GetAddressOf())))
    {
        std::cout << "ImpostorAtlas: failed to create bake buffers." << std::endl;
        AlbedoSRV.Reset();
        NormalSRV.Reset();
        return false;
    }

    const std::string bakeShader = EnginePaths::Shader("ImpostorBake.hlsl").generic_u8string();
    ID3D11VertexShader* vertexShader = game.GetVertexShader(bakeShader);
    ID3D11PixelShader* pixelShader = game.GetPixelShader(bakeShader);
    if (vertexShader == nullptr || pixelShader == nullptr)
    {
        std::cout << "ImpostorAtlas: bake shaders are unavailable." << std::endl;
        AlbedoSRV.Reset();
        NormalSRV.Reset();
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterState;
    CD3D11_RASTERIZER_DESC rasterDesc(D3D11_DEFAULT);
    rasterDesc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rasterDesc, rasterState.GetAddressOf());

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
    CD3D11_DEPTH_STENCIL_DESC depthStateDesc(D3D11_DEFAULT);
    device->CreateDepthStencilState(&depthStateDesc, depthState.GetAddressOf());

    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
    TextureLoader::CreateLinearSampler(device, D3D11_TEXTURE_ADDRESS_WRAP, sampler);

    // Transparent background: alpha is the coverage mask used by the impostor shader.
    const float clearAlbedo[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float clearNormal[4] = {0.5f, 0.5f, 1.0f, 0.0f};
    context->ClearRenderTargetView(albedoRTV.Get(), clearAlbedo);
    context->ClearRenderTargetView(normalRTV.Get(), clearNormal);

    ID3D11RenderTargetView* targets[2] = {albedoRTV.Get(), normalRTV.Get()};
    context->OMSetRenderTargets(2, targets, depthView.Get());
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    context->OMSetDepthStencilState(depthState.Get(), 0);
    context->RSSetState(rasterState.Get());
    context->IASetInputLayout(game.GetInputLayout(VertexFormat::Mesh));
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* vertexBuffers[] = {vertexBuffer.Get()};
    const UINT stride = MeshVertexStride;
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    context->PSSetShaderResources(0, 1, &modelTexture);
    context->PSSetSamplers(0, 1, sampler.GetAddressOf());

    const XMVECTOR center = XMVectorSet(Bounds.Center.x, Bounds.Center.y, Bounds.Center.z, 1.0f);
    const float distance = Bounds.Radius * 3.0f;
    const float minPitch = XMConvertToRadians(Settings.MinPitchDegrees);
    const float maxPitch = XMConvertToRadians(Settings.MaxPitchDegrees);
    const float pitchStep = Settings.PitchCount > 1 ? (maxPitch - minPitch) / (Settings.PitchCount - 1) : 0.0f;
    const float yawStep = XM_2PI / static_cast<float>(Settings.YawCount);

    for (int pitchIndex = 0; pitchIndex < Settings.PitchCount; ++pitchIndex)
    {
        const float pitch = minPitch + pitchStep * pitchIndex;
        for (int yawIndex = 0; yawIndex < Settings.YawCount; ++yawIndex)
        {
            // Same convention the runtime shader uses: yaw measured from +Z towards +X.
            const float yaw = yawStep * yawIndex;
            const XMVECTOR direction = XMVectorSet(std::sin(yaw) * std::cos(pitch),
                                                   std::sin(pitch),
                                                   std::cos(yaw) * std::cos(pitch), 0.0f);
            const XMVECTOR eye = XMVectorAdd(center, XMVectorScale(direction, distance));
            XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            if (std::abs(std::sin(pitch)) > 0.98f)
            {
                up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
            }

            const XMMATRIX view = XMMatrixLookAtLH(eye, center, up);
            const XMMATRIX projection = XMMatrixOrthographicLH(Bounds.Radius * 2.0f, Bounds.Radius * 2.0f,
                                                               0.01f, distance + Bounds.Radius * 2.0f);

            ImpostorBakeConstants constants = {};
            XMStoreFloat4x4(&constants.ViewProjection, XMMatrixTranspose(XMMatrixMultiply(view, projection)));
            constants.BaseColor = XMFLOAT4(baseColor.x, baseColor.y, baseColor.z, baseColor.w);
            constants.Flags = XMFLOAT4(modelTexture != nullptr ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
            context->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &constants, 0, 0);

            // One pixel of margin keeps filtering from reaching into the neighbouring tile.
            D3D11_VIEWPORT viewport = {};
            viewport.TopLeftX = static_cast<float>(yawIndex * Settings.TileSize) + 1.0f;
            viewport.TopLeftY = static_cast<float>(pitchIndex * Settings.TileSize) + 1.0f;
            viewport.Width = static_cast<float>(Settings.TileSize) - 2.0f;
            viewport.Height = static_cast<float>(Settings.TileSize) - 2.0f;
            viewport.MaxDepth = 1.0f;
            context->RSSetViewports(1, &viewport);

            context->ClearDepthStencilView(depthView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

            context->DrawIndexed(static_cast<UINT>(mesh.Indices.size()), 0, 0);
        }
    }

    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->PSSetShaderResources(0, 1, &nullSRV);
    context->OMSetRenderTargets(0, nullptr, nullptr);

    // Colour is averaged together with the transparent background, so the shader divides it back
    // by the coverage; the normal is renormalised there as well.
    context->GenerateMips(AlbedoSRV.Get());
    context->GenerateMips(NormalSRV.Get());

    std::cout << "ImpostorAtlas: baked " << Settings.YawCount << "x" << Settings.PitchCount
              << " views at " << Settings.TileSize << " px, " << MipLevels << " mips." << std::endl;
    return true;
}

namespace
{
    /** 24-bit BMP, bottom-up like the format wants. */
    bool WriteColorBmp(const std::string& path, const std::vector<uint8_t>& rgb, int width, int height)
    {
        FILE* file = nullptr;
        if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
        {
            return false;
        }

        const int rowSize = ((width * 3 + 3) / 4) * 4;
        const uint32_t pixelBytes = static_cast<uint32_t>(rowSize) * height;
        const uint32_t fileSize = 54 + pixelBytes;
        uint8_t header[54] = {};
        header[0] = 'B';
        header[1] = 'M';
        std::memcpy(header + 2, &fileSize, 4);
        const uint32_t offset = 54;
        std::memcpy(header + 10, &offset, 4);
        const uint32_t infoSize = 40;
        std::memcpy(header + 14, &infoSize, 4);
        std::memcpy(header + 18, &width, 4);
        std::memcpy(header + 22, &height, 4);
        const uint16_t planes = 1;
        const uint16_t bits = 24;
        std::memcpy(header + 26, &planes, 2);
        std::memcpy(header + 28, &bits, 2);
        std::memcpy(header + 34, &pixelBytes, 4);
        std::fwrite(header, 1, sizeof(header), file);

        std::vector<uint8_t> row(static_cast<size_t>(rowSize), 0);
        for (int y = height - 1; y >= 0; --y)
        {
            for (int x = 0; x < width; ++x)
            {
                const size_t source = (static_cast<size_t>(y) * width + x) * 3;
                row[static_cast<size_t>(x) * 3 + 0] = rgb[source + 2];
                row[static_cast<size_t>(x) * 3 + 1] = rgb[source + 1];
                row[static_cast<size_t>(x) * 3 + 2] = rgb[source + 0];
            }
            std::fwrite(row.data(), 1, row.size(), file);
        }
        std::fclose(file);
        return true;
    }
}

bool ImpostorAtlas::Dump(Game& game, const std::string& pathPrefix) const
{
    ID3D11Device* device = game.GetDevice();
    ID3D11DeviceContext* context = game.GetContext();
    if (device == nullptr || context == nullptr || !IsValid())
    {
        return false;
    }

    auto dumpLayer = [&](ID3D11Texture2D* texture, const char* layer) -> bool
    {
        D3D11_TEXTURE2D_DESC desc = {};
        texture->GetDesc(&desc);

        bool ok = true;
        for (UINT mip = 0; mip < desc.MipLevels; ++mip)
        {
            const UINT width = std::max<UINT>(1, desc.Width >> mip);
            const UINT height = std::max<UINT>(1, desc.Height >> mip);

            D3D11_TEXTURE2D_DESC stagingDesc = desc;
            stagingDesc.Width = width;
            stagingDesc.Height = height;
            stagingDesc.MipLevels = 1;
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.BindFlags = 0;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            stagingDesc.MiscFlags = 0;

            Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
            if (FAILED(device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf())))
            {
                ok = false;
                continue;
            }
            context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, texture, mip, nullptr);

            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
            {
                ok = false;
                continue;
            }

            std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3, 0);
            size_t covered = 0;
            for (UINT y = 0; y < height; ++y)
            {
                const uint8_t* row = static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch;
                for (UINT x = 0; x < width; ++x)
                {
                    const uint8_t* texel = row + static_cast<size_t>(x) * 4;
                    const size_t target = (static_cast<size_t>(y) * width + x) * 3;
                    rgb[target + 0] = texel[0];
                    rgb[target + 1] = texel[1];
                    rgb[target + 2] = texel[2];
                    if (texel[3] >= 90)
                    {
                        ++covered;
                    }
                }
            }
            context->Unmap(staging.Get(), 0);

            const std::string file = pathPrefix + "-" + layer + "-mip" + std::to_string(mip) + ".bmp";
            const bool written = WriteColorBmp(file, rgb, static_cast<int>(width), static_cast<int>(height));
            std::cout << "impostor-dump " << layer << " mip=" << mip << " size=" << width << "x" << height
                      << " written=" << (written ? 1 : 0) << " covered=" << covered << std::endl;
            ok = ok && written;
        }
        return ok;
    };

    const bool albedo = dumpLayer(AlbedoTexture.Get(), "albedo");
    const bool normal = dumpLayer(NormalTexture.Get(), "normal");
    return albedo && normal;
}
