#include "../../../Public/Components/SpecificComponents/FBXComponent.h"
#include <iostream>
#include <d3dcompiler.h>
#include <stb_image.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <mutex>
#include <unordered_map>

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
    std::unordered_map<std::string, std::weak_ptr<CachedFBXModel>> modelCache;
    std::unordered_map<std::string, std::weak_ptr<CachedFBXTexture>> textureCache;
    std::mutex cacheMutex;

    std::string NormalizeCachePath(std::string path)
    {
        std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c)
        {
            if (c == '\\')
            {
                return '/';
            }
            return static_cast<char>(std::tolower(c));
        });
        return path;
    }
}

FBXComponent::FBXComponent() : GameComponent()
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
}

FBXComponent::FBXComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) 
    : GameComponent(pos, rot, scale)
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
}

FBXComponent::FBXComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color) 
    : GameComponent(pos, rot, scale, color)
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
}

FBXComponent::~FBXComponent()
{
    for (auto& mesh : renderMeshes)
    {
        if (mesh.VertexBuffer) mesh.VertexBuffer->Release();
        if (mesh.IndexBuffer) mesh.IndexBuffer->Release();
    }
    if (textureSRV) textureSRV->Release();
    if (samplerState) samplerState->Release();
    if (inputLayout) inputLayout->Release();
}


bool FBXComponent::LoadModel(const std::string& filePath)
{
    for (auto& mesh : renderMeshes)
    {
        if (mesh.VertexBuffer)
        {
            mesh.VertexBuffer->Release();
            mesh.VertexBuffer = nullptr;
        }
        if (mesh.IndexBuffer)
        {
            mesh.IndexBuffer->Release();
            mesh.IndexBuffer = nullptr;
        }
    }
    renderMeshes.clear();
    totalIndexCount = 0;
    baseBoundingRadius = 0.0f;

    const std::string cacheKey = NormalizeCachePath(filePath);
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto found = modelCache.find(cacheKey);
        if (found != modelCache.end())
        {
            cachedModel = found->second.lock();
            if (cachedModel)
            {
                renderMeshes.reserve(cachedModel->Meshes.size());
                for (const CachedFBXMesh& cachedMesh : cachedModel->Meshes)
                {
                    RenderMesh mesh;
                    mesh.Color = cachedMesh.Color;
                    mesh.IndexCount = cachedMesh.IndexCount;
                    mesh.VertexCount = cachedMesh.VertexCount;
                    mesh.VertexBuffer = cachedMesh.VertexBuffer.Get();
                    mesh.IndexBuffer = cachedMesh.IndexBuffer.Get();
                    if (mesh.VertexBuffer) mesh.VertexBuffer->AddRef();
                    if (mesh.IndexBuffer) mesh.IndexBuffer->AddRef();
                    renderMeshes.push_back(mesh);
                }
                totalIndexCount = cachedModel->TotalIndexCount;
                baseBoundingRadius = cachedModel->BaseBoundingRadius;
                modelPath = filePath;
                std::cout << "Model cache hit: " << filePath << " (" << renderMeshes.size() << " meshes)" << std::endl;
                return true;
            }
        }
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath, 
        aiProcess_Triangulate | 
        aiProcess_ConvertToLeftHanded |
        aiProcess_GenNormals |
        aiProcess_OptimizeMeshes);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cerr << "Assimp error: " << importer.GetErrorString() << std::endl;
        return false;
    }
    
    modelPath = filePath;
    ProcessNode(scene->mRootNode, scene);

    float maxRadiusSq = 0.0f;
    for (const auto& mesh : renderMeshes)
    {
        for (size_t i = 0; i < mesh.vertices.size(); i += 3)
        {
            const DirectX::XMFLOAT4& position = mesh.vertices[i];
            const float radiusSq = position.x * position.x + position.y * position.y + position.z * position.z;
            maxRadiusSq = std::max(maxRadiusSq, radiusSq);
        }
    }
    baseBoundingRadius = std::sqrt(maxRadiusSq);

    CreateMeshBuffers();

    std::shared_ptr<CachedFBXModel> newCachedModel = std::make_shared<CachedFBXModel>();
    newCachedModel->TotalIndexCount = totalIndexCount;
    newCachedModel->BaseBoundingRadius = baseBoundingRadius;
    newCachedModel->Meshes.reserve(renderMeshes.size());
    for (const RenderMesh& mesh : renderMeshes)
    {
        CachedFBXMesh cachedMesh;
        cachedMesh.Color = mesh.Color;
        cachedMesh.IndexCount = mesh.IndexCount;
        cachedMesh.VertexCount = mesh.VertexCount;
        cachedMesh.VertexBuffer = mesh.VertexBuffer;
        cachedMesh.IndexBuffer = mesh.IndexBuffer;
        newCachedModel->Meshes.push_back(std::move(cachedMesh));
    }
    cachedModel = newCachedModel;
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        modelCache[cacheKey] = newCachedModel;
    }
    
    std::cout << "Model loaded (cached): " << renderMeshes.size() << " meshes" << std::endl;
    return true;
}

void FBXComponent::CreateInputLayout()
{
    ID3D11Device* device = nullptr;
    if (GamePtr && GamePtr->GetContext())
    {
        GamePtr->GetContext()->GetDevice(&device);
    }
    
    if (!device) return;
    
    // HLSL код вершинного шейдера (должен совпадать с вашим BaseFBX.hlsl)
    const char* vertexShaderCode = R"(
        struct VS_IN
        {
            float4 pos : POSITION;
            float4 normal : NORMAL;
            float4 texCoord : TEXCOORD;
        };
        
        struct PS_IN
        {
            float4 pos : SV_POSITION;
            float3 normal : NORMAL;
            float2 texCoord : TEXCOORD;
        };
        
        cbuffer ConstantBuffer : register(b0)
        {
            float4x4 worldMatrix;
            float4x4 viewMatrix;
            float4x4 projectionMatrix;
            float4 ObjectColor;
        };
        
        PS_IN VSMain(VS_IN input)
        {
            PS_IN output = (PS_IN)0;
            
            float4 worldPos = mul(input.pos, worldMatrix);
            float4 viewPos = mul(worldPos, viewMatrix);
            float4 projectionPos = mul(viewPos, projectionMatrix);
            
            output.pos = projectionPos;
            output.normal = mul(input.normal.xyz, (float3x3)worldMatrix);
            output.texCoord = input.texCoord.xy;
            
            return output;
        }
    )";
    
    // Компилируем шейдер чтобы получить blob для создания InputLayout
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    
    HRESULT hr = D3DCompile(vertexShaderCode, strlen(vertexShaderCode), nullptr, nullptr, nullptr,
                             "VSMain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        std::cout << "Failed to compile vertex shader for InputLayout" << std::endl;
        return;
    }
    
    // Описание Input Layout для 48-байтной вершины (3 x XMFLOAT4)
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        // POSITION: XMFLOAT4, смещение 0, 16 байт
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        
        // NORMAL: XMFLOAT4, смещение 16, 16 байт (читаем все 4 компонента)
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        
        // TEXCOORD: читаем только первые 2 компонента из XMFLOAT4, смещение 32
        // Используем R32G32_FLOAT, потому что шейдер ожидает float2
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    
    // Создаем InputLayout
    hr = device->CreateInputLayout(layoutDesc, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    
    if (FAILED(hr))
    {
        std::cout << "Failed to create InputLayout for FBXComponent" << std::endl;
    }
    else
    {
        std::cout << "FBXComponent InputLayout created successfully" << std::endl;
    }
    
    vsBlob->Release();
    device->Release();
}

void FBXComponent::ProcessNode(aiNode* node, const aiScene* scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        MeshData meshData = ProcessMesh(mesh, scene);
        
        RenderMesh renderMesh;
        renderMesh.Color = meshData.Color;
        
        for (const auto& vertex : meshData.Vertices)
        {
            renderMesh.vertices.push_back(DirectX::XMFLOAT4(vertex.Position.x, vertex.Position.y, vertex.Position.z, 1.0f));
            renderMesh.vertices.push_back(DirectX::XMFLOAT4(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z, 1.0f));
            renderMesh.vertices.push_back(DirectX::XMFLOAT4(vertex.TexCoord.x, vertex.TexCoord.y, 0.0f, 1.0f));
        }
        
        renderMesh.indices = meshData.Indices;
        renderMesh.IndexCount = meshData.Indices.size();
        renderMesh.VertexCount = meshData.Vertices.size();
        
        totalIndexCount += renderMesh.IndexCount;
        renderMeshes.push_back(renderMesh);
    }
    
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(node->mChildren[i], scene);
    }
}

MeshData FBXComponent::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
    MeshData meshData;
    
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        VertexData vertex;
        vertex.Position = DirectX::XMFLOAT3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        
        if (mesh->HasNormals())
        {
            vertex.Normal = DirectX::XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        }
        else
        {
            vertex.Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
        }
        
        if (mesh->HasTextureCoords(0))
        {
            vertex.TexCoord = DirectX::XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }
        else
        {
            vertex.TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
        }
        
        meshData.Vertices.push_back(vertex);
    }
    
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
            meshData.Indices.push_back(face.mIndices[j]);
        }
    }
    
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        meshData.Color = ProcessMaterial(material);
    }
    else
    {
        meshData.Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    return meshData;
}

void FBXComponent::CreateBuffers(Microsoft::WRL::ComPtr<ID3D11Device> Device)
{
    if (!vertexShader || !pixelShader || !inputLayout)
    {
        GameComponent::CreateBuffers(Device);
    }

    if (!cb)
    {
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(ConstantBufferData);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = 0;
        Device->CreateBuffer(&bufferDesc, nullptr, &cb);
    }

    bool hasMissingBuffers = false;
    for (const auto& mesh : renderMeshes)
    {
        if (!mesh.VertexBuffer || !mesh.IndexBuffer)
        {
            hasMissingBuffers = true;
            break;
        }
    }

    if (hasMissingBuffers)
    {
        CreateMeshBuffers();
    }
}

DirectX::XMFLOAT4 FBXComponent::ProcessMaterial(aiMaterial* material)
{
    aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
    material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
    return DirectX::XMFLOAT4(color.r, color.g, color.b, color.a);
}

bool FBXComponent::LoadTexture(const std::string& texturePath)
{
    hasTexture = false;
    if (textureSRV)
    {
        textureSRV->Release();
        textureSRV = nullptr;
    }
    if (samplerState)
    {
        samplerState->Release();
        samplerState = nullptr;
    }

    const std::string cacheKey = NormalizeCachePath(texturePath);
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto found = textureCache.find(cacheKey);
        if (found != textureCache.end())
        {
            cachedTexture = found->second.lock();
            if (cachedTexture)
            {
                textureSRV = cachedTexture->TextureSRV.Get();
                samplerState = cachedTexture->SamplerState.Get();
                if (textureSRV) textureSRV->AddRef();
                if (samplerState) samplerState->AddRef();
                GameComponent::textureSRV = textureSRV;
                GameComponent::samplerState = samplerState;
                hasTexture = (textureSRV != nullptr && samplerState != nullptr);
                std::cout << "Texture cache hit: " << texturePath << std::endl;
                return hasTexture;
            }
        }
    }

    ID3D11Device* device = nullptr;
    if (GamePtr && GamePtr->GetContext())
    {
        GamePtr->GetContext()->GetDevice(&device);
    }
    
    if (!device) return false;
    
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* imageData = stbi_load(texturePath.c_str(), &width, &height, &channels, 4);
    
    if (!imageData)
    {
        std::cout << "Failed to load texture: " << texturePath << std::endl;
        device->Release();
        return false;
    }
    
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = imageData;
    initData.SysMemPitch = width * 4;
    
    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device->CreateTexture2D(&texDesc, &initData, &texture);
    
    if (SUCCEEDED(hr))
    {
        hr = device->CreateShaderResourceView(texture, nullptr, &textureSRV);
        texture->Release();
        
        if (SUCCEEDED(hr))
        {
            // Создаем сэмплер
            D3D11_SAMPLER_DESC sampDesc = {};
            sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
            sampDesc.MinLOD = 0;
            sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
            
            device->CreateSamplerState(&sampDesc, &samplerState);
            GameComponent::textureSRV = textureSRV;
            GameComponent::samplerState = samplerState;
            
            hasTexture = true;
            std::shared_ptr<CachedFBXTexture> newCachedTexture = std::make_shared<CachedFBXTexture>();
            newCachedTexture->TextureSRV = textureSRV;
            newCachedTexture->SamplerState = samplerState;
            cachedTexture = newCachedTexture;
            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                textureCache[cacheKey] = newCachedTexture;
            }
            std::cout << "Texture loaded: " << texturePath << " (" << width << "x" << height << ")" << std::endl;
        }
    }
    
    stbi_image_free(imageData);
    device->Release();
    
    return hasTexture;
}

void FBXComponent::CreateMeshBuffers()
{
    ID3D11Device* device = nullptr;
    if (GamePtr && GamePtr->GetContext())
    {
        GamePtr->GetContext()->GetDevice(&device);
    }
    
    if (!device) return;
    
    for (auto& mesh : renderMeshes)
    {
        if (mesh.VertexBuffer && mesh.IndexBuffer)
        {
            continue;
        }

        D3D11_BUFFER_DESC vertexBufDesc = {};
        vertexBufDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertexBufDesc.ByteWidth = sizeof(DirectX::XMFLOAT4) * mesh.vertices.size();
        
        D3D11_SUBRESOURCE_DATA vertexData = {};
        vertexData.pSysMem = mesh.vertices.data();
        
        D3D11_BUFFER_DESC indexBufDesc = {};
        indexBufDesc.Usage = D3D11_USAGE_DEFAULT;
        indexBufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        indexBufDesc.ByteWidth = sizeof(int) * mesh.indices.size();
        
        D3D11_SUBRESOURCE_DATA indexData = {};
        indexData.pSysMem = mesh.indices.data();
        device->CreateBuffer(&vertexBufDesc, &vertexData, &mesh.VertexBuffer);
        device->CreateBuffer(&indexBufDesc, &indexData, &mesh.IndexBuffer);
    }

    if (!cb)
    {
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(ConstantBufferData);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = 0;
        device->CreateBuffer(&bufferDesc, nullptr, &cb);
    }
    
    device->Release();
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
    if (renderMeshes.empty()) return;
    
    Context->VSSetShader(vertexShader, nullptr, 0);
    Context->PSSetShader(pixelShader, nullptr, 0);
    if (!inputLayout)
    {
        CreateInputLayout();
    }
    
    Context->IASetInputLayout(inputLayout);
    
    if (hasTexture && textureSRV && samplerState)
    {
        Context->PSSetShaderResources(0, 1, &textureSRV);
        Context->PSSetSamplers(0, 1, &samplerState);
    }
    if (cubeMapSRV && cubeMapSamplerState)
    {
        Context->PSSetShaderResources(1, 1, &cubeMapSRV);
        Context->PSSetSamplers(1, 1, &cubeMapSamplerState);
    }
    if (GamePtr && GamePtr->IsShadowEnabled())
    {
        ID3D11ShaderResourceView* shadowMapSRV = GamePtr->GetShadowMapSRV();
        ID3D11SamplerState* shadowMapSampler = GamePtr->GetShadowSampler();
        if (shadowMapSRV && shadowMapSampler)
        {
            Context->PSSetShaderResources(4, 1, &shadowMapSRV);
            Context->PSSetSamplers(4, 1, &shadowMapSampler);
        }
    }
    
    Update();
    Context->UpdateSubresource(cb, 0, nullptr, &ConstantPositionBuffer, 0, 0);
    Context->VSSetConstantBuffers(0, 1, &cb);
    Context->PSSetConstantBuffers(0, 1, &cb);
    
    UINT stride = sizeof(DirectX::XMFLOAT4) * 3;
    UINT offset = 0;
    
    for (const auto& mesh : renderMeshes)
    {
        if (mesh.VertexBuffer && mesh.IndexBuffer)
        {
            ID3D11Buffer* vbArray[] = {mesh.VertexBuffer};
            Context->IASetVertexBuffers(0, 1, vbArray, &stride, &offset);
            Context->IASetIndexBuffer(mesh.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
            Context->DrawIndexed(mesh.IndexCount, 0, 0);
        }
    }

    ID3D11ShaderResourceView* nullSRV = nullptr;
    Context->PSSetShaderResources(0, 1, &nullSRV);
    Context->PSSetShaderResources(1, 1, &nullSRV);
    Context->PSSetShaderResources(4, 1, &nullSRV);
}

void FBXComponent::RenderShadow(ID3D11DeviceContext* context,
                                ID3D11VertexShader* shadowVertexShader,
                                ID3D11Buffer* shadowCB,
                                ID3D11InputLayout* shadowPrimitiveLayout,
                                ID3D11InputLayout* shadowMeshLayout,
                                const DirectX::XMFLOAT4X4& lightViewProjection)
{
    (void)shadowPrimitiveLayout;

    if (context == nullptr || shadowVertexShader == nullptr || shadowCB == nullptr || renderMeshes.empty())
    {
        return;
    }

    if (bHasOpacity || bIsSkybox)
    {
        return;
    }

    Update();
    ShadowPassBufferData shadowData = {};
    shadowData.worldMatrix = ConstantPositionBuffer.worldMatrix;
    shadowData.lightViewProjection = lightViewProjection;

    context->IASetInputLayout(shadowMeshLayout ? shadowMeshLayout : inputLayout);
    context->VSSetShader(shadowVertexShader, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    context->UpdateSubresource(shadowCB, 0, nullptr, &shadowData, 0, 0);
    context->VSSetConstantBuffers(0, 1, &shadowCB);

    UINT stride = sizeof(DirectX::XMFLOAT4) * 3;
    UINT offset = 0;

    for (const auto& mesh : renderMeshes)
    {
        if (mesh.VertexBuffer && mesh.IndexBuffer)
        {
            ID3D11Buffer* vbArray[] = {mesh.VertexBuffer};
            context->IASetVertexBuffers(0, 1, vbArray, &stride, &offset);
            context->IASetIndexBuffer(mesh.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
            context->DrawIndexed(mesh.IndexCount, 0, 0);
        }
    }
}

void FBXComponent::InitPoints(DirectX::XMFLOAT4* NewPoints, int pointCount, int* NewIndices, int indexCount)
{
    GameComponent::InitPoints(NewPoints, pointCount, NewIndices, indexCount);
}

DirectX::XMFLOAT4X4 FBXComponent::ConvertMatrix(const aiMatrix4x4& matrix)
{
    DirectX::XMFLOAT4X4 result;
    result._11 = matrix.a1; result._12 = matrix.a2; result._13 = matrix.a3; result._14 = matrix.a4;
    result._21 = matrix.b1; result._22 = matrix.b2; result._23 = matrix.b3; result._24 = matrix.b4;
    result._31 = matrix.c1; result._32 = matrix.c2; result._33 = matrix.c3; result._34 = matrix.c4;
    result._41 = matrix.d1; result._42 = matrix.d2; result._43 = matrix.d3; result._44 = matrix.d4;
    return result;
}
