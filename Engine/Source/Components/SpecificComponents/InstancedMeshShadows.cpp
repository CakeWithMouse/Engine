#include "InstancedMeshInternal.h"

using namespace InstancedMeshDetail;

bool InstancedMeshComponent::EnsureStaticShadowData(ID3D11DeviceContext* context)
{
    if (!bStaticShadowDataDirty && StaticShadowSRV && StaticShadowStaging.size() == Instances.size())
    {
        return !StaticShadowStaging.empty();
    }

    // Baked casters are not culled by the camera and keep one detail level for the whole map.
    StaticShadowLevel = std::min(std::max(ShadowLodBias, 0), LodCount - 1);
    StaticShadowStaging.clear();
    StaticShadowStaging.reserve(Instances.size());
    for (const MeshInstance& instance : Instances)
    {
        if (!instance.bVisible)
        {
            continue;
        }
        InstData data;
        data.World = MakeInstanceMatrix(instance.Position, instance.Yaw, instance.Scale);
        data.ObjectColor = DirectX::XMFLOAT4(instance.Color.x, instance.Color.y, instance.Color.z, instance.Color.w);
        data.Params = (AnimationTextureSRV != nullptr)
            ? DirectX::XMFLOAT4(1.0f, static_cast<float>(instance.ClipId), instance.AnimationPhase, instance.Speed)
            : DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
        StaticShadowStaging.push_back(data);
    }

    const bool uploaded = UploadStream(context, StaticShadowStaging, StaticShadowBuffer,
                                       StaticShadowSRV, StaticShadowCapacity);
    bStaticShadowDataDirty = false;
    return uploaded;
}

void InstancedMeshComponent::CollectShadowCasters(
    const MeshInstance& instance, InstData data, int meshLevel, float radius,
    const glm::vec3& lightDirection, const DirectX::XMFLOAT4* cascadeSpheres, int cascadeLimit)
{
    data.ObjectColor = DirectX::XMFLOAT4(instance.Color.x, instance.Color.y, instance.Color.z, instance.Color.w);
    if (AnimationTextureSRV != nullptr)
    {
        data.Params = DirectX::XMFLOAT4(1.0f, static_cast<float>(instance.ClipId), instance.AnimationPhase, instance.Speed);
    }
    else
    {
        data.Params = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
    }

    for (int cascade = 0; cascade < cascadeLimit; ++cascade)
    {
        const DirectX::XMFLOAT4& sphere = cascadeSpheres[cascade];
        if (sphere.w <= 0.0f)
        {
            continue;
        }

        const glm::vec3 toInstance = instance.Position - glm::vec3(sphere.x, sphere.y, sphere.z);
        const float along = std::max(0.0f, std::min(glm::dot(toInstance, -lightDirection), ShadowCasterExtrusion));
        const glm::vec3 closest = -lightDirection * along;
        const glm::vec3 offset = toInstance - closest;
        const float reach = sphere.w + radius;
        if (glm::dot(offset, offset) > reach * reach)
        {
            continue;
        }

        const int shadowLevel = std::min(meshLevel + ShadowLodBias + cascade, LodCount - 1);
        Lods[shadowLevel].ShadowStaging[cascade].push_back(data);
    }
}

void InstancedMeshComponent::RenderStaticShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass)
{
    if (!EnsureStaticShadowData(context) || StaticShadowLevel >= LodCount)
    {
        return;
    }

    LodLevel& lod = Lods[StaticShadowLevel];
    if (!lod.VertexBuffer || !lod.IndexBuffer || !StaticShadowSRV)
    {
        return;
    }

    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    const bool bUseSkinned = (AnimationTextureSRV != nullptr) && EnsureSkinnedShaders(device);

    ShadowPassBufferData shadowData = {};
    DirectX::XMStoreFloat4x4(&shadowData.worldMatrix, DirectX::XMMatrixIdentity());
    shadowData.lightViewProjection = shadowPass.LightViewProjection;

    if (bUseSkinned)
    {
        context->IASetInputLayout(SkinnedShadowInputLayout.Get());
        context->VSSetShader(SkinnedShadowVertexShader.Get(), nullptr, 0);
        ID3D11ShaderResourceView* animSRV = AnimationTextureSRV.Get();
        context->VSSetShaderResources(1, 1, &animSRV);
        if (AnimClipConstantBuffer)
        {
            ID3D11Buffer* clipCB = AnimClipConstantBuffer.Get();
            context->VSSetConstantBuffers(1, 1, &clipCB);
        }
    }
    else
    {
        context->IASetInputLayout(shadowPass.MeshLayout);
        context->VSSetShader(shadowPass.InstancedVertexShader, nullptr, 0);
    }

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->PSSetShader(nullptr, nullptr, 0);
    context->UpdateSubresource(shadowPass.ConstantBuffer, 0, nullptr, &shadowData, 0, 0);
    context->VSSetConstantBuffers(0, 1, &shadowPass.ConstantBuffer);

    ID3D11Buffer* vertexBuffers[] = {lod.VertexBuffer.Get()};
    ID3D11ShaderResourceView* instanceSRV = StaticShadowSRV.Get();
    const UINT stride = lod.VertexStride;
    const UINT offset = 0;
    const UINT instanceCount = static_cast<UINT>(StaticShadowStaging.size());
    context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
    context->IASetIndexBuffer(lod.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShaderResources(0, 1, &instanceSRV);
    context->DrawIndexedInstanced(lod.IndexCount, instanceCount, 0, 0, 0);
    if (GamePtr) GamePtr->CountDraw(lod.IndexCount, instanceCount);

    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->VSSetShaderResources(0, 2, nullSRVs);
}

void InstancedMeshComponent::RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass)
{
    if (context == nullptr || LodCount == 0 || !CastsShadow() ||
        shadowPass.ConstantBuffer == nullptr)
    {
        return;
    }

    if (shadowPass.CascadeIndex < 0)
    {
        RenderStaticShadow(context, shadowPass);
        return;
    }

    EnsureFrameData(context);

    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    const bool bUseSkinned = (AnimationTextureSRV != nullptr) && EnsureSkinnedShaders(device);

    if (!bUseSkinned && (shadowPass.InstancedVertexShader == nullptr || shadowPass.MeshLayout == nullptr))
    {
        return;
    }

    ShadowPassBufferData shadowData = {};
    DirectX::XMStoreFloat4x4(&shadowData.worldMatrix, DirectX::XMMatrixIdentity());
    shadowData.lightViewProjection = shadowPass.LightViewProjection;

    if (bUseSkinned)
    {
        context->IASetInputLayout(SkinnedShadowInputLayout.Get());
        context->VSSetShader(SkinnedShadowVertexShader.Get(), nullptr, 0);
        ID3D11ShaderResourceView* animSRV = AnimationTextureSRV.Get();
        context->VSSetShaderResources(1, 1, &animSRV);
        if (AnimClipConstantBuffer)
        {
            ID3D11Buffer* clipCB = AnimClipConstantBuffer.Get();
            context->VSSetConstantBuffers(1, 1, &clipCB);
        }
    }
    else
    {
        context->IASetInputLayout(shadowPass.MeshLayout);
        context->VSSetShader(shadowPass.InstancedVertexShader, nullptr, 0);
    }

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->PSSetShader(nullptr, nullptr, 0);
    context->UpdateSubresource(shadowPass.ConstantBuffer, 0, nullptr, &shadowData, 0, 0);
    context->VSSetConstantBuffers(0, 1, &shadowPass.ConstantBuffer);

    const int cascade = std::max(0, std::min(shadowPass.CascadeIndex, MaxShadowCascades - 1));
    const UINT offset = 0;
    for (int level = 0; level < LodCount; ++level)
    {
        LodLevel& lod = Lods[level];
        // Only the casters this cascade needs, at the detail chosen for it.
        const UINT instanceCount = static_cast<UINT>(lod.ShadowStaging[cascade].size());
        if (instanceCount == 0 || !lod.VertexBuffer || !lod.IndexBuffer || !lod.ShadowSRV[cascade])
        {
            continue;
        }

        ID3D11Buffer* vertexBuffers[] = {lod.VertexBuffer.Get()};
        ID3D11ShaderResourceView* instanceSRV = lod.ShadowSRV[cascade].Get();
        const UINT stride = lod.VertexStride;
        context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
        context->IASetIndexBuffer(lod.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->VSSetShaderResources(0, 1, &instanceSRV);
        context->DrawIndexedInstanced(lod.IndexCount, instanceCount, 0, 0, 0);
        if (GamePtr) GamePtr->CountDraw(lod.IndexCount, instanceCount);
    }

    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->VSSetShaderResources(0, 2, nullSRVs);
}
