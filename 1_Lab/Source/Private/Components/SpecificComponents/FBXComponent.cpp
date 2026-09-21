#include "../../../Public/Components/SpecificComponents/FBXComponent.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <unordered_map>
#include <utility>
#include "../../../Public/Render/TextureLoader.h"

namespace
{
    constexpr UINT MeshVertexStride = sizeof(DirectX::XMFLOAT4) * 3;

    // Main-thread caches. Weak references: an asset lives while at least one component uses it.
    std::unordered_map<std::string, std::weak_ptr<const FBXImportedModel>> importedModelCache;
    std::map<std::pair<const ID3D11Device*, std::string>, std::weak_ptr<const FBXGpuModel>> gpuModelCache;
    std::map<std::pair<const ID3D11Device*, std::string>, std::weak_ptr<CachedFBXTexture>> textureCache;

    /** Canonical asset identity: absolute, normalised, case-folded (Windows paths are case-insensitive). */
    std::string MakeAssetKey(const std::string& path)
    {
        std::error_code error;
        std::filesystem::path fsPath = std::filesystem::u8path(path);
        std::filesystem::path canonical = std::filesystem::weakly_canonical(fsPath, error);
        std::string key = (error ? fsPath.lexically_normal() : canonical).generic_u8string();
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return key;
    }

    DirectX::XMFLOAT4 ReadDiffuseColor(const aiMaterial* material)
    {
        aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
        if (material)
        {
            material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        }
        return DirectX::XMFLOAT4(color.r, color.g, color.b, color.a);
    }

    FBXImportedMesh ImportMesh(const aiMesh* mesh, const aiScene* scene)
    {
        FBXImportedMesh result;
        result.Vertices.reserve(static_cast<size_t>(mesh->mNumVertices) * 3);

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            const aiVector3D& position = mesh->mVertices[i];
            const aiVector3D normal = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0.0f, 1.0f, 0.0f);
            const aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0.0f, 0.0f, 0.0f);

            result.Vertices.emplace_back(position.x, position.y, position.z, 1.0f);
            result.Vertices.emplace_back(normal.x, normal.y, normal.z, 0.0f);
            result.Vertices.emplace_back(uv.x, uv.y, 0.0f, 0.0f);
        }

        result.Indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
        {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j)
            {
                result.Indices.push_back(face.mIndices[j]);
            }
        }

        const aiMaterial* material = mesh->mMaterialIndex < scene->mNumMaterials ? scene->mMaterials[mesh->mMaterialIndex] : nullptr;
        result.Color = ReadDiffuseColor(material);
        return result;
    }

    void CollectNodeMeshes(const aiNode* node, const aiScene* scene, FBXImportedModel& model)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
            FBXImportedMesh mesh = ImportMesh(scene->mMeshes[node->mMeshes[i]], scene);
            if (mesh.Indices.empty())
            {
                continue;
            }
            model.TotalIndexCount += static_cast<int>(mesh.Indices.size());
            model.Meshes.push_back(std::move(mesh));
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i)
        {
            CollectNodeMeshes(node->mChildren[i], scene, model);
        }
    }

    std::shared_ptr<const FBXImportedModel> ImportModel(const std::string& filePath)
    {
        Assimp::Importer importer;
        // PreTransformVertices bakes every node transform into the vertices and normals, so the
        // node hierarchy of the file is preserved in the geometry.
        const aiScene* scene = importer.ReadFile(filePath,
            aiProcess_Triangulate |
            aiProcess_ConvertToLeftHanded |
            aiProcess_GenNormals |
            aiProcess_PreTransformVertices |
            aiProcess_OptimizeMeshes);

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        {
            std::cerr << "Assimp error (" << filePath << "): " << importer.GetErrorString() << std::endl;
            return nullptr;
        }

        std::shared_ptr<FBXImportedModel> model = std::make_shared<FBXImportedModel>();
        CollectNodeMeshes(scene->mRootNode, scene, *model);

        float maxRadiusSq = 0.0f;
        for (const FBXImportedMesh& mesh : model->Meshes)
        {
            for (size_t i = 0; i < mesh.Vertices.size(); i += 3)
            {
                const DirectX::XMFLOAT4& p = mesh.Vertices[i];
                maxRadiusSq = std::max(maxRadiusSq, p.x * p.x + p.y * p.y + p.z * p.z);
            }
        }
        model->BoundingRadius = std::sqrt(maxRadiusSq);
        return model;
    }

    std::shared_ptr<const FBXGpuModel> CreateGpuModel(ID3D11Device* device, const FBXImportedModel& model)
    {
        std::shared_ptr<FBXGpuModel> gpuModel = std::make_shared<FBXGpuModel>();
        gpuModel->Meshes.reserve(model.Meshes.size());

        for (const FBXImportedMesh& mesh : model.Meshes)
        {
            D3D11_BUFFER_DESC vertexBufDesc = {};
            vertexBufDesc.Usage = D3D11_USAGE_IMMUTABLE;
            vertexBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexBufDesc.ByteWidth = static_cast<UINT>(sizeof(DirectX::XMFLOAT4) * mesh.Vertices.size());
            D3D11_SUBRESOURCE_DATA vertexData = {};
            vertexData.pSysMem = mesh.Vertices.data();

            D3D11_BUFFER_DESC indexBufDesc = {};
            indexBufDesc.Usage = D3D11_USAGE_IMMUTABLE;
            indexBufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            indexBufDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh.Indices.size());
            D3D11_SUBRESOURCE_DATA indexData = {};
            indexData.pSysMem = mesh.Indices.data();

            FBXGpuMesh gpuMesh;
            gpuMesh.Color = mesh.Color;
            gpuMesh.IndexCount = static_cast<UINT>(mesh.Indices.size());
            if (FAILED(device->CreateBuffer(&vertexBufDesc, &vertexData, gpuMesh.VertexBuffer.GetAddressOf())) ||
                FAILED(device->CreateBuffer(&indexBufDesc, &indexData, gpuMesh.IndexBuffer.GetAddressOf())))
            {
                // Nothing partial is ever published to the cache.
                return nullptr;
            }
            gpuModel->Meshes.push_back(std::move(gpuMesh));
        }

        return gpuModel;
    }
}

FBXComponent::FBXComponent() : GameComponent()
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
    Color = glm::vec4(1.0f); // untextured sub-meshes show their material colour
}

FBXComponent::FBXComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
    : GameComponent(pos, rot, scale)
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
    Color = glm::vec4(1.0f);
}

FBXComponent::FBXComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color)
    : GameComponent(pos, rot, scale, color)
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
}

FBXComponent::~FBXComponent() = default;

bool FBXComponent::LoadModel(const std::string& filePath)
{
    const std::string cacheKey = MakeAssetKey(filePath);

    std::shared_ptr<const FBXImportedModel> model = importedModelCache[cacheKey].lock();
    if (model)
    {
        std::cout << "Model cache hit: " << filePath << " (" << model->Meshes.size() << " meshes)" << std::endl;
    }
    else
    {
        model = ImportModel(filePath);
        if (!model)
        {
            return false;
        }
        importedModelCache[cacheKey] = model;
        std::cout << "Model loaded: " << filePath << " (" << model->Meshes.size() << " meshes)" << std::endl;
    }

    importedModel = model;
    gpuModel.reset();
    modelPath = cacheKey;
    totalIndexCount = model->TotalIndexCount;
    baseBoundingRadius = model->BoundingRadius;

    if (GamePtr && GamePtr->GetDevice())
    {
        CreateBuffers(GamePtr->GetDevice());
    }
    return true;
}

void FBXComponent::CreateBuffers(ID3D11Device* Device)
{
    if (Device == nullptr)
    {
        return;
    }

    if (!cb)
    {
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(ConstantBufferData);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(Device->CreateBuffer(&bufferDesc, nullptr, cb.GetAddressOf())))
        {
            std::cout << "FBXComponent: failed to create constant buffer." << std::endl;
            return;
        }
    }

    if (gpuModel || !importedModel)
    {
        return;
    }

    const auto gpuKey = std::make_pair(static_cast<const ID3D11Device*>(Device), modelPath);
    gpuModel = gpuModelCache[gpuKey].lock();
    if (!gpuModel)
    {
        gpuModel = CreateGpuModel(Device, *importedModel);
        if (!gpuModel)
        {
            std::cout << "FBXComponent: failed to create GPU buffers for " << modelPath << std::endl;
            return;
        }
        gpuModelCache[gpuKey] = gpuModel;
    }
}

bool FBXComponent::LoadTexture(const std::string& texturePath)
{
    ID3D11Device* device = GamePtr ? GamePtr->GetDevice() : nullptr;
    ID3D11DeviceContext* context = GamePtr ? GamePtr->GetContext() : nullptr;
    if (device == nullptr || context == nullptr)
    {
        std::cout << "FBXComponent::LoadTexture requires SetGame with an initialized Game." << std::endl;
        return false;
    }

    const auto cacheKey = std::make_pair(static_cast<const ID3D11Device*>(device), MakeAssetKey(texturePath));
    std::shared_ptr<CachedFBXTexture> texture = textureCache[cacheKey].lock();
    if (texture)
    {
        std::cout << "Texture cache hit: " << texturePath << std::endl;
    }
    else
    {
        std::shared_ptr<CachedFBXTexture> newTexture = std::make_shared<CachedFBXTexture>();
        if (!TextureLoader::LoadTexture2D(device, context, texturePath, true, newTexture->TextureSRV) ||
            !TextureLoader::CreateLinearSampler(device, D3D11_TEXTURE_ADDRESS_WRAP, newTexture->SamplerState))
        {
            return false;
        }
        texture = newTexture;
        textureCache[cacheKey] = texture;
    }

    cachedTexture = texture;
    textureSRV = texture->TextureSRV;
    samplerState = texture->SamplerState;
    return true;
}

void FBXComponent::Tick(float deltaTime)
{
    GameComponent::Tick(deltaTime);
}

float FBXComponent::GetBoundingRadius() const
{
    const float maxScale = std::max(std::max(std::abs(ComponentScale.x), std::abs(ComponentScale.y)), std::abs(ComponentScale.z));
    return baseBoundingRadius * maxScale;
}

void FBXComponent::Render(ID3D11DeviceContext* Context)
{
    if (Context == nullptr || !gpuModel || !cb || vertexShader == nullptr || pixelShader == nullptr)
    {
        return;
    }

    Context->IASetInputLayout(GamePtr ? GamePtr->GetInputLayout(VertexFormat::Mesh) : nullptr);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->VSSetShader(vertexShader, nullptr, 0);
    Context->PSSetShader(pixelShader, nullptr, 0);
    BindMaterialResources(Context);
    Context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    Context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());

    // Object constants were prepared in the update phase; only the sub-mesh colour changes here.
    const DirectX::XMFLOAT4 objectColor = ConstantPositionBuffer.ObjectColor;
    bool bUploaded = false;
    DirectX::XMFLOAT4 uploadedColor = {};

    const UINT stride = MeshVertexStride;
    const UINT offset = 0;
    for (const FBXGpuMesh& mesh : gpuModel->Meshes)
    {
        const DirectX::XMFLOAT4 meshColor(objectColor.x * mesh.Color.x, objectColor.y * mesh.Color.y,
                                          objectColor.z * mesh.Color.z, objectColor.w * mesh.Color.w);
        if (!bUploaded || std::memcmp(&meshColor, &uploadedColor, sizeof(meshColor)) != 0)
        {
            ConstantPositionBuffer.ObjectColor = meshColor;
            Context->UpdateSubresource(cb.Get(), 0, nullptr, &ConstantPositionBuffer, 0, 0);
            uploadedColor = meshColor;
            bUploaded = true;
        }

        ID3D11Buffer* vbArray[] = {mesh.VertexBuffer.Get()};
        Context->IASetVertexBuffers(0, 1, vbArray, &stride, &offset);
        Context->IASetIndexBuffer(mesh.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        Context->DrawIndexed(mesh.IndexCount, 0, 0);
        if (GamePtr) GamePtr->CountDraw(mesh.IndexCount);
    }
    ConstantPositionBuffer.ObjectColor = objectColor;

    UnbindMaterialResources(Context);
}

void FBXComponent::RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass)
{
    if (context == nullptr || !CastsShadow() || !gpuModel ||
        shadowPass.VertexShader == nullptr || shadowPass.ConstantBuffer == nullptr || shadowPass.MeshLayout == nullptr)
    {
        return;
    }

    // The world matrix comes from this frame's update phase.
    ShadowPassBufferData shadowData = {};
    shadowData.worldMatrix = ConstantPositionBuffer.worldMatrix;
    shadowData.lightViewProjection = shadowPass.LightViewProjection;

    context->IASetInputLayout(shadowPass.MeshLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(shadowPass.VertexShader, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    context->UpdateSubresource(shadowPass.ConstantBuffer, 0, nullptr, &shadowData, 0, 0);
    context->VSSetConstantBuffers(0, 1, &shadowPass.ConstantBuffer);

    const UINT stride = MeshVertexStride;
    const UINT offset = 0;
    for (const FBXGpuMesh& mesh : gpuModel->Meshes)
    {
        ID3D11Buffer* vbArray[] = {mesh.VertexBuffer.Get()};
        context->IASetVertexBuffers(0, 1, vbArray, &stride, &offset);
        context->IASetIndexBuffer(mesh.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->DrawIndexed(mesh.IndexCount, 0, 0);
        if (GamePtr) GamePtr->CountDraw(mesh.IndexCount);
    }
}

void FBXComponent::InitPoints(DirectX::XMFLOAT4* NewPoints, int pointCount, int* NewIndices, int indexCount)
{
    GameComponent::InitPoints(NewPoints, pointCount, NewIndices, indexCount);
}
