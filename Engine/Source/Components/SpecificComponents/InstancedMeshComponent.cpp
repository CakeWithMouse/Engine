#include "InstancedMeshInternal.h"
#include <Engine/Render/Material.h>

#include <Engine/Render/MeshSimplifier.h>
#include <Engine/Render/MeshValidation.h>

using namespace InstancedMeshDetail;

InstancedMeshComponent::InstancedMeshComponent()
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
    Color = glm::vec4(1.0f);
}

InstancedMeshComponent::InstancedMeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
    : GameComponent(pos, rot, scale)
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
    Color = glm::vec4(1.0f);
}

bool InstancedMeshComponent::SetLod(int level, MeshLodData data)
{
    if (level < 0 || level >= MaxLodCount || data.Vertices.empty() || data.Indices.empty())
    {
        return false;
    }
    if (level > LodCount)
    {
        std::cout << "InstancedMeshComponent: LOD levels must be filled in order, got " << level
                  << " while " << LodCount << " are set." << std::endl;
        return false;
    }

    Lods[level].Data = std::move(data);
    Lods[level].VertexBuffer.Reset();
    Lods[level].IndexBuffer.Reset();
    Lods[level].IndexCount = static_cast<UINT>(Lods[level].Data.Indices.size());
    LodCount = std::max(LodCount, level + 1);

    if (level == 0)
    {
        // Bounds and index count come from the most detailed level.
        float maxRadiusSq = 0.0f;
        for (const MeshVertex& vertex : Lods[0].Data.Vertices)
        {
            const float radiusSq = vertex.Position.x * vertex.Position.x +
                                   vertex.Position.y * vertex.Position.y +
                                   vertex.Position.z * vertex.Position.z;
            maxRadiusSq = std::max(maxRadiusSq, radiusSq);
        }
        MeshRadius = std::sqrt(maxRadiusSq);
        TotalIndexCount = static_cast<int>(Lods[0].Data.Indices.size());

        if (bValidateAsset)
        {
            const MeshValidationReport report = ValidateMesh(Lods[0].Data);
            std::cout << report.ToText(AssetName.empty() ? std::string("LOD0") : AssetName) << std::endl;
        }
    }

    if (GamePtr && GamePtr->GetDevice())
    {
        CreateLodBuffers(GamePtr->GetDevice(), Lods[level]);
    }
    return true;
}

void InstancedMeshComponent::SetLodDistances(const std::vector<float>& distances)
{
    LodDistances = distances;
    std::sort(LodDistances.begin(), LodDistances.end());
}

void InstancedMeshComponent::SetLodScreenHeights(const std::vector<float>& pixels)
{
    LodScreenHeights = pixels;
    // Screen-height thresholds run from closest (largest) to farthest (smallest).
    std::sort(LodScreenHeights.begin(), LodScreenHeights.end(), std::greater<float>());
}

size_t InstancedMeshComponent::AddInstance(const MeshInstance& instance)
{
    Instances.push_back(instance);
    PreparedFrame = UINT64_MAX;
    bStaticShadowDataDirty = true;
    return Instances.size() - 1;
}

uint32_t InstancedMeshComponent::GetDrawnInstancesForLod(int level) const
{
    // SelectLod can return the sprite level, so asking for it by number has to work too -
    // otherwise a caller counting the levels silently reports zero for the farthest one.
    if (level >= 0 && level == GetImpostorLevelIndex())
    {
        return GetDrawnImpostors();
    }
    if (level < 0 || level >= LodCount)
    {
        return 0;
    }
    return static_cast<uint32_t>(Lods[level].ColorStaging.size());
}

bool InstancedMeshComponent::CreateLodBuffers(ID3D11Device* device, LodLevel& lod)
{
    if (device == nullptr)
    {
        return false;
    }
    if (lod.VertexBuffer && lod.IndexBuffer)
    {
        return true;
    }
    if (bIsSkinned)
    {
        return CreateSkinnedLodBuffers(device, lod);
    }
    if (lod.Data.Vertices.empty() || lod.Data.Indices.empty())
    {
        return false;
    }

    lod.VertexStride = sizeof(MeshVertex);

    D3D11_BUFFER_DESC vertexDesc = {};
    vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDesc.ByteWidth = static_cast<UINT>(sizeof(MeshVertex) * lod.Data.Vertices.size());
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = lod.Data.Vertices.data();

    D3D11_BUFFER_DESC indexDesc = {};
    indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * lod.Data.Indices.size());
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = lod.Data.Indices.data();

    if (FAILED(device->CreateBuffer(&vertexDesc, &vertexData, lod.VertexBuffer.ReleaseAndGetAddressOf())) ||
        FAILED(device->CreateBuffer(&indexDesc, &indexData, lod.IndexBuffer.ReleaseAndGetAddressOf())))
    {
        std::cout << "InstancedMeshComponent: failed to create LOD buffers." << std::endl;
        lod.VertexBuffer.Reset();
        lod.IndexBuffer.Reset();
        return false;
    }
    return true;
}

void InstancedMeshComponent::CreateBuffers(ID3D11Device* Device)
{
    if (Device == nullptr)
    {
        return;
    }

    if (!cb)
    {
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(ConstantBufferData);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(Device->CreateBuffer(&bufferDesc, nullptr, cb.GetAddressOf())))
        {
            std::cout << "InstancedMeshComponent: failed to create constant buffer." << std::endl;
            return;
        }
    }

    BuildAutoLods();

    for (int level = 0; level < LodCount; ++level)
    {
        CreateLodBuffers(Device, Lods[level]);
    }

    BuildImpostorAtlas();

    if (AnimationTextureSRV)
    {
        UpdateAnimClipConstantBuffer(Device);
        EnsureSkinnedShaders(Device);
    }
}

void InstancedMeshComponent::SetAutoLod(const std::vector<float>& triangleRatios)
{
    AutoLodRatios = triangleRatios;
    bAutoLodBuilt = false;
}

void InstancedMeshComponent::SetAssetValidation(bool enabled, const std::string& modelName)
{
    bValidateAsset = enabled;
    AssetName = modelName;
}

void InstancedMeshComponent::BuildAutoLods()
{
    if (bAutoLodBuilt || AutoLodRatios.empty() || LodCount == 0 || Lods[0].Data.Indices.empty())
    {
        return;
    }
    bAutoLodBuilt = true;

    const std::string name = AssetName.empty() ? std::string("mesh") : AssetName;
    for (size_t step = 0; step < AutoLodRatios.size(); ++step)
    {
        const int level = static_cast<int>(step) + 1;
        if (level >= MaxLodCount)
        {
            break;
        }
        // A level the designer supplied is never replaced.
        if (level < LodCount && !Lods[level].Data.Indices.empty())
        {
            continue;
        }

        MeshSimplifySettings settings;
        settings.TargetRatio = AutoLodRatios[step];
        // The last generated level only has to keep the silhouette, so it may lose topology.
        settings.bAllowSloppy = step + 1 == AutoLodRatios.size();

        MeshSimplifyResult result;
        MeshLodData simplified = SimplifyMesh(Lods[0].Data, settings, &result);
        if (simplified.Indices.empty())
        {
            std::cout << "auto-lod " << name << " LOD" << level << ": could not be built from LOD0." << std::endl;
            break;
        }

        std::cout << "auto-lod " << name << " LOD" << level << ": " << result.SourceTriangles << " -> "
                  << result.ResultTriangles << " triangles (target " << result.TargetTriangles
                  << ", error " << result.Error << ", silhouette drift "
                  << result.SilhouetteDrift * 100.0f << "%"
                  << (result.bUsedSloppy ? ", sloppy" : "") << ")" << std::endl;
        if (result.SilhouetteDrift > 0.1f)
        {
            std::cout << "  warning: the generated level lost a part of the silhouette. A thin feature "
                         "(a gun, an antenna) does not survive simplification; supply this level by hand "
                         "if it has to stay." << std::endl;
        }

        SetLod(level, std::move(simplified));
    }
}

int InstancedMeshComponent::SelectLod(float distance, float screenHeightPixels) const
{
    int level = 0;
    if (!LodScreenHeights.empty())
    {
        // Screen size drives the switch, so the same setting works for every model and window.
        while (level < static_cast<int>(LodScreenHeights.size()) && screenHeightPixels < LodScreenHeights[level])
        {
            ++level;
        }
    }
    else
    {
        while (level < static_cast<int>(LodDistances.size()) && distance >= LodDistances[level])
        {
            ++level;
        }
    }
    // The sprite level is the last one, so the clamp must include it.
    return std::min(level, GetLevelCount() - 1);
}

void InstancedMeshComponent::BuildVisibleSets()
{
    for (int level = 0; level < LodCount; ++level)
    {
        Lods[level].ColorStaging.clear();
        for (int cascade = 0; cascade < MaxShadowCascades; ++cascade)
        {
            Lods[level].ShadowStaging[cascade].clear();
        }
    }
    ImpostorLevel.ColorStaging.clear();
    DrawnInstances = 0;
    CulledInstances = 0;

    const int levelCount = GetLevelCount();
    const int impostorLevelIndex = GetImpostorLevelIndex();

    if (LodCount == 0 || GamePtr == nullptr)
    {
        return;
    }

    const FrameConstants& frame = GamePtr->GetFrameConstants();
    const glm::vec3 cameraPosition(frame.CameraPosition.x, frame.CameraPosition.y, frame.CameraPosition.z);
    const float drawDistanceSq = DrawDistance > 0.0f ? DrawDistance * DrawDistance : 0.0f;
    const float shadowDistanceSq = ShadowDistance > 0.0f ? ShadowDistance * ShadowDistance : 0.0f;
    const bool bCastsShadow = CastsShadow() && GamePtr->AreShadowsActive();
    const float screenFactor = ComputeScreenFactor();
    const DirectX::XMFLOAT4* cascadeSpheres = GamePtr->GetCascadeSpheres();
    const int cascadeLimit = ShadowCascadeLimit > 0 ? std::min(ShadowCascadeLimit, MaxShadowCascades) : MaxShadowCascades;
    const DirectX::XMFLOAT4& lightDirectionValue = GamePtr->GetDirectionalLightDirection();
    glm::vec3 lightDirection(lightDirectionValue.x, lightDirectionValue.y, lightDirectionValue.z);
    const float lightLength = glm::length(lightDirection);
    lightDirection = lightLength > 0.0001f ? lightDirection / lightLength : glm::vec3(0.0f, -1.0f, 0.0f);
    const bool bAnimated = (AnimationTextureSRV != nullptr);

    const float shadowFadeStart = ShadowDistance * (1.0f - ShadowDistanceFade);
    const float shadowFadeStartSq = shadowFadeStart * shadowFadeStart;

    for (size_t instanceIndex = 0; instanceIndex < Instances.size(); ++instanceIndex)
    {
        const MeshInstance& instance = Instances[instanceIndex];
        if (!instance.bVisible)
        {
            continue;
        }

        const glm::vec3 toCamera = instance.Position - cameraPosition;
        const float distanceSq = glm::dot(toCamera, toCamera);
        if (drawDistanceSq > 0.0f && distanceSq > drawDistanceSq)
        {
            ++CulledInstances;
            continue;
        }

        const float radius = MeshRadius * instance.Scale;
        bool bInsideFrustum = true;
        for (const DirectX::XMFLOAT4& plane : frame.FrustumPlanes)
        {
            if (plane.x * instance.Position.x + plane.y * instance.Position.y +
                plane.z * instance.Position.z + plane.w < -radius)
            {
                bInsideFrustum = false;
                break;
            }
        }

        // Casters thin out towards the limit instead of stopping at it: a hard limit is a circle
        // around the camera, and on a dense field it shows up as a ring of unshadowed ground.
        bool bWantsShadow = bCastsShadow;
        if (bWantsShadow && shadowDistanceSq > 0.0f)
        {
            if (distanceSq > shadowDistanceSq)
            {
                bWantsShadow = false;
            }
            else if (distanceSq > shadowFadeStartSq && ShadowDistance > shadowFadeStart)
            {
                const float keep = (ShadowDistance - std::sqrt(distanceSq)) / (ShadowDistance - shadowFadeStart);
                bWantsShadow = StableUnitFraction(instanceIndex) < keep;
            }
        }
        if (!bInsideFrustum && !bWantsShadow)
        {
            ++CulledInstances;
            continue;
        }

        const float distance = std::sqrt(distanceSq);
        // Height of this instance on screen, in pixels of window height.
        const float screenHeight = distance > 0.001f ? radius * screenFactor / distance : screenFactor;

        int level = SelectLod(distance, screenHeight);
        int fadePartner = -1;
        float levelFade = 1.0f;
        float partnerFade = 0.0f;

        // Inside a transition band both levels are drawn and the dither in the shader keeps one
        // of them per pixel, so the switch is not a visible jump.
        if (LodFadeFraction > 0.0f && !LodScreenHeights.empty())
        {
            const int thresholdCount = std::min<int>(static_cast<int>(LodScreenHeights.size()), levelCount - 1);
            for (int index = 0; index < thresholdCount; ++index)
            {
                const float threshold = LodScreenHeights[index];
                const float upper = threshold * (1.0f + LodFadeFraction);
                const float lower = threshold * (1.0f - LodFadeFraction);
                if (screenHeight <= upper && screenHeight >= lower && upper > lower)
                {
                    const float coarseWeight = (upper - screenHeight) / (upper - lower);
                    level = index;
                    levelFade = 1.0f - coarseWeight;
                    fadePartner = std::min(index + 1, levelCount - 1);
                    partnerFade = coarseWeight;
                    break;
                }
            }
        }

        InstData data;
        data.World = MakeInstanceMatrix(instance.Position, instance.Yaw, instance.Scale);

        // The pair shares one dither pattern and splits it: the level that fades in takes the
        // pixels the other one drops, so together they always cover the whole unit. The sign of
        // the fade says which of the two this copy is, which keeps yzw free for the animation
        // clip and for the per-instance data of the material.
        auto pushColor = [&](int targetLevel, float fade, bool bFadingIn)
        {
            const glm::vec4 color = bLodDebugColors ? LodDebugColors[std::min(targetLevel, MaxLodCount - 1)] : instance.Color;
            data.ObjectColor = DirectX::XMFLOAT4(color.x, color.y, color.z, color.w);
            const float signedFade = bFadingIn ? -fade : fade;
            if (bAnimated)
            {
                data.Params = DirectX::XMFLOAT4(signedFade, static_cast<float>(instance.ClipId), instance.AnimationPhase, instance.Speed);
            }
            else
            {
                data.Params = DirectX::XMFLOAT4(signedFade, instance.CustomData.x, instance.CustomData.y, instance.CustomData.z);
            }
            LodLevel& bucket = targetLevel == impostorLevelIndex ? ImpostorLevel : Lods[targetLevel];
            bucket.ColorStaging.push_back(data);
        };

        if (bInsideFrustum)
        {
            pushColor(level, levelFade, false);
            if (fadePartner >= 0 && fadePartner != level)
            {
                pushColor(fadePartner, partnerFade, true);
            }
            ++DrawnInstances;
        }
        else
        {
            ++CulledInstances;
        }

        if (bWantsShadow)
        {
            const int meshLevel = std::min(level, LodCount - 1);
            CollectShadowCasters(instance, data, meshLevel, radius, lightDirection, cascadeSpheres, cascadeLimit);
        }
    }

    GamePtr->CountCulledInstances(CulledInstances);
}

bool InstancedMeshComponent::UploadStream(ID3D11DeviceContext* context,
                                          const std::vector<InstData>& staging,
                                          Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer,
                                          Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
                                          size_t& capacity)
{
    if (staging.empty())
    {
        return false;
    }

    if (!buffer || capacity < staging.size())
    {
        size_t newCapacity = std::max<size_t>(256, capacity);
        while (newCapacity < staging.size())
        {
            newCapacity *= 2;
        }

        Microsoft::WRL::ComPtr<ID3D11Device> device;
        context->GetDevice(device.GetAddressOf());

        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.ByteWidth = static_cast<UINT>(sizeof(InstData) * newCapacity);
        bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bufferDesc.StructureByteStride = sizeof(InstData);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.NumElements = static_cast<UINT>(newCapacity);

        srv.Reset();
        buffer.Reset();
        capacity = 0;
        if (FAILED(device->CreateBuffer(&bufferDesc, nullptr, buffer.GetAddressOf())) ||
            FAILED(device->CreateShaderResourceView(buffer.Get(), &srvDesc, srv.GetAddressOf())))
        {
            std::cout << "InstancedMeshComponent: failed to create instance buffer for "
                      << staging.size() << " instances." << std::endl;
            srv.Reset();
            buffer.Reset();
            return false;
        }
        capacity = newCapacity;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return false;
    }
    std::memcpy(mapped.pData, staging.data(), sizeof(InstData) * staging.size());
    context->Unmap(buffer.Get(), 0);
    return true;
}

void InstancedMeshComponent::EnsureFrameData(ID3D11DeviceContext* context)
{
    const uint64_t frameIndex = GamePtr ? GamePtr->GetFrameIndex() : 0;
    if (PreparedFrame == frameIndex)
    {
        return;
    }

    // Visibility, LOD selection and both instance streams are prepared once per frame and
    // reused by the colour pass and by every shadow cascade.
    BuildVisibleSets();
    for (int level = 0; level < LodCount; ++level)
    {
        LodLevel& lod = Lods[level];
        UploadStream(context, lod.ColorStaging, lod.ColorBuffer, lod.ColorSRV, lod.ColorCapacity);
        for (int cascade = 0; cascade < MaxShadowCascades; ++cascade)
        {
            UploadStream(context, lod.ShadowStaging[cascade], lod.ShadowBuffer[cascade],
                         lod.ShadowSRV[cascade], lod.ShadowCapacity[cascade]);
        }
    }
    UploadStream(context, ImpostorLevel.ColorStaging, ImpostorLevel.ColorBuffer,
                 ImpostorLevel.ColorSRV, ImpostorLevel.ColorCapacity);
    PreparedFrame = frameIndex;
}

uint32_t InstancedMeshComponent::GetMaterialVariant() const
{
    // Animated copies are skinned from the animation texture (the skinned levels need it).
    return MaterialVariant::Instanced | (AnimationTextureSRV ? MaterialVariant::Skinned : MaterialVariant::Mesh);
}

void InstancedMeshComponent::Render(ID3D11DeviceContext* context)
{
    if (context == nullptr || LodCount == 0 || !cb)
    {
        return;
    }

    EnsureFrameData(context);

    ID3D11Device* device = nullptr;
    context->GetDevice(&device);

    const bool bUseSkinned = (AnimationTextureSRV != nullptr) && EnsureSkinnedShaders(device);

    if (bUseSkinned)
    {
        // A material brings its skinned set (resolved by the pass); otherwise the crowd shaders.
        const bool bMaterial = GetMaterial() != nullptr;
        if (bMaterial && (vertexShader == nullptr || pixelShader == nullptr))
        {
            return;
        }
        context->IASetInputLayout(SkinnedInputLayout.Get());
        context->VSSetShader(bMaterial ? vertexShader : SkinnedVertexShader.Get(), nullptr, 0);
        context->PSSetShader(bMaterial ? pixelShader : SkinnedPixelShader.Get(), nullptr, 0);

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
        if (vertexShader == nullptr || pixelShader == nullptr)
        {
            return;
        }
        context->IASetInputLayout(GamePtr ? GamePtr->GetInputLayout(VertexFormat::Mesh) : nullptr);
        context->VSSetShader(vertexShader, nullptr, 0);
        context->PSSetShader(pixelShader, nullptr, 0);
    }

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    BindMaterialResources(context);
    context->UpdateSubresource(cb.Get(), 0, nullptr, &ConstantPositionBuffer, 0, 0);
    context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());

    const UINT offset = 0;
    for (int level = 0; level < LodCount; ++level)
    {
        LodLevel& lod = Lods[level];
        const UINT instanceCount = static_cast<UINT>(lod.ColorStaging.size());
        if (instanceCount == 0 || !lod.VertexBuffer || !lod.IndexBuffer || !lod.ColorSRV)
        {
            continue;
        }

        ID3D11Buffer* vertexBuffers[] = {lod.VertexBuffer.Get()};
        ID3D11ShaderResourceView* instanceSRV = lod.ColorSRV.Get();
        const UINT stride = lod.VertexStride;
        context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
        context->IASetIndexBuffer(lod.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->VSSetShaderResources(0, 1, &instanceSRV);
        context->DrawIndexedInstanced(lod.IndexCount, instanceCount, 0, 0, 0);
        if (GamePtr) GamePtr->CountDraw(lod.IndexCount, instanceCount);
    }

    DrawImpostors(context);

    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->VSSetShaderResources(0, 2, nullSRVs);
    UnbindMaterialResources(context);
}
