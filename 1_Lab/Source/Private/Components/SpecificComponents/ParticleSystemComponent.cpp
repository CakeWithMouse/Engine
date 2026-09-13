#include "../../../../Source/Public/Components/SpecificComponents/ParticleSystemComponent.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <d3dcompiler.h>
#include <iostream>
#include <random>

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
    std::wstring ParticleShaderPath = L"Source/Shaders/GPUParticleSystem.hlsl";

    unsigned int NextPowerOfTwo(unsigned int value)
    {
        unsigned int result = 1u;
        while (result < value)
        {
            result <<= 1u;
        }

        return result;
    }

    template <typename T>
    void ReleaseIfValid(T*& resource)
    {
        if (resource)
        {
            resource->Release();
            resource = nullptr;
        }
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

ParticleSystemComponent::~ParticleSystemComponent()
{
    for (int i = 0; i < 2; ++i)
    {
        if (ParticleUAV[i]) ParticleUAV[i]->Release();
        if (ParticleSRV[i]) ParticleSRV[i]->Release();
        if (ParticleBuffers[i]) ParticleBuffers[i]->Release();
    }

    ReleaseIfValid(ParticleSortUAV);
    ReleaseIfValid(ParticleSortSRV);
    ReleaseIfValid(ParticleSortBuffer);
    if (ComputeShader) ComputeShader->Release();
    if (BuildSortKeysShader) BuildSortKeysShader->Release();
    if (BitonicSortShader) BitonicSortShader->Release();
    if (ParticleVertexShader) ParticleVertexShader->Release();
    if (ParticlePixelShader) ParticlePixelShader->Release();
    if (SimulationCB) SimulationCB->Release();
    if (RenderCB) RenderCB->Release();
    if (SortCB) SortCB->Release();
}

void ParticleSystemComponent::SetParticleCount(unsigned int newCount)
{
    const unsigned int clampedCount = std::max(64u, std::min(newCount, 200000u));
    if (clampedCount == ParticleCount)
    {
        return;
    }

    ParticleCount = clampedCount;
    if (GamePtr == nullptr || GamePtr->GetContext() == nullptr)
    {
        return;
    }

    ID3D11Device* device = nullptr;
    GamePtr->GetContext()->GetDevice(&device);
    if (!device)
    {
        return;
    }

    CreateParticleBuffers(device);
    device->Release();
}

void ParticleSystemComponent::SetRandomness(float speedRandomness, float sizeRandomness)
{
    SpeedRandomness = std::max(0.0f, speedRandomness);
    SizeRandomness = std::max(0.0f, sizeRandomness);
}

void ParticleSystemComponent::CreateBuffers(Microsoft::WRL::ComPtr<ID3D11Device> device)
{
    if (!device)
    {
        return;
    }

    if (!CompileShaders(device.Get()))
    {
        std::cout << "ParticleSystemComponent: failed to compile shaders." << std::endl;
        return;
    }

    if (!CreateParticleBuffers(device.Get()))
    {
        std::cout << "ParticleSystemComponent: failed to create particle buffers." << std::endl;
        return;
    }

    D3D11_BUFFER_DESC simCBDesc = {};
    simCBDesc.Usage = D3D11_USAGE_DEFAULT;
    simCBDesc.ByteWidth = sizeof(GPUParticleSimulationCB);
    simCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    simCBDesc.CPUAccessFlags = 0;
    device->CreateBuffer(&simCBDesc, nullptr, &SimulationCB);

    D3D11_BUFFER_DESC renderCBDesc = {};
    renderCBDesc.Usage = D3D11_USAGE_DEFAULT;
    renderCBDesc.ByteWidth = sizeof(GPUParticleRenderCB);
    renderCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    renderCBDesc.CPUAccessFlags = 0;
    device->CreateBuffer(&renderCBDesc, nullptr, &RenderCB);

    D3D11_BUFFER_DESC sortCBDesc = {};
    sortCBDesc.Usage = D3D11_USAGE_DEFAULT;
    sortCBDesc.ByteWidth = sizeof(GPUParticleSortCB);
    sortCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sortCBDesc.CPUAccessFlags = 0;
    device->CreateBuffer(&sortCBDesc, nullptr, &SortCB);
}

void ParticleSystemComponent::Tick(float deltaTime)
{
    GameComponent::Tick(deltaTime);
    LastDeltaTime = std::max(0.0f, std::min(deltaTime, 0.05f));
    SimulationTime += LastDeltaTime;
}

void ParticleSystemComponent::Render(ID3D11DeviceContext* context)
{
    if (!context || !ComputeShader || !ParticleVertexShader || !ParticlePixelShader ||
        !BuildSortKeysShader || !BitonicSortShader ||
        !SimulationCB || !RenderCB || !SortCB ||
        !ParticleSRV[ReadBufferIndex] || !ParticleUAV[1u - ReadBufferIndex] ||
        !ParticleSortSRV || !ParticleSortUAV)
    {
        return;
    }

    DispatchSimulation(context);
    DispatchSort(context);
    UpdateRenderConstants(context);

    ID3D11InputLayout* previousLayout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY previousTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    context->IAGetInputLayout(&previousLayout);
    context->IAGetPrimitiveTopology(&previousTopology);

    context->VSSetShader(ParticleVertexShader, nullptr, 0);
    context->PSSetShader(ParticlePixelShader, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &RenderCB);
    context->PSSetConstantBuffers(0, 1, &RenderCB);
    ID3D11ShaderResourceView* vertexSRVs[2] = {ParticleSRV[ReadBufferIndex], ParticleSortSRV};
    context->VSSetShaderResources(0, 2, vertexSRVs);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->DrawInstanced(4, ParticleCount, 0, 0);

    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->VSSetShaderResources(0, 2, nullSRVs);
    context->IASetInputLayout(previousLayout);
    context->IASetPrimitiveTopology(previousTopology);
    if (previousLayout)
    {
        previousLayout->Release();
    }
}

void ParticleSystemComponent::RenderShadow(ID3D11DeviceContext* context,
                                           ID3D11VertexShader* shadowVertexShader,
                                           ID3D11Buffer* shadowCB,
                                           ID3D11InputLayout* shadowPrimitiveLayout,
                                           ID3D11InputLayout* shadowMeshLayout,
                                           const DirectX::XMFLOAT4X4& lightViewProjection)
{
    (void)context;
    (void)shadowVertexShader;
    (void)shadowCB;
    (void)shadowPrimitiveLayout;
    (void)shadowMeshLayout;
    (void)lightViewProjection;
}

bool ParticleSystemComponent::CompileShaders(ID3D11Device* device)
{
    if (!device)
    {
        return false;
    }

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* csBlob = nullptr;
    ID3DBlob* buildSortBlob = nullptr;
    ID3DBlob* bitonicSortBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    auto releaseBlobs = [&]()
    {
        if (vsBlob) { vsBlob->Release(); vsBlob = nullptr; }
        if (psBlob) { psBlob->Release(); psBlob = nullptr; }
        if (csBlob) { csBlob->Release(); csBlob = nullptr; }
        if (buildSortBlob) { buildSortBlob->Release(); buildSortBlob = nullptr; }
        if (bitonicSortBlob) { bitonicSortBlob->Release(); bitonicSortBlob = nullptr; }
        if (errorBlob)
        {
            errorBlob->Release();
            errorBlob = nullptr;
        }
    };

    auto compileEntry = [&](const char* entryPoint, const char* target, ID3DBlob** blob) -> bool
    {
        HRESULT hr = D3DCompileFromFile(
            ParticleShaderPath.c_str(),
            nullptr,
            nullptr,
            entryPoint,
            target,
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0,
            blob,
            &errorBlob
        );
        if (SUCCEEDED(hr))
        {
            if (errorBlob)
            {
                errorBlob->Release();
                errorBlob = nullptr;
            }
            return true;
        }

        if (errorBlob)
        {
            std::cout << static_cast<const char*>(errorBlob->GetBufferPointer()) << std::endl;
            errorBlob->Release();
            errorBlob = nullptr;
        }

        return false;
    };

    if (!compileEntry("VSMain", "vs_5_0", &vsBlob) ||
        !compileEntry("PSMain", "ps_5_0", &psBlob) ||
        !compileEntry("CSMain", "cs_5_0", &csBlob) ||
        !compileEntry("CSBuildSortKeys", "cs_5_0", &buildSortBlob) ||
        !compileEntry("CSBitonicSort", "cs_5_0", &bitonicSortBlob))
    {
        releaseBlobs();
        return false;
    }

    ID3D11VertexShader* newVertexShader = nullptr;
    ID3D11PixelShader* newPixelShader = nullptr;
    ID3D11ComputeShader* newComputeShader = nullptr;
    ID3D11ComputeShader* newBuildSortKeysShader = nullptr;
    ID3D11ComputeShader* newBitonicSortShader = nullptr;

    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &newVertexShader);
    if (SUCCEEDED(hr))
    {
        hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &newPixelShader);
    }
    if (SUCCEEDED(hr))
    {
        hr = device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &newComputeShader);
    }
    if (SUCCEEDED(hr))
    {
        hr = device->CreateComputeShader(buildSortBlob->GetBufferPointer(), buildSortBlob->GetBufferSize(), nullptr, &newBuildSortKeysShader);
    }
    if (SUCCEEDED(hr))
    {
        hr = device->CreateComputeShader(bitonicSortBlob->GetBufferPointer(), bitonicSortBlob->GetBufferSize(), nullptr, &newBitonicSortShader);
    }

    releaseBlobs();

    if (FAILED(hr) || !newVertexShader || !newPixelShader || !newComputeShader ||
        !newBuildSortKeysShader || !newBitonicSortShader)
    {
        if (newVertexShader) newVertexShader->Release();
        if (newPixelShader) newPixelShader->Release();
        if (newComputeShader) newComputeShader->Release();
        if (newBuildSortKeysShader) newBuildSortKeysShader->Release();
        if (newBitonicSortShader) newBitonicSortShader->Release();
        return false;
    }

    if (ParticleVertexShader) ParticleVertexShader->Release();
    if (ParticlePixelShader) ParticlePixelShader->Release();
    if (ComputeShader) ComputeShader->Release();
    if (BuildSortKeysShader) BuildSortKeysShader->Release();
    if (BitonicSortShader) BitonicSortShader->Release();

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
        if (ParticleUAV[i]) { ParticleUAV[i]->Release(); ParticleUAV[i] = nullptr; }
        if (ParticleSRV[i]) { ParticleSRV[i]->Release(); ParticleSRV[i] = nullptr; }
        if (ParticleBuffers[i]) { ParticleBuffers[i]->Release(); ParticleBuffers[i] = nullptr; }
    }
    ReleaseIfValid(ParticleSortUAV);
    ReleaseIfValid(ParticleSortSRV);
    ReleaseIfValid(ParticleSortBuffer);
    SortElementCount = NextPowerOfTwo(ParticleCount);

    std::vector<GPUParticleData> initialParticles;
    initialParticles.resize(ParticleCount);

    std::mt19937 rng(
        static_cast<unsigned int>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
        )
    );
    std::uniform_real_distribution<float> rand01(0.0f, 1.0f);
    std::uniform_real_distribution<float> randSigned(-1.0f, 1.0f);

    for (unsigned int i = 0; i < ParticleCount; ++i)
    {
        GPUParticleData& p = initialParticles[i];
        p.PositionLife = DirectX::XMFLOAT4(0.0f, -100000.0f, 0.0f, 0.0f);
        p.VelocityLifetime = DirectX::XMFLOAT4(randSigned(rng), 1.0f + rand01(rng), randSigned(rng), BaseLifetime);
        p.ColorSize = DirectX::XMFLOAT4(Color.x, Color.y, Color.z, BaseSize);
    }

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(GPUParticleData) * initialParticles.size());
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(GPUParticleData);

    for (int i = 0; i < 2; ++i)
    {
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = initialParticles.data();

        HRESULT hr = device->CreateBuffer(&bufferDesc, &initData, &ParticleBuffers[i]);
        if (FAILED(hr))
        {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = ParticleCount;
        hr = device->CreateShaderResourceView(ParticleBuffers[i], &srvDesc, &ParticleSRV[i]);
        if (FAILED(hr))
        {
            return false;
        }

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = ParticleCount;
        hr = device->CreateUnorderedAccessView(ParticleBuffers[i], &uavDesc, &ParticleUAV[i]);
        if (FAILED(hr))
        {
            return false;
        }
    }

    D3D11_BUFFER_DESC sortBufferDesc = {};
    sortBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    sortBufferDesc.ByteWidth = static_cast<UINT>(sizeof(GPUParticleSortPair) * SortElementCount);
    sortBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    sortBufferDesc.CPUAccessFlags = 0;
    sortBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    sortBufferDesc.StructureByteStride = sizeof(GPUParticleSortPair);

    HRESULT hr = device->CreateBuffer(&sortBufferDesc, nullptr, &ParticleSortBuffer);
    if (FAILED(hr))
    {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC sortSRVDesc = {};
    sortSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    sortSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sortSRVDesc.Buffer.FirstElement = 0;
    sortSRVDesc.Buffer.NumElements = SortElementCount;
    hr = device->CreateShaderResourceView(ParticleSortBuffer, &sortSRVDesc, &ParticleSortSRV);
    if (FAILED(hr))
    {
        return false;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC sortUAVDesc = {};
    sortUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    sortUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    sortUAVDesc.Buffer.FirstElement = 0;
    sortUAVDesc.Buffer.NumElements = SortElementCount;
    hr = device->CreateUnorderedAccessView(ParticleSortBuffer, &sortUAVDesc, &ParticleSortUAV);
    if (FAILED(hr))
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
    simulationData.BaseVelocity = DirectX::XMFLOAT3(BaseVelocity.x, BaseVelocity.y, BaseVelocity.z);
    simulationData.Acceleration = DirectX::XMFLOAT3(Acceleration.x, Acceleration.y, Acceleration.z);
    simulationData.BaseColor = DirectX::XMFLOAT4(Color.x, Color.y, Color.z, Color.w);
    simulationData.BaseSize = BaseSize * std::max(0.001f, ComponentScale.x);
    simulationData.SizeRandomness = SizeRandomness;
    if (GamePtr && GamePtr->GetPlayer())
    {
        simulationData.SimulationViewMatrix = GamePtr->GetViewMatrix();
        simulationData.SimulationProjectionMatrix = GamePtr->GetProjectionMatrix();

        const XMMATRIX viewStored = XMLoadFloat4x4(&simulationData.SimulationViewMatrix);
        const XMMATRIX projStored = XMLoadFloat4x4(&simulationData.SimulationProjectionMatrix);
        const XMMATRIX viewOriginal = XMMatrixTranspose(viewStored);
        const XMMATRIX projOriginal = XMMatrixTranspose(projStored);
        XMStoreFloat4x4(&simulationData.SimulationInvViewMatrix, XMMatrixTranspose(XMMatrixInverse(nullptr, viewOriginal)));
        XMStoreFloat4x4(&simulationData.SimulationInvProjectionMatrix, XMMatrixTranspose(XMMatrixInverse(nullptr, projOriginal)));

        const glm::vec3 cameraPosition = GamePtr->GetPlayer()->GetPosition();
        simulationData.SimulationCameraPosition = XMFLOAT4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f);
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
        context->OMGetRenderTargets(1, &previousRTV, &previousDSV);
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    context->UpdateSubresource(SimulationCB, 0, nullptr, &simulationData, 0, 0);
    context->CSSetShader(ComputeShader, nullptr, 0);
    context->CSSetConstantBuffers(0, 1, &SimulationCB);
    context->CSSetShaderResources(0, 1, &ParticleSRV[ReadBufferIndex]);
    context->CSSetShaderResources(2, 1, &depthSRV);
    context->CSSetUnorderedAccessViews(0, 1, &ParticleUAV[1u - ReadBufferIndex], nullptr);

    const unsigned int dispatchCount = (ParticleCount + ThreadGroupSize - 1u) / ThreadGroupSize;
    context->Dispatch(dispatchCount, 1, 1);

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
    if (!context || !SortCB || !BuildSortKeysShader || !BitonicSortShader ||
        !ParticleSRV[ReadBufferIndex] || !ParticleSortUAV || SortElementCount == 0u)
    {
        return;
    }

    GPUParticleSortCB sortData = {};
    if (GamePtr)
    {
        sortData.SortViewMatrix = GamePtr->GetViewMatrix();
    }
    sortData.SortParticleCount = ParticleCount;
    sortData.SortElementCount = SortElementCount;

    context->UpdateSubresource(SortCB, 0, nullptr, &sortData, 0, 0);
    context->CSSetShader(BuildSortKeysShader, nullptr, 0);
    context->CSSetConstantBuffers(1, 1, &SortCB);
    context->CSSetShaderResources(0, 1, &ParticleSRV[ReadBufferIndex]);
    context->CSSetUnorderedAccessViews(1, 1, &ParticleSortUAV, nullptr);

    const unsigned int sortDispatchCount = (SortElementCount + ThreadGroupSize - 1u) / ThreadGroupSize;
    context->Dispatch(sortDispatchCount, 1, 1);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->CSSetShaderResources(0, 1, &nullSRV);

    context->CSSetShader(BitonicSortShader, nullptr, 0);
    for (unsigned int level = 2u; level <= SortElementCount; level <<= 1u)
    {
        sortData.BitonicLevel = level;
        for (unsigned int levelMask = level >> 1u; levelMask > 0u; levelMask >>= 1u)
        {
            sortData.BitonicLevelMask = levelMask;
            context->UpdateSubresource(SortCB, 0, nullptr, &sortData, 0, 0);
            context->Dispatch(sortDispatchCount, 1, 1);
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
    renderData.ViewMatrix = GamePtr->GetViewMatrix();
    renderData.ProjectionMatrix = GamePtr->GetProjectionMatrix();

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

    context->UpdateSubresource(RenderCB, 0, nullptr, &renderData, 0, 0);
}

DirectX::XMFLOAT3 ParticleSystemComponent::GetEmitterWorldPosition()
{
    const DirectX::XMFLOAT4X4 world = GetWorldMatrix();
    return DirectX::XMFLOAT3(world._41, world._42, world._43);
}
