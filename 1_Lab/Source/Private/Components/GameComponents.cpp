#include "../../../Source/Public/Components/GameComponents.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <directxmath.h>
#include <iostream>
#include "../../Public/MainGame/BaseGameClass/Game.h"
#include "../../Public/Render/TextureLoader.h"
#include "../../includes/GLM-master/glm/gtx/matrix_decompose.hpp"

namespace
{
    constexpr UINT PrimitiveVertexStride = sizeof(DirectX::XMFLOAT4) * 2;

    DirectX::XMMATRIX RemoveScaleFromMatrix(DirectX::XMMATRIX matrix)
    {
        DirectX::XMVECTOR scale, rotation, translation;
        DirectX::XMMatrixDecompose(&scale, &rotation, &translation, matrix);

        return DirectX::XMMatrixAffineTransformation(
            DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f),
            DirectX::XMVectorZero(),
            rotation,
            translation
        );
    }
}

GameComponent::GameComponent() = default;

GameComponent::GameComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
{
    ComponentPosition = pos;
    SetRotation(rot);
    ComponentScale = scale;
}

GameComponent::GameComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 Color)
{
    ComponentPosition = pos;
    SetRotation(rot);
    ComponentScale = scale;
    this->Color = Color;
    if (Color.w < 1.0f)
    {
        bHasOpacity = true;
    }
}

GameComponent::~GameComponent() = default;

GameComponent* GameComponent::CreateInstance(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 Color)
{
    std::unique_ptr<GameComponent> NewComponent = std::make_unique<GameComponent>(pos, rot, scale, Color);
    // The instance is usable (Update/Tick) as soon as it is created.
    NewComponent->SetGame(GamePtr);
    GameComponent* Result = NewComponent.get();
    InstanceArray.push_back(std::move(NewComponent));
    return Result;
}

void GameComponent::InitPoints(DirectX::XMFLOAT4* NewPoints, int pointCount, int* NewIndices, int indexCount)
{
    points.assign(NewPoints, NewPoints + pointCount);
    indices.assign(NewIndices, NewIndices + indexCount);
    this->indexCount = indexCount;

    // Primitive vertices are (position, colour) pairs.
    float maxRadiusSq = 0.0f;
    for (size_t i = 0; i < points.size(); i += 2)
    {
        const DirectX::XMFLOAT4& p = points[i];
        maxRadiusSq = std::max(maxRadiusSq, p.x * p.x + p.y * p.y + p.z * p.z);
    }
    LocalBoundingRadius = std::sqrt(maxRadiusSq);
}

void GameComponent::CreateBuffers(ID3D11Device* Device)
{
    if (Device == nullptr || points.empty() || indices.empty())
    {
        return;
    }

    D3D11_BUFFER_DESC vertexBufDesc = {};
    vertexBufDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufDesc.ByteWidth = static_cast<UINT>(sizeof(DirectX::XMFLOAT4) * points.size());

    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = points.data();

    D3D11_BUFFER_DESC indexBufDesc = {};
    indexBufDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufDesc.ByteWidth = static_cast<UINT>(sizeof(int) * indices.size());

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices.data();

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(ConstantBufferData);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    D3D11_BUFFER_DESC instBufferDesc = {};
    instBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    instBufferDesc.ByteWidth = sizeof(InstConstantBufferData);
    instBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    if (FAILED(Device->CreateBuffer(&vertexBufDesc, &vertexData, vb.ReleaseAndGetAddressOf())) ||
        FAILED(Device->CreateBuffer(&indexBufDesc, &indexData, ib.ReleaseAndGetAddressOf())) ||
        FAILED(Device->CreateBuffer(&instBufferDesc, nullptr, inst_Basecb.ReleaseAndGetAddressOf())) ||
        FAILED(Device->CreateBuffer(&bufferDesc, nullptr, cb.ReleaseAndGetAddressOf())))
    {
        std::cout << "GameComponent: failed to create GPU buffers." << std::endl;
        vb.Reset();
        ib.Reset();
        inst_Basecb.Reset();
        cb.Reset();
    }
}

void GameComponent::SetShaderNames(const std::string& vertexShaderName, const std::string& pixelShaderName)
{
    VertexShaderName = vertexShaderName;
    PixelShaderName = pixelShaderName;
    ShaderVariants[0] = ComponentShaderVariant{};
    ShaderVariants[1] = ComponentShaderVariant{};
}

ComponentShaderVariant& GameComponent::GetShaderVariant(ShaderCompileVariant variant)
{
    return variant == ShaderCompileVariant::DeferredGBuffer ? ShaderVariants[1] : ShaderVariants[0];
}

void GameComponent::Update()
{
    UpdateObjectConstants();

    if (!InstanceArray.empty())
    {
        if (GamePtr)
        {
            ApplyFrameConstants(InstConstantPositionBuffer, GamePtr->GetFrameConstants());
        }
        InstConstantPositionBuffer.ReflectionData = ConstantPositionBuffer.ReflectionData;

        for (const auto& Inst : InstanceArray)
        {
            Inst->UpdateInstanceTransform();
        }
    }
}

void GameComponent::UpdateObjectConstants()
{
    using namespace DirectX;

    const XMFLOAT4X4 WorldMatrix = GetWorldMatrix();
    const XMMATRIX world = XMLoadFloat4x4(&WorldMatrix);
    XMStoreFloat4x4(&ConstantPositionBuffer.worldMatrix, XMMatrixTranspose(world));

    // Normals need the inverse-transpose; in the transposed HLSL layout that is simply the inverse.
    XMVECTOR determinant;
    const XMMATRIX inverseWorld = XMMatrixInverse(&determinant, world);
    if (std::abs(XMVectorGetX(determinant)) > 1e-12f)
    {
        XMStoreFloat4x4(&ConstantPositionBuffer.normalMatrix, inverseWorld);
    }
    else
    {
        XMStoreFloat4x4(&ConstantPositionBuffer.normalMatrix, XMMatrixTranspose(world));
    }

    ConstantPositionBuffer.ObjectColor = XMFLOAT4(Color.x, Color.y, Color.z, Color.w);
    ConstantPositionBuffer.HasTexture = textureSRV ? 1.0f : 0.0f;
    ConstantPositionBuffer.UVOffset = XMFLOAT2(uvOffset.x, uvOffset.y);
    ConstantPositionBuffer.ReflectionData = XMFLOAT4(
        cubeMapSRV ? 1.0f : 0.0f,
        ReflectionStrength,
        FresnelPower,
        bIsSkybox ? 1.0f : 0.0f
    );

    if (GamePtr)
    {
        ApplyFrameConstants(ConstantPositionBuffer, GamePtr->GetFrameConstants());
    }
}

void GameComponent::UpdateInstanceTransform()
{
    using namespace DirectX;

    const XMFLOAT4X4 WorldMatrix = GetWorldMatrix();
    XMStoreFloat4x4(&ConstantPositionBuffer.worldMatrix, XMMatrixTranspose(XMLoadFloat4x4(&WorldMatrix)));
    ConstantPositionBuffer.ObjectColor = XMFLOAT4(Color.x, Color.y, Color.z, Color.w);
}

void GameComponent::Tick(float deltaTime)
{
}

void GameComponent::SetRotation(glm::vec3 rot)
{
    ComponentRotation = rot;
    LocalRotationQuaternion = glm::quat(glm::radians(rot));
    bUseQuaternionRotation = false;
    MarkTransformDirty();
}

void GameComponent::SetRotationQuat(const glm::quat& rotQuat)
{
    LocalRotationQuaternion = glm::normalize(rotQuat);
    ComponentRotation = glm::degrees(glm::eulerAngles(LocalRotationQuaternion));
    bUseQuaternionRotation = true;
    MarkTransformDirty();
}

bool GameComponent::SetParent(GameComponent* parent)
{
    for (GameComponent* ancestor = parent; ancestor != nullptr; ancestor = ancestor->Parent)
    {
        if (ancestor == this)
        {
            std::cout << "SetParent rejected: the hierarchy would contain a cycle." << std::endl;
            return false;
        }
    }

    Parent = parent;
    bApplyParentScale = true;
    MarkTransformDirty();
    return true;
}

void GameComponent::ClearParentReferences(const GameComponent* removed)
{
    if (Parent == removed)
    {
        Parent = nullptr;
        MarkTransformDirty();
    }
    for (const auto& Instance : InstanceArray)
    {
        Instance->ClearParentReferences(removed);
    }
}

bool GameComponent::SetParentWithoutScale(GameComponent* parent)
{
    if (!SetParent(parent))
    {
        return false;
    }
    bApplyParentScale = false;
    return true;
}

float GameComponent::GetBoundingRadius() const
{
    float maxRadiusSq = 0.0f;

    for (size_t i = 0; i < points.size(); i += 2)
    {
        const DirectX::XMFLOAT4& position = points[i];
        const float scaledX = position.x * ComponentScale.x;
        const float scaledY = position.y * ComponentScale.y;
        const float scaledZ = position.z * ComponentScale.z;
        const float radiusSq = scaledX * scaledX + scaledY * scaledY + scaledZ * scaledZ;
        maxRadiusSq = std::max(maxRadiusSq, radiusSq);
    }

    return std::sqrt(maxRadiusSq);
}

float GameComponent::GetWorldBoundingRadius()
{
    const DirectX::XMFLOAT4X4 world = GetWorldMatrix();
    const float sx = std::sqrt(world._11 * world._11 + world._12 * world._12 + world._13 * world._13);
    const float sy = std::sqrt(world._21 * world._21 + world._22 * world._22 + world._23 * world._23);
    const float sz = std::sqrt(world._31 * world._31 + world._32 * world._32 + world._33 * world._33);
    return GetLocalBoundingRadius() * std::max(sx, std::max(sy, sz));
}

DirectX::XMFLOAT4X4 GameComponent::GetWorldMatrix()
{
    using namespace DirectX;

    // The parent is brought up to date first; its version tells whether our cache is still valid.
    XMFLOAT4X4 parentWorld{};
    uint64_t parentVersion = 0;
    if (Parent != nullptr)
    {
        parentWorld = Parent->GetWorldMatrix();
        parentVersion = Parent->GetWorldVersion();
    }

    if (!bTransformDirty && WorldVersion != 0 && CachedParent == Parent && CachedParentVersion == parentVersion)
    {
        return CachedWorldMatrix;
    }

    XMMATRIX worldMatrixXM = GetLocalMatrix();
    if (Parent != nullptr)
    {
        XMMATRIX parentWorldMatrix = XMLoadFloat4x4(&parentWorld);
        if (!bApplyParentScale)
        {
            parentWorldMatrix = RemoveScaleFromMatrix(parentWorldMatrix);
        }
        worldMatrixXM = worldMatrixXM * parentWorldMatrix;
    }

    XMStoreFloat4x4(&CachedWorldMatrix, worldMatrixXM);
    bTransformDirty = false;
    CachedParent = Parent;
    CachedParentVersion = parentVersion;
    ++WorldVersion;

    return CachedWorldMatrix;
}

glm::vec3 GameComponent::GetWorldPosition()
{
    const DirectX::XMFLOAT4X4 world = GetWorldMatrix();
    return glm::vec3(world._41, world._42, world._43);
}

glm::mat4 GameComponent::GetWorldMatrixGLM()
{
    DirectX::XMFLOAT4X4 dxMatrix = GetWorldMatrix();
    return glm::mat4(
        dxMatrix._11, dxMatrix._12, dxMatrix._13, dxMatrix._14,
        dxMatrix._21, dxMatrix._22, dxMatrix._23, dxMatrix._24,
        dxMatrix._31, dxMatrix._32, dxMatrix._33, dxMatrix._34,
        dxMatrix._41, dxMatrix._42, dxMatrix._43, dxMatrix._44
    );
}

glm::quat GameComponent::GetWorldRotationQuat()
{
    glm::mat4 worldMatrix = GetWorldMatrixGLM();
    glm::quat rotation;
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(worldMatrix, scale, rotation, translation, skew, perspective);
    return rotation;
}

DirectX::XMMATRIX GameComponent::GetLocalMatrix() const
{
    using namespace DirectX;

    XMMATRIX ScaleMatrix = XMMatrixScaling(
        ComponentScale.x,
        ComponentScale.y,
        ComponentScale.z
    );

    XMMATRIX RotationMatrix;
    if (bUseQuaternionRotation)
    {
        const XMVECTOR rotationQuaternion = XMVectorSet(
            LocalRotationQuaternion.x,
            LocalRotationQuaternion.y,
            LocalRotationQuaternion.z,
            LocalRotationQuaternion.w
        );
        RotationMatrix = XMMatrixRotationQuaternion(rotationQuaternion);
    }
    else
    {
        RotationMatrix = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(ComponentRotation.x),
            XMConvertToRadians(ComponentRotation.y),
            XMConvertToRadians(ComponentRotation.z)
        );
    }

    XMMATRIX TranslationMatrix = XMMatrixTranslation(
        ComponentPosition.x,
        ComponentPosition.y,
        ComponentPosition.z
    );

    // Порядок: Scale * Rotation * Translation
    return ScaleMatrix * RotationMatrix * TranslationMatrix;
}

void GameComponent::SetTexture(const std::string& path)
{
    ID3D11Device* device = GamePtr ? GamePtr->GetDevice() : nullptr;
    ID3D11DeviceContext* context = GamePtr ? GamePtr->GetContext() : nullptr;
    if (!TextureLoader::LoadTexture2D(device, context, path, true, textureSRV))
    {
        return;
    }

    texturePath = path;
    CreateSamplerState();
}

void GameComponent::SetCubeMap(CubeMapResource* cubeMap)
{
    if (!cubeMap)
    {
        cubeMapSRV = nullptr;
        cubeMapSamplerState = nullptr;
        return;
    }

    cubeMapSRV = cubeMap->ShaderResourceView.Get();
    cubeMapSamplerState = cubeMap->SamplerState.Get();
}

void GameComponent::SetReflectionSettings(float strength, float fresnelPower)
{
    ReflectionStrength = strength;
    FresnelPower = fresnelPower;
}

void GameComponent::CreateSamplerState()
{
    ID3D11Device* device = GamePtr ? GamePtr->GetDevice() : nullptr;
    TextureLoader::CreateLinearSampler(device, D3D11_TEXTURE_ADDRESS_CLAMP, samplerState);
}

InstData GameComponent::GetInstanceData() const
{
    InstData Result;
    Result.World = ConstantPositionBuffer.worldMatrix;
    Result.ObjectColor = ConstantPositionBuffer.ObjectColor;
    return Result;
}

bool GameComponent::UploadInstanceData(ID3D11DeviceContext* context)
{
    if (InstanceArray.empty() || context == nullptr)
    {
        return false;
    }

    // The stream is uploaded once per frame even if several passes draw it.
    const uint64_t frameIndex = GamePtr ? GamePtr->GetFrameIndex() : 0;
    const size_t count = InstanceArray.size();
    if (instanceSRV && InstanceUploadFrame == frameIndex && InstanceCapacity >= count)
    {
        return true;
    }

    if (!instanceBuffer || InstanceCapacity < count)
    {
        size_t newCapacity = std::max<size_t>(64, InstanceCapacity);
        while (newCapacity < count)
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
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>(newCapacity);

        instanceSRV.Reset();
        instanceBuffer.Reset();
        InstanceCapacity = 0;
        if (FAILED(device->CreateBuffer(&bufferDesc, nullptr, instanceBuffer.GetAddressOf())) ||
            FAILED(device->CreateShaderResourceView(instanceBuffer.Get(), &srvDesc, instanceSRV.GetAddressOf())))
        {
            std::cout << "GameComponent: failed to create instance buffer for " << count << " instances." << std::endl;
            instanceSRV.Reset();
            instanceBuffer.Reset();
            return false;
        }
        InstanceCapacity = newCapacity;
    }

    InstanceStaging.clear();
    InstanceStaging.reserve(count);
    for (const auto& Instance : InstanceArray)
    {
        InstanceStaging.push_back(Instance->GetInstanceData());
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (FAILED(context->Map(instanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
    {
        return false;
    }
    std::memcpy(mappedResource.pData, InstanceStaging.data(), sizeof(InstData) * InstanceStaging.size());
    context->Unmap(instanceBuffer.Get(), 0);

    InstanceUploadFrame = frameIndex;
    return true;
}

void GameComponent::BindMaterialResources(ID3D11DeviceContext* context)
{
    ID3D11ShaderResourceView* texture = textureSRV.Get();
    ID3D11SamplerState* sampler = samplerState.Get();
    context->PSSetShaderResources(0, 1, &texture);
    if (sampler)
    {
        context->PSSetSamplers(0, 1, &sampler);
    }

    context->PSSetShaderResources(1, 1, &cubeMapSRV);
    if (cubeMapSamplerState)
    {
        context->PSSetSamplers(1, 1, &cubeMapSamplerState);
    }

    if (GamePtr && GamePtr->AreShadowsActive())
    {
        ID3D11ShaderResourceView* shadowMapSRV = GamePtr->GetShadowMapSRV();
        ID3D11SamplerState* shadowMapSampler = GamePtr->GetShadowSampler();
        context->PSSetShaderResources(4, 1, &shadowMapSRV);
        context->PSSetSamplers(4, 1, &shadowMapSampler);
    }
}

void GameComponent::UnbindMaterialResources(ID3D11DeviceContext* context)
{
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->PSSetShaderResources(0, 1, &nullSRV);
    context->PSSetShaderResources(1, 1, &nullSRV);
    context->PSSetShaderResources(4, 1, &nullSRV);
}

void GameComponent::Render(ID3D11DeviceContext* Context)
{
    if (Context == nullptr || !vb || !ib || vertexShader == nullptr || pixelShader == nullptr || GetIndexCount() <= 0)
    {
        return;
    }

    // Every draw sets its own vertex format; nothing is inherited from the previous object.
    Context->IASetInputLayout(GamePtr ? GamePtr->GetInputLayout(VertexFormat::Primitive) : nullptr);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* vbArray[] = {vb.Get()};
    const UINT stride = PrimitiveVertexStride;
    const UINT offset = 0;
    Context->IASetVertexBuffers(0, 1, vbArray, &stride, &offset);
    Context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);

    Context->VSSetShader(vertexShader, nullptr, 0);
    Context->PSSetShader(pixelShader, nullptr, 0);
    BindMaterialResources(Context);

    if (!InstanceArray.empty())
    {
        if (UploadInstanceData(Context) && inst_Basecb)
        {
            Context->UpdateSubresource(inst_Basecb.Get(), 0, nullptr, &InstConstantPositionBuffer, 0, 0);
            Context->VSSetConstantBuffers(0, 1, inst_Basecb.GetAddressOf());
            Context->PSSetConstantBuffers(0, 1, inst_Basecb.GetAddressOf());

            ID3D11ShaderResourceView* srv = instanceSRV.Get();
            Context->VSSetShaderResources(0, 1, &srv);
            const UINT instanceCount = static_cast<UINT>(InstanceArray.size());
            Context->DrawIndexedInstanced(GetIndexCount(), instanceCount, 0, 0, 0);
            if (GamePtr) GamePtr->CountDraw(GetIndexCount(), instanceCount);

            ID3D11ShaderResourceView* nullSRV = nullptr;
            Context->VSSetShaderResources(0, 1, &nullSRV);
        }
    }
    else if (cb)
    {
        Context->UpdateSubresource(cb.Get(), 0, nullptr, &ConstantPositionBuffer, 0, 0);
        Context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
        Context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());

        Context->DrawIndexed(GetIndexCount(), 0, 0);
        if (GamePtr) GamePtr->CountDraw(GetIndexCount());
    }

    UnbindMaterialResources(Context);
}

void GameComponent::RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass)
{
    if (context == nullptr || !CastsShadow() || !vb || !ib || GetIndexCount() <= 0 ||
        shadowPass.ConstantBuffer == nullptr || shadowPass.PrimitiveLayout == nullptr)
    {
        return;
    }

    ShadowPassBufferData shadowData = {};
    shadowData.lightViewProjection = shadowPass.LightViewProjection;

    context->IASetInputLayout(shadowPass.PrimitiveLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* vbArray[] = {vb.Get()};
    const UINT stride = PrimitiveVertexStride;
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, vbArray, &stride, &offset);
    context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->PSSetShader(nullptr, nullptr, 0);

    if (!InstanceArray.empty())
    {
        // Same instance stream as the colour pass: every visible instance casts its own shadow.
        if (shadowPass.InstancedVertexShader == nullptr || !UploadInstanceData(context))
        {
            return;
        }

        DirectX::XMStoreFloat4x4(&shadowData.worldMatrix, DirectX::XMMatrixIdentity());
        context->VSSetShader(shadowPass.InstancedVertexShader, nullptr, 0);
        context->UpdateSubresource(shadowPass.ConstantBuffer, 0, nullptr, &shadowData, 0, 0);
        context->VSSetConstantBuffers(0, 1, &shadowPass.ConstantBuffer);

        ID3D11ShaderResourceView* srv = instanceSRV.Get();
        context->VSSetShaderResources(0, 1, &srv);
        const UINT instanceCount = static_cast<UINT>(InstanceArray.size());
        context->DrawIndexedInstanced(GetIndexCount(), instanceCount, 0, 0, 0);
        if (GamePtr) GamePtr->CountDraw(GetIndexCount(), instanceCount);

        ID3D11ShaderResourceView* nullSRV = nullptr;
        context->VSSetShaderResources(0, 1, &nullSRV);
        return;
    }

    if (shadowPass.VertexShader == nullptr)
    {
        return;
    }

    // World matrix was computed in this frame's update phase; the shadow pass does not update the object.
    shadowData.worldMatrix = ConstantPositionBuffer.worldMatrix;
    context->VSSetShader(shadowPass.VertexShader, nullptr, 0);
    context->UpdateSubresource(shadowPass.ConstantBuffer, 0, nullptr, &shadowData, 0, 0);
    context->VSSetConstantBuffers(0, 1, &shadowPass.ConstantBuffer);
    context->DrawIndexed(GetIndexCount(), 0, 0);
    if (GamePtr) GamePtr->CountDraw(GetIndexCount());
}
