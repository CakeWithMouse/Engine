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
    void SerializeSaveGame(SaveArchive& archive) override;
    SkeletalMeshComponent();
    SkeletalMeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    ~SkeletalMeshComponent() override;

    bool LoadFromVfs(std::string_view vpath);
    bool SetModel(std::shared_ptr<SkeletalModel> model);
    bool SetAssetModel(std::string path, std::shared_ptr<SkeletalModel> model);
    bool LoadTexture(const std::string& texturePath);
    /** Source material IDs (possibly sparse). Empty path uses the engine default.
     * SetMaterialAsset/SetMaterial on the object overrides every slot; clear that override to use slots. */
    const std::map<uint32_t, std::string>& GetMaterialSlots() const { return materialSlots; }
    bool SetMaterialSlot(uint32_t sourceIndex, const std::string& path);
    std::string SaveProjectData() const override;
    void LoadProjectData(const std::string& data) override;
    std::string DescribeEditorProperties() const override;
    std::string GetModelAsset() const override { return GameComponent::GetModelAsset().empty() ? assetPath : GameComponent::GetModelAsset(); }

    void Tick(float deltaTime) override;
    void Render(ID3D11DeviceContext* context) override;
    void RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass) override;

    const std::shared_ptr<SkeletalModel>& GetModel() const { return skeletalModel; }
    const std::vector<DirectX::XMFLOAT4X4>& GetBonePalette() const { return bonePalette; }

    void SetClipIndex(size_t index) { currentClipIndex = index; }
    size_t GetClipIndex() const { return currentClipIndex; }
    void SetPlaybackSpeed(float speed) { playbackSpeed = speed; }
    float GetPlaybackSpeed() const { return playbackSpeed; }
    float GetAnimationTime() const { return animationTime; }
    void SetAnimationTime(float time) { animationTime = time; }

    void SetModelYawDegrees(float yaw) { modelYawDegrees = yaw; if (GamePtr) GamePtr->InvalidateStaticShadow(); }
    float GetModelYawDegrees() const { return modelYawDegrees; }

    bool SupportsExactTrace() const override {return skeletalModel && skeletalModel->Clips.empty();}
    DirectX::XMFLOAT4X4 GetTraceMatrix() override;
    bool IsStaticShadowCaster() const override { return skeletalModel && skeletalModel->Clips.empty() && GameComponent::IsStaticShadowCaster(); }
    bool SupportsMaterials() const override { return true; }
    bool RequiresMaterial() const override { return true; }
    uint32_t GetMaterialVariant() const override;
    bool SupportsFrustumCulling() const override { return false; }

protected:
    virtual void CreateBuffers(ID3D11Device* device) override;

private:
    friend class Game;
    class MaterialPart;
    void AppendRenderParts(std::vector<GameComponent*>& output);
    static GameComponent* RenderOwnerOf(GameComponent* component);
    std::map<uint32_t, std::string> materialSlots;
    std::vector<std::unique_ptr<MaterialPart>> materialParts;
    bool CompileShaders(ID3D11Device* device);
    bool BindGeometry(ID3D11DeviceContext* context);

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

};
