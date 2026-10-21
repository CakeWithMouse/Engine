#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <Engine/Components/GameComponents.h>
#include <Engine/Render/SkeletalData.h>

/**
 * Component for rendering and animating skinned meshes.
 * Does not include Assimp headers; relies on SkeletalData and SkeletalLoader.
 */
class SkeletalMeshComponent : public GameComponent
{
public:
    SkeletalMeshComponent();
    SkeletalMeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    ~SkeletalMeshComponent() override;

    bool LoadFromVfs(std::string_view vpath);
    bool SetModel(std::shared_ptr<SkeletalModel> model);
    bool LoadTexture(const std::string& texturePath);

    void Tick(float deltaTime) override;
    void Render(ID3D11DeviceContext* context) override;

    const std::shared_ptr<SkeletalModel>& GetModel() const { return skeletalModel; }
    const std::vector<DirectX::XMFLOAT4X4>& GetBonePalette() const { return bonePalette; }

    void SetClipIndex(size_t index) { currentClipIndex = index; }
    size_t GetClipIndex() const { return currentClipIndex; }
    void SetPlaybackSpeed(float speed) { playbackSpeed = speed; }
    float GetPlaybackSpeed() const { return playbackSpeed; }
    float GetAnimationTime() const { return animationTime; }
    void SetAnimationTime(float time) { animationTime = time; }

    void SetModelYawDegrees(float yaw) { modelYawDegrees = yaw; }
    float GetModelYawDegrees() const { return modelYawDegrees; }

protected:
    virtual void CreateBuffers(ID3D11Device* device) override;

private:
    bool CompileShaders(ID3D11Device* device);

    std::string assetPath;
    std::shared_ptr<SkeletalModel> skeletalModel;
    std::vector<DirectX::XMFLOAT4X4> bonePalette;

    size_t currentClipIndex = 0;
    float animationTime = 0.0f;
    float playbackSpeed = 1.0f;
    bool isPlaying = true;
    float modelYawDegrees = 0.0f;

    // GPU resources
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> boneBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    UINT indexCount = 0;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
    std::string texturePath;
};
