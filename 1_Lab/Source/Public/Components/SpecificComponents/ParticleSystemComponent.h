#pragma once

#include "../../../../Source/Public/Components/GameComponents.h"
#include <algorithm>
#include <wrl/client.h>

struct GPUParticleData
{
    DirectX::XMFLOAT4 PositionLife;
    DirectX::XMFLOAT4 VelocityLifetime;
    DirectX::XMFLOAT4 ColorSize;
};

struct GPUParticleSimulationCB
{
    float DeltaTime;
    float TotalTime;
    unsigned int ParticleCount;
    float BaseLifetime;
    DirectX::XMFLOAT3 EmitterPosition;
    float SpreadRadius;
    float BaseSpeed;
    float SpeedRandomness;
    float MinLifeFraction;
    float MaxDistance;
    DirectX::XMFLOAT3 BaseVelocity;
    float padding0;
    DirectX::XMFLOAT3 Acceleration;
    float padding1;
    DirectX::XMFLOAT4 BaseColor;
    float BaseSize;
    float SizeRandomness;
    float padding2;
    float padding3;
    DirectX::XMFLOAT4X4 SimulationViewMatrix;
    DirectX::XMFLOAT4X4 SimulationProjectionMatrix;
    DirectX::XMFLOAT4X4 SimulationInvViewMatrix;
    DirectX::XMFLOAT4X4 SimulationInvProjectionMatrix;
    DirectX::XMFLOAT4 SimulationCameraPosition;
    DirectX::XMFLOAT4 DepthCollisionParams;
};

struct GPUParticleSortPair
{
    unsigned int DepthKey;
    unsigned int ParticleIndex;
};

struct GPUParticleSortCB
{
    DirectX::XMFLOAT4X4 SortViewMatrix;
    unsigned int SortParticleCount;
    unsigned int SortElementCount;
    unsigned int BitonicLevel;
    unsigned int BitonicLevelMask;
};

struct GPUParticleRenderCB
{
    DirectX::XMFLOAT4X4 ViewMatrix;
    DirectX::XMFLOAT4X4 ProjectionMatrix;
    DirectX::XMFLOAT4 CameraRight;
    DirectX::XMFLOAT4 CameraUp;
    DirectX::XMFLOAT4 GlobalTint;
    float Brightness;
    float padding0;
    float padding1;
    float padding2;
};

class ParticleSystemComponent : public GameComponent
{
public:
    ParticleSystemComponent();
    ParticleSystemComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);
    ~ParticleSystemComponent() override;

    void SetParticleCount(unsigned int newCount);
    void SetBaseLifetime(float newLifetime) { BaseLifetime = newLifetime; }
    void SetSpreadRadius(float newRadius) { SpreadRadius = newRadius; }
    void SetRadialSpeed(float newSpeed) { BaseSpeed = std::max(0.0f, newSpeed); }
    void SetMaxDistance(float newDistance) { MaxDistance = std::max(0.01f, newDistance); }
    void SetBaseVelocity(const glm::vec3& velocity) { BaseVelocity = velocity; }
    void SetAcceleration(const glm::vec3& acceleration) { Acceleration = acceleration; }
    void SetBaseSize(float size) { BaseSize = size; }
    void SetBrightness(float newBrightness) { Brightness = std::max(0.0f, newBrightness); }
    void SetRandomness(float speedRandomness, float sizeRandomness);
    void SetDepthCollision(bool enabled) { bDepthCollisionEnabled = enabled; }
    void SetDepthCollisionResponse(float depthBias, float bounce, float friction)
    {
        DepthCollisionBias = std::max(0.0f, depthBias);
        DepthCollisionBounce = std::max(0.0f, bounce);
        DepthCollisionFriction = std::max(0.0f, std::min(friction, 1.0f));
    }

    void CreateBuffers(Microsoft::WRL::ComPtr<ID3D11Device> Device) override;
    void Tick(float deltaTime) override;
    void Render(ID3D11DeviceContext* context) override;
    void RenderShadow(ID3D11DeviceContext* context,
                      ID3D11VertexShader* shadowVertexShader,
                      ID3D11Buffer* shadowCB,
                      ID3D11InputLayout* shadowPrimitiveLayout,
                      ID3D11InputLayout* shadowMeshLayout,
                      const DirectX::XMFLOAT4X4& lightViewProjection) override;

private:
    bool CompileShaders(ID3D11Device* device);
    bool CreateParticleBuffers(ID3D11Device* device);
    void DispatchSimulation(ID3D11DeviceContext* context);
    void DispatchSort(ID3D11DeviceContext* context);
    void UpdateRenderConstants(ID3D11DeviceContext* context);
    DirectX::XMFLOAT3 GetEmitterWorldPosition();

    static constexpr unsigned int ThreadGroupSize = 256u;
    static constexpr unsigned int DefaultParticleCount = 4096u;

    unsigned int ParticleCount = DefaultParticleCount;
    unsigned int ReadBufferIndex = 0u;
    float LastDeltaTime = 0.016f;
    float SimulationTime = 0.0f;
    float BaseLifetime = 2.8f;
    float SpreadRadius = 0.0f;
    float BaseSpeed = 18.0f;
    float MaxDistance = 220.0f;
    glm::vec3 BaseVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 Acceleration = glm::vec3(0.0f, -9.8f, 0.0f);
    float SpeedRandomness = 0.9f;
    float BaseSize = 0.22f;
    float Brightness = 3.0f;
    float SizeRandomness = 0.55f;
    float MinLifeFraction = 0.35f;
    bool bDepthCollisionEnabled = true;
    float DepthCollisionBias = 0.08f;
    float DepthCollisionBounce = 0.72f;
    float DepthCollisionFriction = 0.10f;

    ID3D11Buffer* ParticleBuffers[2] = {nullptr, nullptr};
    ID3D11ShaderResourceView* ParticleSRV[2] = {nullptr, nullptr};
    ID3D11UnorderedAccessView* ParticleUAV[2] = {nullptr, nullptr};
    ID3D11Buffer* ParticleSortBuffer = nullptr;
    ID3D11ShaderResourceView* ParticleSortSRV = nullptr;
    ID3D11UnorderedAccessView* ParticleSortUAV = nullptr;
    unsigned int SortElementCount = 0u;

    ID3D11ComputeShader* ComputeShader = nullptr;
    ID3D11ComputeShader* BuildSortKeysShader = nullptr;
    ID3D11ComputeShader* BitonicSortShader = nullptr;
    ID3D11VertexShader* ParticleVertexShader = nullptr;
    ID3D11PixelShader* ParticlePixelShader = nullptr;
    ID3D11Buffer* SimulationCB = nullptr;
    ID3D11Buffer* RenderCB = nullptr;
    ID3D11Buffer* SortCB = nullptr;
};
