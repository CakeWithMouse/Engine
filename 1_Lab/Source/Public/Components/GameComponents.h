#pragma once
#include <cstdint>
#include <d3d11.h>
#include <directxmath.h>
#include <memory>
#include <string>
#include <wrl/client.h>
#include <vector>
#include "../../includes/GLM-master/glm/glm.hpp"
#include "../../includes/GLM-master/glm/gtc/constants.hpp"
#include "../../includes/GLM-master/glm/gtc/quaternion.hpp"
#include "../Render/ShaderConstants.h"
#include "../MainGame/BaseGameClass/Game.h"

namespace GameComponentNames
{
    enum GeometryType
    {
        Object3D,
        Object2D
    };
}

class Game;

/** Shader pair of one pass variant, resolved lazily by Game::BindComponentShaders. */
struct ComponentShaderVariant
{
    ID3D11VertexShader* VertexShader = nullptr; // owned by the Game shader cache
    ID3D11PixelShader* PixelShader = nullptr;   // owned by the Game shader cache
    bool bResolved = false;
    bool bWritesGBuffer = false;
};

class GameComponent
{
public:
    GameComponent();
    GameComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    GameComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 Color);
    virtual ~GameComponent();
    GameComponent(const GameComponent&) = delete;
    GameComponent& operator=(const GameComponent&) = delete;

    // Material
    virtual void SetTexture(const std::string& texturePath);
    void SetCubeMap(CubeMapResource* cubeMap);
    void SetReflectionSettings(float strength, float fresnelPower = 5.0f);
    ID3D11ShaderResourceView* GetTexture() const { return textureSRV.Get(); }
    bool HasTexture() const { return textureSRV != nullptr; }
    bool HasCubeMap() const { return cubeMapSRV != nullptr; }

    // Lifecycle
    virtual void CreateBuffers(ID3D11Device* Device);
    void SetGame(Game* game) { GamePtr = game; }
    Game* GetGame() const { return GamePtr; }
    /** Per-frame: world transform and object constants from the frame snapshot. No GPU calls. */
    void Update();
    virtual void Tick(float deltaTime);
    virtual void Render(ID3D11DeviceContext* context);
    virtual void RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass);
    /** GPU work that must run once per frame independently of drawing (e.g. particle simulation). */
    virtual void DispatchCompute(ID3D11DeviceContext* context) {}
    virtual bool IsRenderable() const { return true; }
    virtual bool CastsShadow() const { return !bHasOpacity && !bIsSkybox; }
    virtual bool SupportsFrustumCulling() const { return InstanceArray.empty() && !bIsSkybox; }

    // Shaders
    void SetShaderNames(const std::string& vertexShaderName, const std::string& pixelShaderName);
    const std::string& GetVertexShaderName() const { return VertexShaderName; }
    const std::string& GetPixelShaderName() const { return PixelShaderName; }
    ComponentShaderVariant& GetShaderVariant(ShaderCompileVariant variant);
    void SetVertexShader(ID3D11VertexShader* vs) { vertexShader = vs; }
    void SetPixelShader(ID3D11PixelShader* ps) { pixelShader = ps; }

    // Transform
    void SetCollision(bool bNewHasCollision) { bHasCollision = bNewHasCollision; }
    /** Returns false (and keeps the old parent) if the new parent would create a cycle. */
    bool SetParent(GameComponent* parent);
    bool SetParentWithoutScale(GameComponent* parent);
    GameComponent* GetParent() const { return Parent; }
    /** Drops references to a component that is being destroyed (self and instances). */
    void ClearParentReferences(const GameComponent* removed);
    void MarkTransformDirty() { bTransformDirty = true; }
    void SetPosition(glm::vec3 pos) { ComponentPosition = pos; MarkTransformDirty(); }
    void SetScale(glm::vec3 scale) { ComponentScale = scale; MarkTransformDirty(); }
    void SetRotation(glm::vec3 rot);
    void SetRotationQuat(const glm::quat& rotQuat);

    glm::vec3 GetLocalPosition() const { return ComponentPosition; }
    glm::vec3 GetWorldPosition();
    /** World-space position (includes the parent chain). */
    glm::vec3 GetCenter() { return GetWorldPosition(); }
    glm::vec3 GetRotation() const { return ComponentRotation; }
    glm::vec3 GetScale() const { return ComponentScale; }
    glm::quat GetLocalRotationQuat() const { return LocalRotationQuaternion; }
    /** Cached world matrix (row-vector convention, not transposed). */
    DirectX::XMFLOAT4X4 GetWorldMatrix();
    uint64_t GetWorldVersion() const { return WorldVersion; }

    bool HasCollison() const { return bHasCollision; }
    bool HasOpacity() const { return bHasOpacity; }
    bool IsSkybox() const { return bIsSkybox; }
    virtual bool DoSmthWithCollision(GameComponent* AnotherComponent) { return false; }

    // Instancing: the owner is a template, only its instances are drawn (colour and shadow passes).
    GameComponent* CreateInstance(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 Color);
    size_t GetInstanceCount() const { return InstanceArray.size(); }
    bool HasInstances() const { return !InstanceArray.empty(); }

    ID3D11Buffer* GetIndexBuffer() const { return ib.Get(); }
    ID3D11Buffer* GetVertexBuffer() const { return vb.Get(); }

    virtual void InitPoints(DirectX::XMFLOAT4* NewPoints, int pointCount, int* NewIndices, int indexCount);
    virtual int GetIndexCount() { return indexCount; }
    virtual float GetRotationAngle(float totalTime) { return 0.f; }
    /** Radius in local space scaled by the local scale (used by gameplay collision). */
    virtual float GetBoundingRadius() const;
    /** Radius of the geometry around the local origin, without any scale. */
    virtual float GetLocalBoundingRadius() const { return LocalBoundingRadius; }
    /** Conservative world-space radius around GetWorldPosition(). */
    float GetWorldBoundingRadius();

protected:
    /** Local transform; override to change how rotation is built. */
    virtual DirectX::XMMATRIX GetLocalMatrix() const;
    glm::mat4 GetWorldMatrixGLM();
    glm::quat GetWorldRotationQuat();
    void UpdateObjectConstants();
    void UpdateInstanceTransform();
    InstData GetInstanceData() const;
    bool UploadInstanceData(ID3D11DeviceContext* context);
    void BindMaterialResources(ID3D11DeviceContext* context);
    void UnbindMaterialResources(ID3D11DeviceContext* context);
    void CreateSamplerState();

    std::vector<std::unique_ptr<GameComponent>> InstanceArray;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
    // Cube map resources are owned by Game::cubeMapCache, the component only observes them.
    ID3D11ShaderResourceView* cubeMapSRV = nullptr;
    ID3D11SamplerState* cubeMapSamplerState = nullptr;
    glm::vec2 uvScale = glm::vec2(1.0f, 1.0f);
    glm::vec2 uvOffset = glm::vec2(0.0f, 0.0f);
    std::string texturePath;

    GameComponent* Parent{nullptr};
    bool bApplyParentScale = true;

    Game* GamePtr{nullptr};

    bool bHasOpacity = false;
    bool bHasCollision = false;
    bool bTransformDirty = true;
    bool bIsSkybox = false;

    float VelocitySpeed = 1.f;
    float RotationSpeed = 0.1f;
    float ReflectionStrength = 0.0f;
    float FresnelPower = 5.0f;

    DirectX::XMFLOAT4X4 CachedWorldMatrix{};
    uint64_t WorldVersion = 0;
    uint64_t CachedParentVersion = 0;
    const GameComponent* CachedParent = nullptr;

    glm::vec4 Color{0.0f, 0.0f, 1.0f, 1.0f};
    glm::vec3 ComponentPosition{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentRotation{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentScale{1.f, 1.f, 1.f};
    glm::quat LocalRotationQuaternion{1.0f, 0.0f, 0.0f, 0.0f};
    bool bUseQuaternionRotation = false;

    Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> inst_Basecb;
    // Instance stream: capacity grows geometrically, count is InstanceArray.size().
    Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> instanceSRV;
    size_t InstanceCapacity = 0;
    uint64_t InstanceUploadFrame = UINT64_MAX;
    std::vector<InstData> InstanceStaging;

    ID3D11VertexShader* GetVertexShader() const { return vertexShader; }
    ID3D11PixelShader* GetPixelShader() const { return pixelShader; }

    ConstantBufferData ConstantPositionBuffer{};
    InstConstantBufferData InstConstantPositionBuffer{};

    std::vector<DirectX::XMFLOAT4> points;
    std::vector<int> indices;
    int indexCount = 0;
    float LocalBoundingRadius = 0.0f;

    GameComponentNames::GeometryType ObjectType = GameComponentNames::GeometryType::Object2D;

    // Currently bound pass variant (set by Game::BindComponentShaders).
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;

private:
    std::string VertexShaderName;
    std::string PixelShaderName;
    ComponentShaderVariant ShaderVariants[2];
};
