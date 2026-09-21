#include "../../../../Source/Public/Components/SpecificComponents/ParticleSystemComponent.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include "../../../Public/Render/ShaderCompiler.h"

namespace
{
    const std::wstring ParticleShaderPath = L"Source/Shaders/GPUParticleSystem.hlsl";

    unsigned int NextPowerOfTwo(unsigned int value)
    {
        unsigned int result = 1u;
        while (result < value)
        {
            result <<= 1u;
        }

        return result;
    }
}

ParticleSystemComponent::ParticleSystemComponent()
{
    bHasOpacity = true;
    Color = glm::vec4(1.0f, 0.7f, 0.3f, 0.85f);
}

ParticleSystemComponent::ParticleSystemComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color)
    : GameComponent(pos, rot, scale, color)
{
    bHasOpacity = true;
}

ParticleSystemComponent::~ParticleSystemComponent() = default;

void ParticleSystemComponent::SetParticleCount(unsigned int newCount)
{
    const unsigned int clampedCount = std::max(64u, std::min(newCount, 200000u));
    if (clampedCount == ParticleCount)
    {
        return;
    }

    ParticleCount = clampedCount;
    if (GamePtr != nullptr && GamePtr->GetDevice() != nullptr)
    {
        CreateParticleBuffers(GamePtr->GetDevice());
    }
}

void ParticleSystemComponent::SetRandomness(float speedRandomness, float sizeRandomness)
{
    SpeedRandomness = std::max(0.0f, speedRandomness);
    SizeRandomness = std::max(0.0f, sizeRandomness);
}

void ParticleSystemComponent::CreateBuffers(ID3D11Device* device)
{
    if (!device)
    {
        return;
    }

    if (!CompileShaders(device))
    {
        std::cout << "ParticleSystemComponent: failed to compile shaders." << std::endl;
        return;
    }

    if (!CreateParticleBuffers(device))
    {
        std::cout << "ParticleSystemComponent: failed to create particle buffers." << std::endl;
        return;
    }

    auto createConstantBuffer = [device](UINT byteWidth, Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer)
    {
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = byteWidth;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        return SUCCEEDED(device->CreateBuffer(&desc, nullptr, buffer.ReleaseAndGetAddressOf()));
    };

    if (!createConstantBuffer(sizeof(GPUParticleSimulationCB), SimulationCB) ||
        !createConstantBuffer(sizeof(GPUParticleRenderCB), RenderCB) ||
        !createConstantBuffer(sizeof(GPUParticleSortCB), SortCB))
    {
        std::cout << "ParticleSystemComponent: failed to create constant buffers." << std::endl;
    }
}

void ParticleSystemComponent::Tick(float deltaTime)
{
    GameComponent::Tick(deltaTime);
    LastDeltaTime = std::max(0.0f, std::min(deltaTime * SimulationRate, MaxSimulationStep));
    SimulationTime += LastDeltaTime;
}

bool ParticleSystemComponent::IsReady() const
{
    return ComputeShader && ParticleVertexShader && ParticlePixelShader &&
           BuildSortKeysShader && BitonicSortShader &&
           SimulationCB && RenderCB && SortCB &&
           ParticleSRV[0] && ParticleSRV[1] && ParticleUAV[0] && ParticleUAV[1] &&
           ParticleSortSRV && ParticleSortUAV;
}

void ParticleSystemComponent::DispatchCompute(ID3D11DeviceContext* context)
{
    if (!context || !IsReady())
    {
        return;
    }

    DispatchSimulation(context);
    if (bSortingEnabled)
    {
        DispatchSort(context);
    }
}

void ParticleSystemComponent::Render(ID3D11DeviceContext* context)
{
    if (!context || !IsReady())
    {
        return;
    }

    UpdateRenderConstants(context);

    context->VSSetShader(ParticleVertexShader.Get(), nullptr, 0);
    context->PSSetShader(ParticlePixelShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, RenderCB.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, RenderCB.GetAddressOf());
    ID3D11ShaderResourceView* vertexSRVs[2] = {ParticleSRV[ReadBufferIndex].Get(), ParticleSortSRV.Get()};
    context->VSSetShaderResources(0, 2, vertexSRVs);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->DrawInstanced(4, ParticleCount, 0, 0);
    if (GamePtr) GamePtr->CountDraw(4, ParticleCount);

    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->VSSetShaderResources(0, 2, nullSRVs);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

bool ParticleSystemComponent::CompileShaders(ID3D11Device* device)
{
    if (!device)
    {
        return false;
    }

    auto compileEntry = [](const char* entryPoint, const char* target, Microsoft::WRL::ComPtr<ID3DBlob>& blob) -> bool
    {
        std::string errors;
        const HRESULT hr = ShaderCompiler::CompileFromFile(ParticleShaderPath, nullptr, entryPoint, target,
                                                           blob.ReleaseAndGetAddressOf(), &errors);
        if (FAILED(hr))
        {
            std::cout << "Particle shader " << entryPoint << ": " << errors << std::endl;
            return false;
        }
        return true;
    };

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> csBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> buildSortBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> bitonicSortBlob;
    if (!compileEntry("VSMain", "vs_5_0", vsBlob) ||
        !compileEntry("PSMain", "ps_5_0", psBlob) ||
        !compileEntry("CSMain", "cs_5_0", csBlob) ||
        !compileEntry("CSBuildSortKeys", "cs_5_0", buildSortBlob) ||
        !compileEntry("CSBitonicSort", "cs_5_0", bitonicSortBlob))
    {
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11VertexShader> newVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> newPixelShader;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> newComputeShader;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> newBuildSortKeysShader;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> newBitonicSortShader;

    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, newVertexShader.GetAddressOf())) ||
        FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, newPixelShader.GetAddressOf())) ||
        FAILED(device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, newComputeShader.GetAddressOf())) ||
        FAILED(device->CreateComputeShader(buildSortBlob->GetBufferPointer(), buildSortBlob->GetBufferSize(), nullptr, newBuildSortKeysShader.GetAddressOf())) ||
        FAILED(device->CreateComputeShader(bitonicSortBlob->GetBufferPointer(), bitonicSortBlob->GetBufferSize(), nullptr, newBitonicSortShader.GetAddressOf())))
    {
        return false;
    }

    // Published together only after every stage succeeded.
    ParticleVertexShader = newVertexShader;
    ParticlePixelShader = newPixelShader;
    ComputeShader = newComputeShader;
    BuildSortKeysShader = newBuildSortKeysShader;
    BitonicSortShader = newBitonicSortShader;
    return true;
}

bool ParticleSystemComponent::CreateParticleBuffers(ID3D11Device* device)
{
    if (!device)
    {
        return false;
    }

    for (int i = 0; i < 2; ++i)
    {
        ParticleUAV[i].Reset();
        ParticleSRV[i].Reset();
        ParticleBuffers[i].Reset();
    }
    ParticleSortUAV.Reset();
    ParticleSortSRV.Reset();
    ParticleSortBuffer.Reset();
    SortElementCount = NextPowerOfTwo(ParticleCount);

    std::vector<GPUParticleData> initialParticles(ParticleCount);

    std::mt19937 rng(static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    std::uniform_real_distribution<float> rand01(0.0f, 1.0f);
    std::uniform_real_distribution<float> randSigned(-1.0f, 1.0f);

    for (GPUParticleData& p : initialParticles)
    {
        p.PositionLife = DirectX::XMFLOAT4(0.0f, -100000.0f, 0.0f, 0.0f);
        p.VelocityLifetime = DirectX::XMFLOAT4(randSigned(rng), 1.0f + rand01(rng), randSigned(rng), BaseLifetime);
        p.ColorSize = DirectX::XMFLOAT4(Color.x, Color.y, Color.z, BaseSize);
    }

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(GPUParticleData) * initialParticles.size());
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(GPUParticleData);

    for (int i = 0; i < 2; ++i)
    {
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = initialParticles.data();

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.NumElements = ParticleCount;

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.NumElements = ParticleCount;

        if (FAILED(device->CreateBuffer(&bufferDesc, &initData, ParticleBuffers[i].GetAddressOf())) ||
            FAILED(device->CreateShaderResourceView(ParticleBuffers[i].Get(), &srvDesc, ParticleSRV[i].GetAddressOf())) ||
            FAILED(device->CreateUnorderedAccessView(ParticleBuffers[i].Get(), &uavDesc, ParticleUAV[i].GetAddressOf())))
        {
            return false;
        }
    }

    D3D11_BUFFER_DESC sortBufferDesc = {};
    sortBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    sortBufferDesc.ByteWidth = static_cast<UINT>(sizeof(GPUParticleSortPair) * SortElementCount);
    sortBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    sortBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    sortBufferDesc.StructureByteStride = sizeof(GPUParticleSortPair);

    D3D11_SHADER_RESOURCE_VIEW_DESC sortSRVDesc = {};
    sortSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    sortSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sortSRVDesc.Buffer.NumElements = SortElementCount;

    D3D11_UNORDERED_ACCESS_VIEW_DESC sortUAVDesc = {};
    sortUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    sortUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    sortUAVDesc.Buffer.NumElements = SortElementCount;

    if (FAILED(device->CreateBuffer(&sortBufferDesc, nullptr, ParticleSortBuffer.GetAddressOf())) ||
        FAILED(device->CreateShaderResourceView(ParticleSortBuffer.Get(), &sortSRVDesc, ParticleSortSRV.GetAddressOf())) ||
        FAILED(device->CreateUnorderedAccessView(ParticleSortBuffer.Get(), &sortUAVDesc, ParticleSortUAV.GetAddressOf())))
    {
        return false;
    }

    ReadBufferIndex = 0u;
    return true;
}

void ParticleSystemComponent::DispatchSimulation(ID3D11DeviceContext* context)
{
    using namespace DirectX;

    GPUParticleSimulationCB simulationData = {};
    simulationData.DeltaTime = LastDeltaTime;
    simulationData.TotalTime = SimulationTime;
    simulationData.ParticleCount = ParticleCount;
    simulationData.BaseLifetime = BaseLifetime;
    simulationData.EmitterPosition = GetEmitterWorldPosition();
    simulationData.SpreadRadius = SpreadRadius * std::max(0.001f, ComponentScale.x);
    simulationData.BaseSpeed = BaseSpeed * std::max(0.001f, ComponentScale.x);
    simulationData.SpeedRandomness = SpeedRandomness;
    simulationData.MinLifeFraction = MinLifeFraction;
    simulationData.MaxDistance = MaxDistance * std::max(0.001f, ComponentScale.x);
    simulationData.BaseVelocity = XMFLOAT3(BaseVelocity.x, BaseVelocity.y, BaseVelocity.z);
    simulationData.Acceleration = XMFLOAT3(Acceleration.x, Acceleration.y, Acceleration.z);
    simulationData.BaseColor = XMFLOAT4(Color.x, Color.y, Color.z, Color.w);
    simulationData.BaseSize = BaseSize * std::max(0.001f, ComponentScale.x);
    simulationData.SizeRandomness = SizeRandomness;
    if (GamePtr)
    {
        // Camera data of the current frame snapshot (inverses are computed once per frame).
        const FrameConstants& frame = GamePtr->GetFrameConstants();
        simulationData.SimulationViewMatrix = frame.viewMatrix;
        simulationData.SimulationProjectionMatrix = frame.projectionMatrix;
        simulationData.SimulationInvViewMatrix = frame.invViewMatrix;
        simulationData.SimulationInvProjectionMatrix = frame.invProjectionMatrix;
        simulationData.SimulationCameraPosition = frame.CameraPosition;
    }

    ID3D11ShaderResourceView* depthSRV = GamePtr ? GamePtr->GetDepthStencilSRV() : nullptr;
    const bool useDepthCollision = bDepthCollisionEnabled && depthSRV != nullptr;
    simulationData.DepthCollisionParams = XMFLOAT4(
        useDepthCollision ? 1.0f : 0.0f,
        DepthCollisionBias,
        DepthCollisionBounce,
        DepthCollisionFriction
    );

    ID3D11RenderTargetView* previousRTV = nullptr;
    ID3D11DepthStencilView* previousDSV = nullptr;
    if (useDepthCollision)
    {
        // The depth buffer is read as an SRV, so it must not stay bound as the depth target.
        context->OMGetRenderTargets(1, &previousRTV, &previousDSV);
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    context->UpdateSubresource(SimulationCB.Get(), 0, nullptr, &simulationData, 0, 0);
    context->CSSetShader(ComputeShader.Get(), nullptr, 0);
    context->CSSetConstantBuffers(0, 1, SimulationCB.GetAddressOf());
    ID3D11ShaderResourceView* particleSRV = ParticleSRV[ReadBufferIndex].Get();
    context->CSSetShaderResources(0, 1, &particleSRV);
    context->CSSetShaderResources(2, 1, &depthSRV);
    ID3D11UnorderedAccessView* particleUAV = ParticleUAV[1u - ReadBufferIndex].Get();
    context->CSSetUnorderedAccessViews(0, 1, &particleUAV, nullptr);

    const unsigned int dispatchCount = (ParticleCount + ThreadGroupSize - 1u) / ThreadGroupSize;
    context->Dispatch(dispatchCount, 1, 1);
    if (GamePtr) GamePtr->CountDispatch();

    ID3D11ShaderResourceView* nullSRVs[3] = {nullptr, nullptr, nullptr};
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    ID3D11Buffer* nullCB = nullptr;
    context->CSSetShaderResources(0, 3, nullSRVs);
    context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    context->CSSetConstantBuffers(0, 1, &nullCB);
    context->CSSetShader(nullptr, nullptr, 0);

    if (useDepthCollision)
    {
        context->OMSetRenderTargets(1, &previousRTV, previousDSV);
        if (previousRTV)
        {
            previousRTV->Release();
        }
        if (previousDSV)
        {
            previousDSV->Release();
        }
    }

    ReadBufferIndex = 1u - ReadBufferIndex;
}

void ParticleSystemComponent::DispatchSort(ID3D11DeviceContext* context)
{
    if (SortElementCount == 0u)
    {
        return;
    }

    GPUParticleSortCB sortData = {};
    if (GamePtr)
    {
        sortData.SortViewMatrix = GamePtr->GetFrameConstants().viewMatrix;
    }
    sortData.SortParticleCount = ParticleCount;
    sortData.SortElementCount = SortElementCount;

    context->UpdateSubresource(SortCB.Get(), 0, nullptr, &sortData, 0, 0);
    context->CSSetShader(BuildSortKeysShader.Get(), nullptr, 0);
    context->CSSetConstantBuffers(1, 1, SortCB.GetAddressOf());
    ID3D11ShaderResourceView* particleSRV = ParticleSRV[ReadBufferIndex].Get();
    context->CSSetShaderResources(0, 1, &particleSRV);
    ID3D11UnorderedAccessView* sortUAV = ParticleSortUAV.Get();
    context->CSSetUnorderedAccessViews(1, 1, &sortUAV, nullptr);

    const unsigned int sortDispatchCount = (SortElementCount + ThreadGroupSize - 1u) / ThreadGroupSize;
    context->Dispatch(sortDispatchCount, 1, 1);
    if (GamePtr) GamePtr->CountDispatch();

    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->CSSetShaderResources(0, 1, &nullSRV);

    // Full bitonic network: k(k+1)/2 dispatches for 2^k elements.
    context->CSSetShader(BitonicSortShader.Get(), nullptr, 0);
    for (unsigned int level = 2u; level <= SortElementCount; level <<= 1u)
    {
        sortData.BitonicLevel = level;
        for (unsigned int levelMask = level >> 1u; levelMask > 0u; levelMask >>= 1u)
        {
            sortData.BitonicLevelMask = levelMask;
            context->UpdateSubresource(SortCB.Get(), 0, nullptr, &sortData, 0, 0);
            context->Dispatch(sortDispatchCount, 1, 1);
            if (GamePtr) GamePtr->CountDispatch();
        }
    }

    ID3D11UnorderedAccessView* nullUAVs[2] = {nullptr, nullptr};
    ID3D11Buffer* nullCBs[2] = {nullptr, nullptr};
    context->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
    context->CSSetConstantBuffers(0, 2, nullCBs);
    context->CSSetShader(nullptr, nullptr, 0);
}

void ParticleSystemComponent::UpdateRenderConstants(ID3D11DeviceContext* context)
{
    if (!GamePtr || !GamePtr->GetPlayer())
    {
        return;
    }

    GPUParticleRenderCB renderData = {};
    renderData.ViewMatrix = GamePtr->GetFrameConstants().viewMatrix;
    renderData.ProjectionMatrix = GamePtr->GetFrameConstants().projectionMatrix;

    const Player* player = GamePtr->GetPlayer();
    glm::vec3 camRight = player->GetRight();
    glm::vec3 camUp = player->GetUp();
    if (player->GetCameraMode() == CameraMode::Orbital)
    {
        camRight = player->GetOrbitRight();
        camUp = player->GetOrbitUp();
    }

    renderData.CameraRight = DirectX::XMFLOAT4(camRight.x, camRight.y, camRight.z, 0.0f);
    renderData.CameraUp = DirectX::XMFLOAT4(camUp.x, camUp.y, camUp.z, 0.0f);
    renderData.GlobalTint = DirectX::XMFLOAT4(Color.x, Color.y, Color.z, Color.w);
    renderData.Brightness = Brightness;

    context->UpdateSubresource(RenderCB.Get(), 0, nullptr, &renderData, 0, 0);
}

DirectX::XMFLOAT3 ParticleSystemComponent::GetEmitterWorldPosition()
{
    const glm::vec3 position = GetWorldPosition();
    return DirectX::XMFLOAT3(position.x, position.y, position.z);
}
