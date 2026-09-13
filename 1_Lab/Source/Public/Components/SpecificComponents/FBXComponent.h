#pragma once

#include <string>
#include <vector>
#include <memory>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "../../../../Source/Public/Components/GameComponents.h"

struct RenderMesh
{
    std::vector<DirectX::XMFLOAT4> vertices; // Position + Color/Normal
    std::vector<int> indices;
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    DirectX::XMFLOAT4 Color;
    int IndexCount = 0;
    int VertexCount = 0;
};

struct VertexData
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexCoord;
};

struct MeshData
{
    std::vector<VertexData> Vertices;
    std::vector<int> Indices;
    std::string MaterialName;
    DirectX::XMFLOAT4 Color;
};

struct CachedFBXMesh
{
    Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
    DirectX::XMFLOAT4 Color;
    int IndexCount = 0;
    int VertexCount = 0;
};

struct CachedFBXModel
{
    std::vector<CachedFBXMesh> Meshes;
    int TotalIndexCount = 0;
    float BaseBoundingRadius = 0.0f;
};

struct CachedFBXTexture
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TextureSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> SamplerState;
};

class FBXComponent : public GameComponent
{
public:
    FBXComponent();
    FBXComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    FBXComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);
    ~FBXComponent();

    bool LoadModel(const std::string& filePath);
    bool LoadTexture(const std::string& texturePath);
    
    virtual void Tick(float deltaTime) override;
    virtual void Render(ID3D11DeviceContext* Context) override;
    virtual void RenderShadow(ID3D11DeviceContext* context,
                              ID3D11VertexShader* shadowVertexShader,
                              ID3D11Buffer* shadowCB,
                              ID3D11InputLayout* shadowPrimitiveLayout,
                              ID3D11InputLayout* shadowMeshLayout,
                              const DirectX::XMFLOAT4X4& lightViewProjection) override;
    void InitPoints(DirectX::XMFLOAT4* NewPoints, int pointCount, int* NewIndices, int indexCount) override;
    int GetIndexCount() override { return totalIndexCount; }
    float GetBoundingRadius() const override;
    
    void CreateInputLayout();

private:
    void ProcessNode(aiNode* node, const aiScene* scene);
    MeshData ProcessMesh(aiMesh* mesh, const aiScene* scene);
    
    virtual void CreateBuffers(Microsoft::WRL::ComPtr<ID3D11Device> Device) override;
    void CreateMeshBuffers();
    
    DirectX::XMFLOAT4 ProcessMaterial(aiMaterial* material);
    DirectX::XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& matrix);
    
    
    std::vector<RenderMesh> renderMeshes;
    int totalIndexCount = 0;
    
    ID3D11ShaderResourceView* textureSRV = nullptr;
    ID3D11SamplerState* samplerState = nullptr;
    bool hasTexture = false;
    
    ID3D11InputLayout* inputLayout = nullptr;
    std::string modelPath;
    std::shared_ptr<CachedFBXModel> cachedModel;
    std::shared_ptr<CachedFBXTexture> cachedTexture;
    float baseBoundingRadius = 0.0f;
};
