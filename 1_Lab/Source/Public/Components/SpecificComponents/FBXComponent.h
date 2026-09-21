#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "../../../../Source/Public/Components/GameComponents.h"

/** One sub-mesh of an imported model, CPU side (device independent). */
struct FBXImportedMesh
{
    // Interleaved position, normal, uv as three float4 per vertex (VertexFormat::Mesh), node transforms baked in.
    std::vector<DirectX::XMFLOAT4> Vertices;
    std::vector<uint32_t> Indices;
    DirectX::XMFLOAT4 Color{1.0f, 1.0f, 1.0f, 1.0f}; // diffuse colour of the sub-mesh material
};

/** Imported model, CPU side. Shared between components loading the same file. */
struct FBXImportedModel
{
    std::vector<FBXImportedMesh> Meshes;
    int TotalIndexCount = 0;
    float BoundingRadius = 0.0f;
};

/** GPU buffers of one sub-mesh for one device. */
struct FBXGpuMesh
{
    Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
    DirectX::XMFLOAT4 Color{1.0f, 1.0f, 1.0f, 1.0f};
    UINT IndexCount = 0;
};

/** GPU resources of a model for one device; published only when every buffer was created. */
struct FBXGpuModel
{
    std::vector<FBXGpuMesh> Meshes;
};

struct CachedFBXTexture
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TextureSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> SamplerState;
};

/**
 * Model loaded with Assimp. Caches are keyed by the canonical file path (and the device for GPU data)
 * and are meant to be used from the main thread only, like the immediate context itself.
 */
class FBXComponent : public GameComponent
{
public:
    FBXComponent();
    FBXComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    FBXComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);
    ~FBXComponent() override;

    /** Imports (or reuses) the CPU asset; GPU buffers are created as soon as a device is available. */
    bool LoadModel(const std::string& filePath);
    bool LoadTexture(const std::string& texturePath);

    virtual void Tick(float deltaTime) override;
    virtual void Render(ID3D11DeviceContext* Context) override;
    virtual void RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass) override;
    void InitPoints(DirectX::XMFLOAT4* NewPoints, int pointCount, int* NewIndices, int indexCount) override;
    int GetIndexCount() override { return totalIndexCount; }
    float GetBoundingRadius() const override;
    float GetLocalBoundingRadius() const override { return baseBoundingRadius; }

protected:
    virtual void CreateBuffers(ID3D11Device* Device) override;

private:
    std::string modelPath;
    std::shared_ptr<const FBXImportedModel> importedModel;
    std::shared_ptr<const FBXGpuModel> gpuModel;
    std::shared_ptr<CachedFBXTexture> cachedTexture;
    int totalIndexCount = 0;
    float baseBoundingRadius = 0.0f;
};
