#include "InstancedMeshInternal.h"
#include <Engine/Render/Material.h>

using namespace InstancedMeshDetail;

void InstancedMeshComponent::SetImpostorEnabled(bool enabled, const ImpostorAtlasSettings& settings)
{
    bImpostorEnabled = enabled;
    ImpostorSettings = settings;
    bImpostorBuildFailed = false;
    if (enabled && GamePtr && GamePtr->GetDevice())
    {
        BuildImpostorAtlas();
    }
}

int InstancedMeshComponent::GetImpostorLevelIndex() const
{
    return HasImpostor() ? LodCount : -1;
}

int InstancedMeshComponent::GetLevelCount() const
{
    return LodCount + (HasImpostor() ? 1 : 0);
}

void InstancedMeshComponent::Tick(float deltaTime)
{
    GameComponent::Tick(deltaTime);

    const Material* material = GetMaterial();
    const bool bStale = material != BakedMaterial || (material && material->GetRevision() != BakedMaterialRevision);
    if (bImpostorEnabled && LodCount > 0 && bStale && (Impostor.IsValid() || bImpostorBuildFailed))
    {
        Impostor = ImpostorAtlas{};
        bImpostorBuildFailed = false;
        BuildImpostorAtlas();
    }
}

void InstancedMeshComponent::BuildImpostorAtlas()
{
    if (!bImpostorEnabled || bImpostorBuildFailed || Impostor.IsValid() || LodCount == 0 || GamePtr == nullptr)
    {
        return;
    }

    ID3D11Device* device = GamePtr->GetDevice();
    if (device == nullptr)
    {
        return;
    }

    // The most detailed level is the source of the sprite, so designers author only that model.
    // The material is compiled with its instanced set now: a later compile would trigger a rebake.
    Material* material = GamePtr->PrepareMaterial(this);
    BakedMaterial = material;
    BakedMaterialRevision = material ? material->GetRevision() : 0;
    if (!Impostor.Build(*GamePtr, Lods[0].Data, textureSRV.Get(), Color, ImpostorSettings, material))
    {
        std::cout << "InstancedMeshComponent: impostor atlas was not built, sprite level disabled." << std::endl;
        bImpostorBuildFailed = true;
        return;
    }

    const ImpostorAtlasSettings& settings = Impostor.GetSettings();
    const float minPitch = DirectX::XMConvertToRadians(settings.MinPitchDegrees);
    const float maxPitch = DirectX::XMConvertToRadians(settings.MaxPitchDegrees);
    const float pitchStep = settings.PitchCount > 1 ? (maxPitch - minPitch) / (settings.PitchCount - 1) : 1.0f;

    ImpostorConstants constants = {};
    constants.AtlasParams = DirectX::XMFLOAT4(static_cast<float>(settings.YawCount),
                                              static_cast<float>(settings.PitchCount),
                                              minPitch, pitchStep);
    // The shader derives the tile margin from the mip it samples, so it needs the tile size and
    // the coarsest mip rather than one fixed inset: a margin of half a texel of the top mip
    // leaks a neighbouring view once the sprite drops to a coarser one.
    constants.ModelParams = DirectX::XMFLOAT4(Impostor.GetBounds().Radius,
                                              Impostor.GetBounds().Center.y,
                                              static_cast<float>(settings.TileSize),
                                              Impostor.GetMaxMipLevel());

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(ImpostorConstants);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA initial = {};
    initial.pSysMem = &constants;
    CD3D11_RASTERIZER_DESC rasterDesc(D3D11_DEFAULT);
    rasterDesc.CullMode = D3D11_CULL_NONE;
    if (FAILED(device->CreateBuffer(&bufferDesc, &initial, ImpostorConstantBuffer.ReleaseAndGetAddressOf())) ||
        !TextureLoader::CreateLinearSampler(device, D3D11_TEXTURE_ADDRESS_CLAMP, ImpostorSampler) ||
        FAILED(device->CreateRasterizerState(&rasterDesc, ImpostorRasterState.ReleaseAndGetAddressOf())))
    {
        std::cout << "InstancedMeshComponent: failed to create impostor resources." << std::endl;
        bImpostorBuildFailed = true;
    }
}

float InstancedMeshComponent::ComputeScreenFactor() const
{
    if (GamePtr == nullptr || GamePtr->GetDisplay() == nullptr || GamePtr->GetPlayer() == nullptr)
    {
        return 0.0f;
    }
    const float windowHeight = static_cast<float>(GamePtr->GetDisplay()->GetHeight());
    const float tanHalfFov = std::tan(GamePtr->GetPlayer()->GetFovY() * 0.5f);
    return tanHalfFov > 0.0001f ? windowHeight / tanHalfFov : 0.0f;
}

void InstancedMeshComponent::DrawImpostors(ID3D11DeviceContext* context)
{
    const UINT instanceCount = static_cast<UINT>(ImpostorLevel.ColorStaging.size());
    if (!HasImpostor() || instanceCount == 0 || !ImpostorLevel.ColorSRV || !ImpostorConstantBuffer || GamePtr == nullptr)
    {
        return;
    }

    // The sprite level has its own material, resolved for the pass that is currently drawing.
    const std::string impostorShader = EnginePaths::Shader("Impostor.hlsl").generic_u8string();
    const ShaderCompileVariant variant = GamePtr->GetCurrentPassVariant();
    ID3D11VertexShader* impostorVS = GamePtr->GetVertexShader(impostorShader, variant);
    ID3D11PixelShader* impostorPS = GamePtr->GetPixelShader(impostorShader, variant);
    if (impostorVS == nullptr || impostorPS == nullptr)
    {
        return;
    }

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(impostorVS, nullptr, 0);
    context->PSSetShader(impostorPS, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
    context->VSSetConstantBuffers(1, 1, ImpostorConstantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(1, 1, ImpostorConstantBuffer.GetAddressOf());

    ID3D11ShaderResourceView* instanceSRV = ImpostorLevel.ColorSRV.Get();
    context->VSSetShaderResources(0, 1, &instanceSRV);
    ID3D11ShaderResourceView* atlas[2] = {Impostor.GetAlbedoSRV(), Impostor.GetNormalSRV()};
    context->PSSetShaderResources(0, 2, atlas);
    context->PSSetSamplers(0, 1, ImpostorSampler.GetAddressOf());
    // The pass may cull back faces for the meshes of a one-sided material; quads keep both sides.
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> passRasterState;
    context->RSGetState(passRasterState.GetAddressOf());
    context->RSSetState(ImpostorRasterState.Get());

    context->DrawInstanced(4, instanceCount, 0, 0);
    GamePtr->CountDraw(4, instanceCount);

    context->RSSetState(passRasterState.Get());
    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->PSSetShaderResources(0, 2, nullSRVs);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

bool InstancedMeshComponent::DumpImpostorAtlas(const std::string& pathPrefix)
{
    if (GamePtr == nullptr || !Impostor.IsValid())
    {
        return false;
    }
    return Impostor.Dump(*GamePtr, pathPrefix);
}
