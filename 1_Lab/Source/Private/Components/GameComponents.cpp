#include "../../../Source/Public/Components/GameComponents.h"

#include <algorithm>
#include <cstring>
#include <directxmath.h>
#include <iostream>
#include <iterator>
#include "../../Public/MainGame/BaseGameClass/Game.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "../../includes/GLM-master/glm/gtx/matrix_decompose.hpp"

namespace
{
    UINT strides[] = {32};
    UINT offsets[] = {0};
}

GameComponent::GameComponent()
{
    std::memset(&ConstantPositionBuffer, 0, sizeof(ConstantPositionBuffer));
    std::memset(&InstConstantPositionBuffer, 0, sizeof(InstConstantPositionBuffer));
}

GameComponent::GameComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
{
    std::memset(&ConstantPositionBuffer, 0, sizeof(ConstantPositionBuffer));
    std::memset(&InstConstantPositionBuffer, 0, sizeof(InstConstantPositionBuffer));
    ComponentPosition = pos;
    SetRotation(rot);
    ComponentScale = scale;
}

GameComponent::GameComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 Color)
{
    std::memset(&ConstantPositionBuffer, 0, sizeof(ConstantPositionBuffer));
    std::memset(&InstConstantPositionBuffer, 0, sizeof(InstConstantPositionBuffer));
    ComponentPosition = pos;
    SetRotation(rot);
    ComponentScale = scale;
    this->Color = Color;
    if (Color.w < 1.0f)
    {
        bHasOpacity = true;
    }
}

GameComponent* GameComponent::CreateInstance(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 Color)
{
    GameComponent* NewComponent = new GameComponent(pos, rot, scale, Color);
    InstanceArray.push_back(NewComponent);
    ID3D11Device* device = nullptr;
    GamePtr->GetContext()->GetDevice(&device);
    device->Release();
    CreateInstanceBuffer(device);
    return NewComponent;
}

void GameComponent::InitPoints(DirectX::XMFLOAT4* NewPoints, int pointCount, int* NewIndices, int indexCount)
{
    points.resize(pointCount);
    indices.resize(indexCount);
    memcpy(points.data(), NewPoints, sizeof(DirectX::XMFLOAT4) * pointCount);
    memcpy(indices.data(), NewIndices, sizeof(int) * indexCount);
    this->indexCount = indexCount;
}

void GameComponent::CreateBuffers(Microsoft::WRL::ComPtr<ID3D11Device> Device)
{
    if (points.empty() || indices.empty()) return;

    OutputDebugStringA("CreateBuffers start\n");

    if (points.empty() || indices.empty())
    {
        OutputDebugStringA("CreateBuffers: points or indices is empty!\n");
        return;
    }

    OutputDebugStringA(("points size: " + std::to_string(points.size()) + "\n").c_str());
    OutputDebugStringA(("indices size: " + std::to_string(indices.size()) + "\n").c_str());

    D3D11_BUFFER_DESC vertexBufDesc = {};
    vertexBufDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufDesc.ByteWidth = sizeof(DirectX::XMFLOAT4) * points.size();

    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = points.data();

    D3D11_BUFFER_DESC indexBufDesc = {};
    indexBufDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufDesc.ByteWidth = sizeof(int) * indices.size();

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices.data();

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(ConstantBufferData);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    
    D3D11_BUFFER_DESC instBufferDesc = {};
    instBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    instBufferDesc.ByteWidth = sizeof(InstConstantBufferData);
    instBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    instBufferDesc.CPUAccessFlags = 0;
    

    Device->CreateBuffer(&vertexBufDesc, &vertexData, &vb);
    Device->CreateBuffer(&indexBufDesc, &indexData, &ib);
    Device->CreateBuffer(&instBufferDesc, nullptr, &inst_Basecb);
    Device->CreateBuffer(&bufferDesc, nullptr, &cb);
}

void GameComponent::SetGame(Game* game)
{
    GamePtr = game;
    if (GamePtr)
    {
        ConstantPositionBuffer.LightDirection = GamePtr->GetDirectionalLightDirection();
        ConstantPositionBuffer.DirectionalLightColorIntensity = GamePtr->GetDirectionalLightColorIntensity();
        InstConstantPositionBuffer.LightDirection = ConstantPositionBuffer.LightDirection;
        InstConstantPositionBuffer.DirectionalLightColorIntensity = ConstantPositionBuffer.DirectionalLightColorIntensity;
    }
}

void GameComponent::Update()
{
    using namespace DirectX;

    XMFLOAT4X4 WorldMatrix = GetWorldMatrix();
    XMMATRIX worldMatrixXM = XMLoadFloat4x4(&WorldMatrix);
    XMMATRIX WorldMatrixTransposed = XMMatrixTranspose(worldMatrixXM);
    XMStoreFloat4x4(&ConstantPositionBuffer.worldMatrix, WorldMatrixTransposed);
    XMStoreFloat4x4(&ConstantPositionBuffer.invViewMatrix, XMMatrixIdentity());
    XMStoreFloat4x4(&ConstantPositionBuffer.invProjectionMatrix, XMMatrixIdentity());
    XMStoreFloat4x4(&InstConstantPositionBuffer.invViewMatrix, XMMatrixIdentity());
    XMStoreFloat4x4(&InstConstantPositionBuffer.invProjectionMatrix, XMMatrixIdentity());

    XMFLOAT4 ObjectColor;
    ObjectColor.x = Color.x;
    ObjectColor.y = Color.y;
    ObjectColor.z = Color.z;
    ObjectColor.w = Color.w;
    ConstantPositionBuffer.ObjectColor = ObjectColor;
    ConstantPositionBuffer.HasTexture = (textureSRV != nullptr) ? 1.0f : 0.0f;
    ConstantPositionBuffer.UVOffset.x = uvOffset.x;
    ConstantPositionBuffer.UVOffset.y = uvOffset.y;
    ConstantPositionBuffer.CameraPosition = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    ConstantPositionBuffer.LightMeta = XMFLOAT4(0.0f, 0.06f, 32.0f, 0.0f);
    ConstantPositionBuffer.ReflectionData = XMFLOAT4(
        cubeMapSRV ? 1.0f : 0.0f,
        ReflectionStrength,
        FresnelPower,
        bIsSkybox ? 1.0f : 0.0f
    );
    ConstantPositionBuffer.CascadeSplits = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    ConstantPositionBuffer.ShadowParams = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    ConstantPositionBuffer.LightDirection = XMFLOAT4(-0.35f, -0.85f, -0.2f, 0.0f);
    ConstantPositionBuffer.DirectionalLightColorIntensity = XMFLOAT4(0.95f, 0.93f, 0.90f, 0.80f);
    for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
    {
        ConstantPositionBuffer.LightViewProjection[cascadeIndex] = XMFLOAT4X4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }
    
    for (int i = 0; i < MaxPointLights; ++i)
    {
        ConstantPositionBuffer.LightPositions[i] = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        ConstantPositionBuffer.LightColors[i] = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        ConstantPositionBuffer.LightParams[i] = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
    }
    
    if (GamePtr)
    {
        const XMFLOAT4X4& view = GamePtr->GetViewMatrix();
        const XMFLOAT4X4& proj = GamePtr->GetProjectionMatrix();
        ConstantPositionBuffer.viewMatrix = view;
        ConstantPositionBuffer.projectionMatrix = proj;
        {
            const XMMATRIX viewStored = XMLoadFloat4x4(&view);
            const XMMATRIX projStored = XMLoadFloat4x4(&proj);
            const XMMATRIX viewOriginal = XMMatrixTranspose(viewStored);
            const XMMATRIX projOriginal = XMMatrixTranspose(projStored);
            const XMMATRIX invViewOriginal = XMMatrixInverse(nullptr, viewOriginal);
            const XMMATRIX invProjOriginal = XMMatrixInverse(nullptr, projOriginal);
            XMStoreFloat4x4(&ConstantPositionBuffer.invViewMatrix, XMMatrixTranspose(invViewOriginal));
            XMStoreFloat4x4(&ConstantPositionBuffer.invProjectionMatrix, XMMatrixTranspose(invProjOriginal));
        }
        
        if (!InstanceArray.empty())
        {
            InstConstantPositionBuffer.viewMatrix = view;
            InstConstantPositionBuffer.projectionMatrix = proj;
            InstConstantPositionBuffer.invViewMatrix = ConstantPositionBuffer.invViewMatrix;
            InstConstantPositionBuffer.invProjectionMatrix = ConstantPositionBuffer.invProjectionMatrix;
            InstConstantPositionBuffer.CameraPosition = ConstantPositionBuffer.CameraPosition;
            InstConstantPositionBuffer.LightMeta = ConstantPositionBuffer.LightMeta;
            InstConstantPositionBuffer.ReflectionData = ConstantPositionBuffer.ReflectionData;
            InstConstantPositionBuffer.CascadeSplits = ConstantPositionBuffer.CascadeSplits;
            InstConstantPositionBuffer.ShadowParams = ConstantPositionBuffer.ShadowParams;
            InstConstantPositionBuffer.LightDirection = ConstantPositionBuffer.LightDirection;
            InstConstantPositionBuffer.DirectionalLightColorIntensity = ConstantPositionBuffer.DirectionalLightColorIntensity;

            for (int i = 0; i < MaxPointLights; ++i)
            {
                InstConstantPositionBuffer.LightPositions[i] = ConstantPositionBuffer.LightPositions[i];
                InstConstantPositionBuffer.LightColors[i] = ConstantPositionBuffer.LightColors[i];
                InstConstantPositionBuffer.LightParams[i] = ConstantPositionBuffer.LightParams[i];
            }
            for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
            {
                InstConstantPositionBuffer.LightViewProjection[cascadeIndex] = ConstantPositionBuffer.LightViewProjection[cascadeIndex];
            }
        }
        
        const glm::vec3 cameraPosition = GamePtr->GetPlayer()->GetPosition();
        ConstantPositionBuffer.CameraPosition = XMFLOAT4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f);
        
        const std::vector<PointLightInfo> pointLights = GamePtr->GetPointLights(MaxPointLights);
        const int lightCount = static_cast<int>(std::min<size_t>(pointLights.size(), MaxPointLights));
        ConstantPositionBuffer.LightMeta.x = static_cast<float>(lightCount);
        
        for (int i = 0; i < lightCount; ++i)
        {
            const PointLightInfo& light = pointLights[i];
            ConstantPositionBuffer.LightPositions[i] = XMFLOAT4(light.Position.x, light.Position.y, light.Position.z, 1.0f);
            ConstantPositionBuffer.LightColors[i] = XMFLOAT4(light.Color.x, light.Color.y, light.Color.z, 1.0f);
            ConstantPositionBuffer.LightParams[i] = XMFLOAT4(light.Intensity, light.Radius, light.bEnabled ? 1.0f : 0.0f, 0.0f);
        }

        if (GamePtr->IsShadowEnabled())
        {
            ConstantPositionBuffer.CascadeSplits = GamePtr->GetCascadeSplits();
            ConstantPositionBuffer.ShadowParams = GamePtr->GetShadowParams();
            ConstantPositionBuffer.LightDirection = GamePtr->GetDirectionalLightDirection();
            ConstantPositionBuffer.DirectionalLightColorIntensity = GamePtr->GetDirectionalLightColorIntensity();
            const DirectX::XMFLOAT4X4* shadowMatrices = GamePtr->GetShadowMatrices();
            for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
            {
                ConstantPositionBuffer.LightViewProjection[cascadeIndex] = shadowMatrices[cascadeIndex];
            }
        }

        if (!InstanceArray.empty())
        {
            InstConstantPositionBuffer.CameraPosition = ConstantPositionBuffer.CameraPosition;
            InstConstantPositionBuffer.LightMeta = ConstantPositionBuffer.LightMeta;
            InstConstantPositionBuffer.ReflectionData = ConstantPositionBuffer.ReflectionData;
            InstConstantPositionBuffer.CascadeSplits = ConstantPositionBuffer.CascadeSplits;
            InstConstantPositionBuffer.ShadowParams = ConstantPositionBuffer.ShadowParams;
            InstConstantPositionBuffer.LightDirection = ConstantPositionBuffer.LightDirection;
            InstConstantPositionBuffer.DirectionalLightColorIntensity = ConstantPositionBuffer.DirectionalLightColorIntensity;

            for (int i = 0; i < MaxPointLights; ++i)
            {
                InstConstantPositionBuffer.LightPositions[i] = ConstantPositionBuffer.LightPositions[i];
                InstConstantPositionBuffer.LightColors[i] = ConstantPositionBuffer.LightColors[i];
                InstConstantPositionBuffer.LightParams[i] = ConstantPositionBuffer.LightParams[i];
            }
            for (int cascadeIndex = 0; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
            {
                InstConstantPositionBuffer.LightViewProjection[cascadeIndex] = ConstantPositionBuffer.LightViewProjection[cascadeIndex];
            }
        }
    }
    for (const auto& Inst : InstanceArray)
    {
        Inst->Update();
    }
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

DirectX::XMMATRIX RemoveScaleFromMatrix(DirectX::XMMATRIX matrix)
{
    DirectX::XMVECTOR scale, rotation, translation;
    XMMatrixDecompose(&scale, &rotation, &translation, matrix);
    
    return DirectX::XMMatrixAffineTransformation(
        DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f),
        DirectX::XMVectorZero(),
        rotation,
        translation
    );
}

DirectX::XMFLOAT4X4 GameComponent::GetWorldMatrix()
{
    using namespace DirectX;

    if (!bTransformDirty && Parent == nullptr)
    {
        return CachedWorldMatrix;
    }

    XMMATRIX worldMatrixXM = GetLocalMatrix();

    if (Parent != nullptr)
    {
        XMFLOAT4X4 ParentMatrix = Parent->GetWorldMatrix(); 
        XMMATRIX parentWorldMatrix = XMLoadFloat4x4(&ParentMatrix);
        if (!bApplyParentScale)
        {
            // Убираем масштаб из матрицы родителя
            parentWorldMatrix = RemoveScaleFromMatrix(parentWorldMatrix);
        }
        //XMMATRIX parentWorldMatrix = XMLoadFloat4x4(std::move(&ParentMatrix));
        worldMatrixXM = worldMatrixXM * parentWorldMatrix;
    }

    XMStoreFloat4x4(&CachedWorldMatrix, worldMatrixXM);
    bTransformDirty = false;

    return CachedWorldMatrix;
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
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* imageData = stbi_load(path.c_str(), &width, &height, &channels, 4);
    
    if (!imageData)
    {
        std::cout << "Failed to load texture: " << path << std::endl;
        return;
    }
    
    ID3D11Device* device = nullptr;
    if (GamePtr && GamePtr->GetContext())
    {
        GamePtr->GetContext()->GetDevice(&device);
    }
    
    if (!device) return;
    
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = imageData;
    initData.SysMemPitch = width * 4;
    
    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device->CreateTexture2D(&texDesc, &initData, &texture);
    
    if (SUCCEEDED(hr))
    {
        hr = device->CreateShaderResourceView(texture, nullptr, &textureSRV);
        texture->Release();
        
        if (SUCCEEDED(hr))
        {
            CreateSamplerState();
            std::cout << "Texture loaded: " << path << " (" << width << "x" << height << ")" << std::endl;
        }
    }
    
    stbi_image_free(imageData);
    device->Release();
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
    ID3D11Device* device = nullptr;
    if (GamePtr && GamePtr->GetContext())
    {
        GamePtr->GetContext()->GetDevice(&device);
    }
    
    if (!device) return;
    
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    
    device->CreateSamplerState(&sampDesc, &samplerState);
    device->Release();
}

InstData GameComponent::GetInstanceData()
{
    InstData Result;
    Result.World = ConstantPositionBuffer.worldMatrix;
    DirectX::XMFLOAT4 ObjectColor;
    ObjectColor.x = ConstantPositionBuffer.ObjectColor.x;
    ObjectColor.y = ConstantPositionBuffer.ObjectColor.y;
    ObjectColor.z = ConstantPositionBuffer.ObjectColor.z;
    ObjectColor.w = ConstantPositionBuffer.ObjectColor.w;
    Result.ObjectColor = ObjectColor;
    return Result;
}

void GameComponent::Render(ID3D11DeviceContext* Context)
{
    ID3D11Buffer* vbArray[] = {vb};
    Context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
    Context->IASetVertexBuffers(0, 1, vbArray, strides, offsets);

    Context->VSSetShader(vertexShader, nullptr, 0);
    Context->PSSetShader(pixelShader, nullptr, 0);

    if (textureSRV && samplerState)
    {
        Context->PSSetShaderResources(0, 1, &textureSRV);
        Context->PSSetSamplers(0, 1, &samplerState);
    }
    if (cubeMapSRV && cubeMapSamplerState)
    {
        Context->PSSetShaderResources(1, 1, &cubeMapSRV);
        Context->PSSetSamplers(1, 1, &cubeMapSamplerState);
    }
    if (GamePtr && GamePtr->IsShadowEnabled())
    {
        ID3D11ShaderResourceView* shadowMapSRV = GamePtr->GetShadowMapSRV();
        ID3D11SamplerState* shadowMapSampler = GamePtr->GetShadowSampler();
        if (shadowMapSRV && shadowMapSampler)
        {
            Context->PSSetShaderResources(4, 1, &shadowMapSRV);
            Context->PSSetSamplers(4, 1, &shadowMapSampler);
        }
    }
    
    if (!InstanceArray.empty())
    {
        Context->UpdateSubresource(inst_Basecb, 0, nullptr, &InstConstantPositionBuffer, 0, 0);
        Context->VSSetConstantBuffers(0, 1, &inst_Basecb);
        Context->PSSetConstantBuffers(0, 1, &inst_Basecb);
        
        if (ins_cb == nullptr || GetBufferElementCount() != InstanceArray.size()) { return; }
        UpdateInstanceBuffer(Context);
        Context->VSSetShaderResources(0, 1, &m_InstanceSRV);
        Context->DrawIndexedInstanced(indexCount, InstanceArray.size(), 0, 0, 0);

        ID3D11ShaderResourceView* nullSRV = nullptr;
        Context->VSSetShaderResources(0, 1, &nullSRV);
    }
    else
    {
        Context->UpdateSubresource(cb, 0, nullptr, &ConstantPositionBuffer, 0, 0);
        Context->VSSetConstantBuffers(0, 1, &cb);
        Context->PSSetConstantBuffers(0, 1, &cb);
        
        Context->DrawIndexed(GetIndexCount(), 0, 0);
    }

    ID3D11ShaderResourceView* nullSRV = nullptr;
    Context->PSSetShaderResources(0, 1, &nullSRV);
    Context->PSSetShaderResources(1, 1, &nullSRV);
    Context->PSSetShaderResources(4, 1, &nullSRV);
}

void GameComponent::RenderShadow(ID3D11DeviceContext* context,
                                 ID3D11VertexShader* shadowVertexShader,
                                 ID3D11Buffer* shadowCB,
                                 ID3D11InputLayout* shadowPrimitiveLayout,
                                 ID3D11InputLayout* shadowMeshLayout,
                                 const DirectX::XMFLOAT4X4& lightViewProjection)
{
    (void)shadowMeshLayout;
    if (context == nullptr || shadowVertexShader == nullptr || shadowCB == nullptr ||
        shadowPrimitiveLayout == nullptr || vb == nullptr || ib == nullptr)
    {
        return;
    }

    if (bHasOpacity || bIsSkybox)
    {
        return;
    }

    Update();
    ShadowPassBufferData shadowData = {};
    shadowData.worldMatrix = ConstantPositionBuffer.worldMatrix;
    shadowData.lightViewProjection = lightViewProjection;

    context->IASetInputLayout(shadowPrimitiveLayout);
    context->VSSetShader(shadowVertexShader, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    context->UpdateSubresource(shadowCB, 0, nullptr, &shadowData, 0, 0);
    context->VSSetConstantBuffers(0, 1, &shadowCB);

    ID3D11Buffer* vbArray[] = {vb};
    context->IASetVertexBuffers(0, 1, vbArray, strides, offsets);
    context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
    context->DrawIndexed(GetIndexCount(), 0, 0);
}

void GameComponent::RenderInstance(ID3D11DeviceContext* context)
{
    //ID3D11Buffer* vbArray[] = {vb};
    //context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
    //context->IASetVertexBuffers(0, 1, vbArray, strides, offsets);
    //context->VSSetShader(vertexShader, nullptr, 0);
    //context->PSSetShader(pixelShader, nullptr, 0);
    /*for (const auto& Instance : InstanceArray)
    {
        Instance->Tick(0.13f);
        Instance->Update();
        ConstantBufferData InstanceConstantPositionBuffer = Instance->ConstantPositionBuffer;
        context->UpdateSubresource(cb, 0, nullptr, &InstanceConstantPositionBuffer, 0, 0);
        context->VSSetConstantBuffers(0, 1, &cb);
        context->DrawIndexed(GetIndexCount(), 0, 0);
    }*/
}

void GameComponent::CreateInstanceBuffer(ID3D11Device* Device)
{
    if (ins_cb) ins_cb->Release();
    if (m_InstanceSRV) m_InstanceSRV->Release();

    if (InstanceArray.empty()) return;

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.ByteWidth = sizeof(InstData) * InstanceArray.size();
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(InstData);

    HRESULT hr = Device->CreateBuffer(&bufferDesc, nullptr, &ins_cb);
    if (FAILED(hr))
    {
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = InstanceArray.size();

    hr = Device->CreateShaderResourceView(ins_cb, &srvDesc, &m_InstanceSRV);
    if (FAILED(hr))
    {
        return;
    }
}

void GameComponent::UpdateInstanceBuffer(ID3D11DeviceContext* Context)
{
    if (!ins_cb || InstanceArray.empty()) return;

    std::vector<InstData> allInstData;
    allInstData.reserve(InstanceArray.size());

    for (const auto& Instance : InstanceArray)
    {
        //Instance->Tick(0.13f);
        //Instance->Update();
        InstData data = Instance->GetInstanceData();
        allInstData.push_back(data);
    }

    // Обновляем буфер
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = Context->Map(ins_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, allInstData.data(), sizeof(InstData) * allInstData.size());
        Context->Unmap(ins_cb, 0);
    }
}

