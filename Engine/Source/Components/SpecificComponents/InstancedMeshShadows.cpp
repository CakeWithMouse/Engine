#include "InstancedMeshInternal.h"
#include <Engine/Render/Material.h>
#include <Engine/Render/PointShadow.h>

using namespace InstancedMeshDetail;

bool InstancedMeshComponent::BindMaterialShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass)
{
    // The material depth pass handles both alpha clipping and the instance LOD dither.
    // Shadows are drawn before the colour passes, so a changed material is compiled here.
    Material* material = GamePtr ? GamePtr->PrepareMaterial(this) : nullptr;
    if (!material && GamePtr)
    {
        // Legacy instances have their own colour shaders. Use the existing opaque material
        // only for depth, so their LOD fade also reaches the shadow pixel shader.
        material = GamePtr->GetDefaultMaterial().get();
        if (material)
        {
            material->RequireVariant(GetMaterialVariant());
            if (material->NeedsCompile()) material->Compile(GamePtr->GetDevice(), context);
        }
    }
    if (material == nullptr || !material->HasShadowShaders(GetMaterialVariant()) || !cb)
    {
        return false;
    }
    // The full vertex layout: the shadow pass layout of meshes only feeds positions.
    context->IASetInputLayout(AnimationTextureSRV ? SkinnedInputLayout.Get() : GamePtr->GetInputLayout(VertexFormat::Mesh));
    context->VSSetShader(material->GetShadowVertexShader(GetMaterialVariant()), nullptr, 0);
    context->PSSetShader(material->GetShadowPixelShader(GetMaterialVariant()), nullptr, 0);
    context->UpdateSubresource(cb.Get(), 0, nullptr, &ConstantPositionBuffer, 0, 0);
    context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
    material->BindPassTransform(context, shadowPass.LightViewProjection);
    return true;
}

void InstancedMeshComponent::UnbindMaterialShadow(ID3D11DeviceContext* context)
{
    Material* material = GetMaterial();
    if (!material && GamePtr) material = GamePtr->GetDefaultMaterial().get();
    if (material) material->Unbind(context);
    context->PSSetShader(nullptr, nullptr, 0);
}

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
            : DirectX::XMFLOAT4(1.0f, instance.CustomData.x, instance.CustomData.y, instance.CustomData.z);
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
    const bool bUnknownAnimationBounds = AnimationTextureSRV != nullptr && AnimationBoundsRadius <= 0.0f;
    data.ObjectColor = DirectX::XMFLOAT4(instance.Color.x, instance.Color.y, instance.Color.z, instance.Color.w);
    if (AnimationTextureSRV != nullptr)
    {
        data.Params = DirectX::XMFLOAT4(1.0f, static_cast<float>(instance.ClipId), instance.AnimationPhase, instance.Speed);
    }
    else
    {
        data.Params = DirectX::XMFLOAT4(1.0f, instance.CustomData.x, instance.CustomData.y, instance.CustomData.z);
    }

    for (int cascade = 0; cascade < cascadeLimit; ++cascade)
    {
        const DirectX::XMFLOAT4& sphere = cascadeSpheres[cascade];
        if (sphere.w <= 0.0f)
        {
            continue;
        }

        if (!bUnknownAnimationBounds)
        {
            const glm::vec3 toInstance = instance.Position - glm::vec3(sphere.x, sphere.y, sphere.z);
            const float along = std::max(0.0f, std::min(glm::dot(toInstance, -lightDirection), ShadowCasterExtrusion));
            const glm::vec3 closest = -lightDirection * along;
            const glm::vec3 offset = toInstance - closest;
            const float reach = sphere.w + radius;
            if (glm::dot(offset, offset) > reach * reach)
            {
                continue;
            }
        }

        // The level follows the on-screen size of the unit, never the cascade. A cascade index says
        // nothing about distance once the splits follow the scene, and a caster coarser than the
        // drawn level encloses the visible hull, which then sits inside its own shadow: the near
        // vehicles went black on their sunlit sides.
        const int shadowLevel = std::min(meshLevel + ShadowLodBias, LodCount - 1);
        Lods[shadowLevel].ShadowStaging[cascade].push_back(data);

    }
}

void InstancedMeshComponent::CollectPointCasters(
    const MeshInstance& instance, InstData data, int meshLevel, float radius,
    const DirectX::XMFLOAT4* lightSpheres, int lightCount, float signedFade)
{
    const bool bUnknownAnimationBounds = AnimationTextureSRV != nullptr && AnimationBoundsRadius <= 0.0f;
    data.ObjectColor = DirectX::XMFLOAT4(instance.Color.x, instance.Color.y, instance.Color.z, instance.Color.w);
    data.Params = AnimationTextureSRV != nullptr
        ? DirectX::XMFLOAT4(signedFade, static_cast<float>(instance.ClipId), instance.AnimationPhase, instance.Speed)
        : DirectX::XMFLOAT4(signedFade, instance.CustomData.x, instance.CustomData.y, instance.CustomData.z);

    // The level that is drawn, without the shadow bias of the sun. A coarser caster is a box wider
    // than the hull, and a lamp close by and low down lights exactly the surfaces that end up
    // inside it: only the top of the turret stayed lit.
    LodLevel& lod = Lods[meshLevel];
    for (int light = 0; light < lightCount; ++light)
    {
        const DirectX::XMFLOAT4& sphere = lightSpheres[light];
        const glm::vec3 offset = instance.Position - glm::vec3(sphere.x, sphere.y, sphere.z);
        if (!bUnknownAnimationBounds)
        {
            const float reach = sphere.w + radius;
            if (glm::dot(offset, offset) > reach * reach)
            {
                continue;
            }
        }

        // Each copy goes only to the faces it can land on: six full streams would draw a dense
        // field six times over.
        const uint32_t faces = bUnknownAnimationBounds
            ? ((1u << PointShadowFaces) - 1u)
            : PointShadowFaceMask(offset.x, offset.y, offset.z, radius);
        for (int face = 0; face < PointShadowFaces; ++face)
        {
            if (faces & (1u << face))
            {
                lod.PointStaging[light * PointShadowFaces + face].push_back(data);
            }
        }
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
            context->VSSetConstantBuffers(3, 1, &clipCB);
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
    // A masked material replaces the depth-only state above with its own shaders.
    const bool bMaterialShadow = BindMaterialShadow(context, shadowPass);

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
    if (bMaterialShadow)
    {
        UnbindMaterialShadow(context);
    }
}

void InstancedMeshComponent::RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass)
{
    if (context == nullptr || LodCount == 0 || !CastsShadow() ||
        shadowPass.ConstantBuffer == nullptr)
    {
        return;
    }

    const bool bPointPass = shadowPass.PointFace >= 0;
    if (!bPointPass && shadowPass.CascadeIndex < 0)
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
            context->VSSetConstantBuffers(3, 1, &clipCB);
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
    // A masked material replaces the depth-only state above with its own shaders.
    const bool bMaterialShadow = BindMaterialShadow(context, shadowPass);

    const int cascade = std::max(0, std::min(shadowPass.CascadeIndex, MaxShadowCascades - 1));
    const int pointStream = shadowPass.PointLightIndex * PointShadowFaces + shadowPass.PointFace;
    const UINT offset = 0;
    for (int level = 0; level < LodCount; ++level)
    {
        LodLevel& lod = Lods[level];
        // Only the casters this cascade or cube face needs, at the detail chosen for it.
        const UINT instanceCount = static_cast<UINT>((bPointPass ? lod.PointStaging[pointStream] : lod.ShadowStaging[cascade]).size());
        ID3D11ShaderResourceView* instanceSRV = bPointPass ? lod.PointSRV[pointStream].Get() : lod.ShadowSRV[cascade].Get();
        if (instanceCount == 0 || !lod.VertexBuffer || !lod.IndexBuffer || instanceSRV == nullptr)
        {
            continue;
        }

        ID3D11Buffer* vertexBuffers[] = {lod.VertexBuffer.Get()};
        const UINT stride = lod.VertexStride;
        context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
        context->IASetIndexBuffer(lod.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->VSSetShaderResources(0, 1, &instanceSRV);
        context->DrawIndexedInstanced(lod.IndexCount, instanceCount, 0, 0, 0);
        if (GamePtr) GamePtr->CountDraw(lod.IndexCount, instanceCount);
    }

    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->VSSetShaderResources(0, 2, nullSRVs);
    if (bMaterialShadow)
    {
        UnbindMaterialShadow(context);
    }
}
