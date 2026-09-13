#pragma once
#include <d3d11.h>
#include <directxmath.h>
#include <wrl/client.h>
#include <vector>
#include "../../includes/GLM-master/glm/glm.hpp"
#include "../../includes/GLM-master/glm/gtc/constants.hpp"
#include "../../includes/GLM-master/glm/gtc/quaternion.hpp"
#include "../MainGame/BaseGameClass/Game.h"

constexpr int MaxPointLights = 8;

struct ConstantBufferData
{
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    DirectX::XMFLOAT4X4 invViewMatrix;
    DirectX::XMFLOAT4X4 invProjectionMatrix;
    DirectX::XMFLOAT4 ObjectColor;
    DirectX::XMFLOAT2 UVOffset;
    float HasTexture = 0.f;
    float padding;
    DirectX::XMFLOAT4 CameraPosition;
    DirectX::XMFLOAT4 LightPositions[MaxPointLights];
    DirectX::XMFLOAT4 LightColors[MaxPointLights];
    DirectX::XMFLOAT4 LightParams[MaxPointLights];
    DirectX::XMFLOAT4 LightMeta;
    DirectX::XMFLOAT4 ReflectionData;
    DirectX::XMFLOAT4X4 LightViewProjection[MaxShadowCascades];
    DirectX::XMFLOAT4 CascadeSplits;
    DirectX::XMFLOAT4 ShadowParams;
    DirectX::XMFLOAT4 LightDirection;
    DirectX::XMFLOAT4 DirectionalLightColorIntensity;
};
struct InstConstantBufferData
{
    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    DirectX::XMFLOAT4X4 invViewMatrix;
    DirectX::XMFLOAT4X4 invProjectionMatrix;
    DirectX::XMFLOAT4 CameraPosition;
    DirectX::XMFLOAT4 LightPositions[MaxPointLights];
    DirectX::XMFLOAT4 LightColors[MaxPointLights];
    DirectX::XMFLOAT4 LightParams[MaxPointLights];
    DirectX::XMFLOAT4 LightMeta;
    DirectX::XMFLOAT4 ReflectionData;
    DirectX::XMFLOAT4X4 LightViewProjection[MaxShadowCascades];
    DirectX::XMFLOAT4 CascadeSplits;
    DirectX::XMFLOAT4 ShadowParams;
    DirectX::XMFLOAT4 LightDirection;
    DirectX::XMFLOAT4 DirectionalLightColorIntensity;
};

struct InstData
{
    DirectX::XMFLOAT4X4 World;
    DirectX::XMFLOAT4 ObjectColor;
};

namespace GameComponentNames
{
    enum GeometryType
    {
        Object3D,
        Object2D
    };
}

class Game;

class GameComponent
{
public:
    GameComponent();
    GameComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    GameComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 Color);
    virtual ~GameComponent() = default;

    virtual void SetTexture(const std::string& texturePath);
    void SetCubeMap(CubeMapResource* cubeMap);
    void SetReflectionSettings(float strength, float fresnelPower = 5.0f);
    ID3D11ShaderResourceView* GetTexture() const { return textureSRV; }
    bool HasTexture() const { return textureSRV != nullptr; }
    bool HasCubeMap() const { return cubeMapSRV != nullptr; }
    
    virtual void CreateBuffers(Microsoft::WRL::ComPtr<ID3D11Device> Device);
    void SetGame(Game* game);
    void Update();
    virtual void Render(ID3D11DeviceContext* context);
    virtual void RenderShadow(ID3D11DeviceContext* context,
                              ID3D11VertexShader* shadowVertexShader,
                              ID3D11Buffer* shadowCB,
                              ID3D11InputLayout* shadowPrimitiveLayout,
                              ID3D11InputLayout* shadowMeshLayout,
                              const DirectX::XMFLOAT4X4& lightViewProjection);
    void RenderInstance(ID3D11DeviceContext* context);
    void SetCollision(bool bNewHasCollision) { bHasCollision = bNewHasCollision; }
    void SetVertexShader(ID3D11VertexShader* vs) { vertexShader = vs; };
    void SetPixelShader(ID3D11PixelShader* ps) { pixelShader = ps; };
    void SetParent(GameComponent* parent) { Parent = parent; }
    void SetParentWithoutScale(GameComponent* parent) { Parent = parent;bApplyParentScale = false; }
    void MarkTransformDirty() { bTransformDirty = true; }
    void SetPosition(glm::vec3 pos) { ComponentPosition = pos; }
    void SetRotation(glm::vec3 rot);
    void SetRotationQuat(const glm::quat& rotQuat);
    GameComponent* GetParent() const { return Parent; }

    bool HasCollison() { return bHasCollision; };
    bool HasOpacity() { return bHasOpacity; };
    bool IsSkybox() const { return bIsSkybox; }
    virtual bool DoSmthWithCollision(GameComponent* AnotherComponent) { return false; };
    GameComponent* CreateInstance(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 Color);

    ID3D11Buffer* GetIndexBuffer() { return ib; }
    ID3D11Buffer* GetVertexBuffer() { return vb; }
    ID3D11Buffer* const* GetConstantBuffer() { return &cb; }

    //virtual
    virtual void InitPoints(DirectX::XMFLOAT4* NewPoints, int pointCount, int* NewIndices, int indexCount);
    virtual void Tick(float deltaTime);
    virtual int GetIndexCount() { return indexCount; }
    virtual float GetRotationAngle(float totalTime) { return 0.f; };
    virtual float GetBoundingRadius() const;

    glm::vec3 GetCenter() const { return ComponentPosition; }
    glm::vec3 GetRotation() const { return ComponentRotation; }
    glm::vec3 GetScale() const { return ComponentScale; }
    glm::quat GetLocalRotationQuat() const { return LocalRotationQuaternion; }
    
    virtual DirectX::XMFLOAT4X4 GetWorldMatrix();
    size_t GetBufferElementCount() const { return InstanceArray.size(); }
protected:
    glm::mat4 GetWorldMatrixGLM();
    glm::quat GetWorldRotationQuat();
    
    std::vector<GameComponent*> InstanceArray;
    DirectX::XMMATRIX GetLocalMatrix() const;
    
    ID3D11ShaderResourceView* textureSRV = nullptr;
    ID3D11SamplerState* samplerState = nullptr;
    ID3D11ShaderResourceView* cubeMapSRV = nullptr;
    ID3D11SamplerState* cubeMapSamplerState = nullptr;
    glm::vec2 uvScale = glm::vec2(1.0f, 1.0f);
    glm::vec2 uvOffset = glm::vec2(0.0f, 0.0f);
    std::string texturePath;
    
    void CreateSamplerState();
private:

public:
protected:
    GameComponent* Parent{nullptr};
    bool bApplyParentScale = true;
    
    Game* GamePtr;
    
    bool bHasOpacity= false;
    bool bHasCollision = false;
    bool bTransformDirty = true;
    bool bIsSkybox = false;
    
    float VelocitySpeed = 1.f;
    float RotationSpeed = 0.1f;
    float ReflectionStrength = 0.0f;
    float FresnelPower = 5.0f;
    
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 CachedWorldMatrix;
    
    glm::vec4 Color{0.0f, 0.0f, 1.0f, 1.0f};
    glm::vec3 ComponentPosition{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentRotation{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentScale{1.f, 1.f, 1.f};
    glm::quat LocalRotationQuaternion{1.0f, 0.0f, 0.0f, 0.0f};
    bool bUseQuaternionRotation = false;

    ID3D11Buffer* ib{nullptr};
    ID3D11Buffer* vb{nullptr};
    ID3D11Buffer* cb{nullptr};
    ID3D11Buffer* inst_Basecb{nullptr};
    ID3D11Buffer* ins_cb{nullptr};
    ID3D11ShaderResourceView* m_InstanceSRV = nullptr;

    ID3D11VertexShader* GetVertexShader() { return vertexShader; }
    ID3D11PixelShader* GetPixelShader() { return pixelShader; }
    void CreateInstanceBuffer(ID3D11Device* Device);
    void UpdateInstanceBuffer(ID3D11DeviceContext* Context);
    
    ConstantBufferData ConstantPositionBuffer;
    InstConstantBufferData InstConstantPositionBuffer;
    
    InstData GetInstanceData();
    std::vector<InstData> InstBuffer;
    
    
    std::vector<DirectX::XMFLOAT4> points;
    std::vector<int> indices;
    int indexCount = 0;

    GameComponentNames::GeometryType ObjectType = GameComponentNames::GeometryType::Object2D;

    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
private:
    
};
