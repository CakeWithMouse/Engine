#pragma once

#include <cstdint>
#include <vector>
#include <Engine/Components/GameComponents.h>
#include <Engine/Render/ImpostorAtlas.h>
#include <Engine/Render/MeshData.h>
#include <Engine/Render/SkeletalData.h>
#include <Engine/Render/SkeletalBaker.h>

/** One rendered unit. Ground vehicles only need yaw and a uniform scale. */
struct MeshInstance
{
    glm::vec3 Position{0.0f, 0.0f, 0.0f};
    float Yaw = 0.0f;   // radians
    float Scale = 1.0f; // uniform: instanced normals are transformed by the instance matrix
    glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
    bool bVisible = true;
    uint8_t Padding = 0;
    uint16_t ClipId = 0;
    float AnimationPhase = 0.0f;
    float Speed = 1.0f;
};
static_assert(sizeof(MeshInstance) == 48, "MeshInstance must be 48 bytes");

/**
 * Draws many copies of one mesh: a few detail levels, per-instance transform and colour,
 * one DrawIndexedInstanced per level in the colour pass and in each shadow cascade.
 *
 * Intended for strategy scenes with thousands of units. Instances are culled against the camera
 * frustum and bucketed by distance into LODs once per frame; the shadow pass uses its own set
 * (no camera frustum, so off-screen units still cast shadows) with a configurable LOD bias.
 */
class InstancedMeshComponent : public GameComponent
{
public:
    static constexpr int MaxLodCount = 4;

    InstancedMeshComponent();
    InstancedMeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);

    /** Sets the geometry of one detail level (0 = most detailed). Levels must be filled in order. */
    bool SetLod(int level, MeshLodData data);
    /** Sets skinned geometry for one LOD level using compact CrowdSkinnedVertex (32B). */
    bool SetSkinnedLod(int level, const std::vector<CrowdSkinnedVertex>& vertices, const std::vector<uint32_t>& indices);
    /** Sets skinned geometry for one LOD level using standard SkinnedVertex (64B). */
    bool SetSkinnedLod(int level, const std::vector<SkinnedVertex>& vertices, const std::vector<uint32_t>& indices);
    int GetLodCount() const { return LodCount; }

    /** Sets animation matrix texture baked by SkeletalBaker (RGBA32F matrix texture). */
    void SetAnimationTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv, const std::vector<BakedClipInfo>& clips = {});
    void SetAnimationTexture(ID3D11ShaderResourceView* srv, const std::vector<BakedClipInfo>& clips = {});
    bool HasAnimationTexture() const { return AnimationTextureSRV != nullptr; }
    void SetAnimationLerp(bool enabled) { bAnimationLerp = enabled; }

    /**
     * Distance from the camera at which the next LOD starts, in world units and ascending.
     * With N levels, N-1 distances are used; beyond the last distance instances keep the last LOD.
     * Manual distances are only used when screen heights are not set.
     */
    void SetLodDistances(const std::vector<float>& distances);
    /**
     * Preferred way to drive LODs: switch to the next level once the on-screen height of the
     * model (diameter of its bounding sphere, in pixels) drops below the threshold; the list is
     * descending. Independent of model size, window size and field of view, so one setting fits
     * every vehicle and no distance has to be tuned by hand.
     */
    void SetLodScreenHeights(const std::vector<float>& pixels);
    /** Instances further than this are not drawn at all (0 disables the limit). */
    void SetDrawDistance(float distance) { DrawDistance = distance; }
    /**
     * Shadow casters use LOD + bias + cascade index: the depth pass needs less detail than the
     * screen, and the farther a cascade reaches the coarser its casters can be.
     */
    void SetShadowLodBias(int bias) { ShadowLodBias = bias; }
    /** Units only cast into the first N cascades (0 = all). Far unit shadows cost the most. */
    void SetShadowCascadeLimit(int cascadeCount) { ShadowCascadeLimit = cascadeCount; }
    /** How far above a cascade a caster can still be and reach it with its shadow. */
    void SetShadowCasterExtrusion(float distance) { ShadowCasterExtrusion = distance; }
    /** Instances further than this from the camera do not cast shadows (0 disables the limit). */
    void SetShadowDistance(float distance) { ShadowDistance = distance; }
    /** Tints instances by their LOD so switching can be seen. Diagnostic only. */
    void SetLodDebugColors(bool enabled) { bLodDebugColors = enabled; }
    /**
     * Adds a generated sprite level behind the mesh levels: the model is rendered from a set of
     * angles once at load and far units are drawn as camera-facing quads. Needs one more entry in
     * SetLodScreenHeights (the mesh to sprite threshold).
     */
    void SetImpostorEnabled(bool enabled, const ImpostorAtlasSettings& settings = ImpostorAtlasSettings{});
    bool HasImpostor() const { return bImpostorEnabled && Impostor.IsValid(); }
    /**
     * Width of the dithered cross-fade around a switch, as a fraction of the threshold
     * (0.15 = +-15%). 0 switches instantly.
     */
    void SetLodFadeFraction(float fraction) { LodFadeFraction = fraction; }

    size_t AddInstance(const MeshInstance& instance);
    void ReserveInstances(size_t count) { Instances.reserve(count); }
    size_t GetMeshInstanceCount() const { return Instances.size(); }
    const MeshInstance& GetMeshInstance(size_t index) const { return Instances[index]; }
    void SetInstancePosition(size_t index, const glm::vec3& position);
    void SetInstanceYaw(size_t index, float yaw);
    void SetInstanceColor(size_t index, const glm::vec4& color);
    void SetInstanceVisible(size_t index, bool visible);
    void ClearInstances();

    // Diagnostics for the last frame.
    uint32_t GetDrawnInstances() const { return DrawnInstances; }
    uint32_t GetCulledInstances() const { return CulledInstances; }
    uint32_t GetDrawnInstancesForLod(int level) const;
    /** Instances drawn as sprites in the last frame. */
    uint32_t GetDrawnImpostors() const { return static_cast<uint32_t>(ImpostorLevel.ColorStaging.size()); }

    void CreateBuffers(ID3D11Device* Device) override;
    void Render(ID3D11DeviceContext* context) override;
    void RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass) override;
    // Visibility is decided per instance inside this component.
    bool SupportsFrustumCulling() const override { return false; }
    float GetLocalBoundingRadius() const override { return MeshRadius; }
    int GetIndexCount() override { return TotalIndexCount; }

private:
    struct LodLevel
    {
        MeshLodData Data;
        Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
        UINT IndexCount = 0;
        UINT VertexStride = sizeof(MeshVertex);

        // Per-frame instance streams: one for the camera pass and one per shadow cascade.
        std::vector<InstData> ColorStaging;
        Microsoft::WRL::ComPtr<ID3D11Buffer> ColorBuffer;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ColorSRV;
        size_t ColorCapacity = 0;
        std::vector<InstData> ShadowStaging[MaxShadowCascades];
        Microsoft::WRL::ComPtr<ID3D11Buffer> ShadowBuffer[MaxShadowCascades];
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShadowSRV[MaxShadowCascades];
        size_t ShadowCapacity[MaxShadowCascades] = {};
    };

    void BuildVisibleSets();
    /** Window height divided by tan(fov/2): converts a world radius and distance into pixels. */
    float ComputeScreenFactor() const;
    /** Number of levels including the sprite level, and the index of the sprite level. */
    int GetLevelCount() const;
    int GetImpostorLevelIndex() const;
    void BuildImpostorAtlas();
    void DrawImpostors(ID3D11DeviceContext* context);
    /** Full set of instances for the baked shadow map, built once and reused between frames. */
    bool EnsureStaticShadowData(ID3D11DeviceContext* context);
    void RenderStaticShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass);
    void EnsureFrameData(ID3D11DeviceContext* context);
    bool UploadStream(ID3D11DeviceContext* context,
                      const std::vector<InstData>& staging,
                      Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer,
                      Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
                      size_t& capacity);
    bool CreateLodBuffers(ID3D11Device* device, LodLevel& lod);
    int SelectLod(float distance, float screenHeightPixels) const;
    bool EnsureSkinnedShaders(ID3D11Device* device);
    void UpdateAnimClipConstantBuffer(ID3D11Device* device);

    LodLevel Lods[MaxLodCount];
    // The sprite level reuses the instance stream of LodLevel; its mesh buffers stay empty.
    LodLevel ImpostorLevel;
    ImpostorAtlas Impostor;
    ImpostorAtlasSettings ImpostorSettings;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ImpostorConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> ImpostorSampler;
    bool bImpostorEnabled = false;
    bool bImpostorBuildFailed = false;
    float LodFadeFraction = 0.06f;
    int LodCount = 0;
    std::vector<float> LodDistances;
    std::vector<float> LodScreenHeights;
    float DrawDistance = 0.0f;
    float ShadowDistance = 0.0f;
    int ShadowLodBias = 1;
    int ShadowCascadeLimit = 0;
    float ShadowCasterExtrusion = 200.0f;
    bool bLodDebugColors = false;

    std::vector<MeshInstance> Instances;
    std::vector<InstData> StaticShadowStaging;
    Microsoft::WRL::ComPtr<ID3D11Buffer> StaticShadowBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> StaticShadowSRV;
    size_t StaticShadowCapacity = 0;
    int StaticShadowLevel = 0;
    bool bStaticShadowDataDirty = true;
    float MeshRadius = 0.0f;   // local radius of LOD 0, unscaled
    int TotalIndexCount = 0;   // LOD 0, for diagnostics only

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AnimationTextureSRV;
    Microsoft::WRL::ComPtr<ID3D11Buffer> AnimClipConstantBuffer;
    std::vector<BakedClipInfo> AnimationClips;
    bool bAnimationLerp = false;
    bool bIsSkinned = false;
    bool bIsCrowdVertex = true;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> SkinnedVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> SkinnedPixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> SkinnedInputLayout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> SkinnedShadowVertexShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> SkinnedShadowInputLayout;

    uint64_t PreparedFrame = UINT64_MAX;
    uint32_t DrawnInstances = 0;
    uint32_t CulledInstances = 0;
};
