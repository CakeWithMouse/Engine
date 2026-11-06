#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/SkeletalAnimation.h>
#include <Engine/Render/Material.h>
#include <Engine/Render/SkeletalLoader.h>
#include <Engine/Render/ShaderCompiler.h>
#include <Engine/Render/TextureLoader.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Scene/SceneAssets.h>
#include "../../../ThirdParty/nlohmann/json.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>

// Render-only views share the owner's geometry, palette and object constants.
// They are not Scene objects: IDs, picking, hierarchy and persistence stay on the owner.
// Each view enters the ordinary material pipeline independently (including mixed blend modes).
class SkeletalMeshComponent::MaterialPart final : public GameComponent
{
public:
    MaterialPart(SkeletalMeshComponent& owner, uint32_t slot, UINT first, UINT count)
        : owner(owner), slot(slot), first(first), count(count) { SetParent(&owner); }
    bool SupportsMaterials() const override { return true; }
    bool RequiresMaterial() const override { return true; }
    bool SupportsFrustumCulling() const override { return false; }
    bool IsRenderable() const override { return owner.IsRenderable(); }
    void SynchronizeFlags() { bHasOpacity=owner.HasOpacity(); }
    bool CastsShadow() const override { return owner.CastsShadow() && GameComponent::CastsShadow(); }
    bool IsStaticShadowCaster() const override { return owner.IsStaticShadowCaster() && CastsShadow(); }
    uint32_t GetMaterialVariant() const override { return MaterialVariant::Skinned; }
    void Render(ID3D11DeviceContext* context) override {
        if (!vertexShader || !pixelShader || !owner.BindGeometry(context)) return;
        context->VSSetShader(vertexShader,nullptr,0);context->PSSetShader(pixelShader,nullptr,0);
        owner.BindMaterialResources(context);
        if (GetMaterial()) GetMaterial()->Bind(context);
        context->DrawIndexed(count,first,0); if(GamePtr) GamePtr->CountDraw(count);
        if (GetMaterial()) GetMaterial()->Unbind(context);
        owner.UnbindMaterialResources(context);
    }
    void RenderShadow(ID3D11DeviceContext* context,const ShadowPassContext& pass) override {
        auto* material=GamePtr?GamePtr->PrepareMaterial(this):nullptr;
        if(!material || !material->HasShadowShaders(MaterialVariant::Skinned) || !CastsShadow() || !owner.BindGeometry(context))return;
        context->VSSetShader(material->GetShadowVertexShader(MaterialVariant::Skinned),nullptr,0);
        context->PSSetShader(material->GetShadowPixelShader(MaterialVariant::Skinned),nullptr,0);
        ID3D11ShaderResourceView* texture=owner.textureSRV.Get();
        context->PSSetShaderResources(0,1,&texture);
        context->PSSetSamplers(0,1,owner.samplerState.GetAddressOf());
        material->BindPassTransform(context,pass.LightViewProjection);
        context->DrawIndexed(count,first,0);if(GamePtr)GamePtr->CountDraw(count);
        material->Unbind(context);texture=nullptr;context->PSSetShaderResources(0,1,&texture);
        context->PSSetShader(nullptr,nullptr,0);
    }
    SkeletalMeshComponent& owner;
    uint32_t slot;UINT first,count;
};

void SkeletalMeshComponent::AppendRenderParts(std::vector<GameComponent*>& output)
{
    if(materialParts.empty() || !GetMaterialAsset().empty() || GetMaterial()) { output.push_back(this);return; }
    for(auto& part:materialParts) {
        part->SetGame(GamePtr);
        part->SynchronizeFlags();
        const auto& path=materialSlots.at(part->slot);
        if(part->GetMaterialAsset()!=path)part->SetMaterialAsset(path);
        output.push_back(part.get());
    }
}

GameComponent* SkeletalMeshComponent::RenderOwnerOf(GameComponent* component)
{
    if(auto* part=dynamic_cast<MaterialPart*>(component))return &part->owner;
    return component;
}

bool SkeletalMeshComponent::SetMaterialSlot(uint32_t index,const std::string& path)
{
    auto found=materialSlots.find(index);
    if(found==materialSlots.end())return false;
    try { SceneAssets::ValidateMaterialPath(path); } catch(...) { return false; }
    if(found->second==path)return true;
    found->second=path;
    if(GamePtr)GamePtr->InvalidateStaticShadow();
    return true;
}

std::string SkeletalMeshComponent::SaveProjectData() const
{
    nlohmann::json slots=nlohmann::json::object();
    for(const auto& entry:materialSlots)slots[std::to_string(entry.first)]=entry.second;
    return nlohmann::json{{"version",1},{"materialSlots",slots}}.dump();
}

void SkeletalMeshComponent::LoadProjectData(const std::string& data)
{
    if(data.empty())return; // Older scenes keep their single material override.
    const auto json=nlohmann::json::parse(data);
    if(!json.is_object() || json.size()!=2 || json.at("version")!=1 || !json.at("materialSlots").is_object())
        throw std::invalid_argument("Invalid model material slots");
    auto candidate=materialSlots;
    const auto& slots=json.at("materialSlots");
    if(slots.size()!=candidate.size())throw std::invalid_argument("Material slots do not match model");
    for(auto& entry:candidate) {
        entry.second=slots.at(std::to_string(entry.first)).get<std::string>();
        SceneAssets::ValidateMaterialPath(entry.second);
    }
    materialSlots=std::move(candidate);
    if(GamePtr)GamePtr->InvalidateStaticShadow();
}

std::string SkeletalMeshComponent::DescribeEditorProperties() const
{
    nlohmann::json fields=nlohmann::json::array();
    for(const auto& slot:materialSlots) {
        std::string name;
        if(skeletalModel)for(const auto& mesh:skeletalModel->Meshes)if(mesh.MaterialIndex==slot.first){name=mesh.MaterialName;break;}
        fields.push_back({{"path","/materialSlots/"+std::to_string(slot.first)},{"type","asset"},
            {"extensions",nlohmann::json::array({".mat"})},
            {"label",std::to_string(slot.first)+": "+name},{"group","Материалы модели"},
            {"description","Материал этой части модели. Пусто — стандартный. Общий материал объекта заменяет все слоты; очистите его для отдельных назначений."}});
    }
    return nlohmann::json{{"version",1},{"fields",fields}}.dump();
}

SkeletalMeshComponent::SkeletalMeshComponent()
    : GameComponent()
{
    SetColor({1, 1, 1, 1});
}

SkeletalMeshComponent::SkeletalMeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
    : GameComponent(pos, rot, scale)
{
}

SkeletalMeshComponent::~SkeletalMeshComponent() = default;

bool SkeletalMeshComponent::LoadFromVfs(std::string_view vpath)
{

    SkeletalLoadResult loadResult = LoadSkeletalModelFromVfs(vpath);
    if (!loadResult.Success || !loadResult.Model)
    {
        std::cerr << "SkeletalMeshComponent: failed to load " << vpath << ": " << loadResult.ErrorMessage << std::endl;
        return false;
    }

    return SetAssetModel(std::string(vpath), std::move(loadResult.Model));
}

bool SkeletalMeshComponent::SetAssetModel(std::string path, std::shared_ptr<SkeletalModel> model)
{
    if (!SetModel(std::move(model))) return false;
    assetPath = std::move(path);
    return true;
}

bool SkeletalMeshComponent::SetModel(std::shared_ptr<SkeletalModel> model)
{
    if (!model || model->ModelSkeleton.Bones.size() > MaxSkeletalBones)
    {
        return false;
    }

    skeletalModel = std::move(model);
    materialParts.clear();
    auto previousSlots=std::move(materialSlots);materialSlots.clear();
    UINT firstIndex=0;
    for(const auto& mesh:skeletalModel->Meshes) {
        materialSlots.emplace(mesh.MaterialIndex,previousSlots.count(mesh.MaterialIndex)?previousSlots.at(mesh.MaterialIndex):std::string{});
        if(!mesh.Indices.empty())materialParts.push_back(std::make_unique<MaterialPart>(*this,mesh.MaterialIndex,firstIndex,static_cast<UINT>(mesh.Indices.size())));
        firstIndex+=static_cast<UINT>(mesh.Indices.size());
    }
    animationTime = 0.0f;

    const size_t boneCount = skeletalModel->ModelSkeleton.Bones.size();
    bonePalette.resize(boneCount);

    if (boneCount > 0)
    {
        EvaluateSkeletalAnimation(*skeletalModel, currentClipIndex, animationTime, bonePalette, false);
    }

    // CPU geometry for editor picking and focus, in the initial animated pose.
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t> triangles;
    skeletalModel->Flatten(vertices, triangles);
    std::vector<DirectX::XMFLOAT4> pickPoints;
    pickPoints.reserve(vertices.size() * 2);
    for (const auto& vertex : vertices)
    {
        using namespace DirectX;
        auto position = XMLoadFloat3(&vertex.Position);
        XMVECTOR skinned = XMVectorZero(); float weight = 0;
        for (int i = 0; i < 4; ++i)
            if (vertex.BoneWeights[i] > 0 && vertex.BoneIndices[i] < bonePalette.size())
            {
                skinned += XMVector3TransformCoord(position, XMLoadFloat4x4(&bonePalette[vertex.BoneIndices[i]])) * vertex.BoneWeights[i];
                weight += vertex.BoneWeights[i];
            }
        XMFLOAT4 point; XMStoreFloat4(&point, weight > 0 ? skinned : position); point.w = 1;
        pickPoints.push_back(point); pickPoints.emplace_back(1, 1, 1, 1);
    }
    std::vector<int> pickIndices(triangles.begin(), triangles.end());
    if (!pickPoints.empty() && !pickIndices.empty())
        InitPoints(pickPoints.data(), static_cast<int>(pickPoints.size()), pickIndices.data(), static_cast<int>(pickIndices.size()));

    if (GamePtr && GamePtr->GetDevice())
    {
        CreateBuffers(GamePtr->GetDevice());
        GamePtr->InvalidateStaticShadow();
    }

    return true;
}

bool SkeletalMeshComponent::LoadTexture(const std::string& path)
{
    return TrySetTexture(path);
}

void SkeletalMeshComponent::Tick(float deltaTime)
{
    GameComponent::Tick(deltaTime);

    if (!skeletalModel || skeletalModel->ModelSkeleton.Bones.empty())
    {
        return;
    }

    if (isPlaying)
    {
        animationTime += deltaTime * playbackSpeed;
    }

    // Evaluates bone palette in-place without dynamic heap reallocations
    EvaluateSkeletalAnimation(*skeletalModel, currentClipIndex, animationTime, bonePalette, false);
}

bool SkeletalMeshComponent::CompileShaders(ID3D11Device* device)
{
    if (!device) return false;
    if (vertexShader && pixelShader && inputLayout) return true;

    std::filesystem::path shaderPath = EnginePaths::Shader("SkinnedMesh.hlsl");
    std::wstring shaderWPath = shaderPath.wstring();

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    std::string vsErrors;
    HRESULT hr = ShaderCompiler::CompileFromFile(shaderWPath, nullptr, "VSMain", "vs_5_0", vsBlob.ReleaseAndGetAddressOf(), &vsErrors);
    if (FAILED(hr) || !vsBlob)
    {
        std::cerr << "SkeletalMeshComponent: failed to compile VSMain in SkinnedMesh.hlsl: " << vsErrors << std::endl;
        return false;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    std::string psErrors;
    hr = ShaderCompiler::CompileFromFile(shaderWPath, nullptr, "PSMain", "ps_5_0", psBlob.ReleaseAndGetAddressOf(), &psErrors);
    if (FAILED(hr) || !psBlob)
    {
        std::cerr << "SkeletalMeshComponent: failed to compile PSMain in SkinnedMesh.hlsl: " << psErrors << std::endl;
        return false;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vertexShader.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, pixelShader.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, TexCoord)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, static_cast<UINT>(offsetof(SkinnedVertex, BoneIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, BoneWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = device->CreateInputLayout(layoutDesc, ARRAYSIZE(layoutDesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), inputLayout.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cerr << "SkeletalMeshComponent: failed to create input layout" << std::endl;
        return false;
    }

    auto& slot = GetShaderVariant(ShaderCompileVariant::Default);
    slot.VertexShader = vertexShader.Get();
    slot.PixelShader = pixelShader.Get();
    slot.bResolved = true;
    slot.bWritesGBuffer = false;

    return true;
}

void SkeletalMeshComponent::CreateBuffers(ID3D11Device* device)
{
    if (!device) return;
    LoadPendingTexture();

    if (!cb)
    {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.ByteWidth = sizeof(ConstantBufferData);
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&cbDesc, nullptr, cb.ReleaseAndGetAddressOf())))
        {
            std::cerr << "SkeletalMeshComponent: failed to create base constant buffer" << std::endl;
            return;
        }
    }

    if (!CompileShaders(device))
    {
        return;
    }

    if (!samplerState)
    {
        TextureLoader::CreateLinearSampler(device, D3D11_TEXTURE_ADDRESS_WRAP, samplerState);
    }

    if (!skeletalModel || skeletalModel->Meshes.empty())
    {
        return;
    }

    std::vector<SkinnedVertex> allVertices;
    std::vector<uint32_t> allIndices;
    skeletalModel->Flatten(allVertices, allIndices);

    indexCount = static_cast<UINT>(allIndices.size());
    if (allVertices.empty() || allIndices.empty())
    {
        return;
    }

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(SkinnedVertex) * allVertices.size());
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = allVertices.data();
    if (FAILED(device->CreateBuffer(&vbDesc, &vbData, vertexBuffer.ReleaseAndGetAddressOf())))
    {
        std::cerr << "SkeletalMeshComponent: failed to create vertex buffer" << std::endl;
        return;
    }

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * allIndices.size());
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = allIndices.data();
    if (FAILED(device->CreateBuffer(&ibDesc, &ibData, indexBuffer.ReleaseAndGetAddressOf())))
    {
        std::cerr << "SkeletalMeshComponent: failed to create index buffer" << std::endl;
        return;
    }

    D3D11_BUFFER_DESC boneBufDesc = {};
    boneBufDesc.Usage = D3D11_USAGE_DYNAMIC;
    boneBufDesc.ByteWidth = static_cast<UINT>(sizeof(DirectX::XMFLOAT4X4) * 64);
    boneBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    boneBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&boneBufDesc, nullptr, boneBuffer.ReleaseAndGetAddressOf())))
    {
        std::cerr << "SkeletalMeshComponent: failed to create bone constant buffer" << std::endl;
        return;
    }
}

uint32_t SkeletalMeshComponent::GetMaterialVariant() const
{
    return MaterialVariant::Skinned;
}

bool SkeletalMeshComponent::BindGeometry(ID3D11DeviceContext* context)
{
    if (!context || !vertexBuffer || !indexBuffer || !boneBuffer || !vertexShader || !pixelShader || !inputLayout || !cb)
    {
        return false;
    }

    DirectX::XMFLOAT4X4 gpuPalette[64];
    const size_t boneCount = std::min(bonePalette.size(), static_cast<size_t>(64));
    for (size_t i = 0; i < boneCount; ++i)
    {
        DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&bonePalette[i]);
        DirectX::XMStoreFloat4x4(&gpuPalette[i], DirectX::XMMatrixTranspose(m));
    }
    for (size_t i = boneCount; i < 64; ++i)
    {
        DirectX::XMStoreFloat4x4(&gpuPalette[i], DirectX::XMMatrixIdentity());
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(boneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return false;
    std::memcpy(mapped.pData, gpuPalette, sizeof(gpuPalette));
    context->Unmap(boneBuffer.Get(), 0);

    DirectX::XMFLOAT4X4 compWorld4x4 = GetWorldMatrix();
    DirectX::XMMATRIX compWorld = DirectX::XMLoadFloat4x4(&compWorld4x4);
    DirectX::XMMATRIX yawMat = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(modelYawDegrees));
    DirectX::XMMATRIX drawWorld = DirectX::XMMatrixMultiply(yawMat, compWorld);

    DirectX::XMStoreFloat4x4(&ConstantPositionBuffer.worldMatrix, DirectX::XMMatrixTranspose(drawWorld));
    DirectX::XMVECTOR det;
    DirectX::XMMATRIX normMat = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&det, drawWorld));
    DirectX::XMStoreFloat4x4(&ConstantPositionBuffer.normalMatrix, DirectX::XMMatrixTranspose(normMat));
    ConstantPositionBuffer.HasTexture = (textureSRV != nullptr) ? 1.0f : 0.0f;

    context->UpdateSubresource(cb.Get(), 0, nullptr, &ConstantPositionBuffer, 0, 0);

    context->IASetInputLayout(inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const UINT stride = sizeof(SkinnedVertex);
    const UINT offset = 0;
    ID3D11Buffer* vbs[] = { vertexBuffer.Get() };
    context->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    // Bones in b3, shared with the skinned set of materials (Shaders/Skinning.hlsli).
    context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    context->VSSetConstantBuffers(3, 1, boneBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
    return true;
}

void SkeletalMeshComponent::Render(ID3D11DeviceContext* context)
{
    // The assigned material, or the engine default, supplies the current forward/G-buffer set.
    if (!GameComponent::vertexShader || !GameComponent::pixelShader || !BindGeometry(context)) return;
    context->VSSetShader(GameComponent::vertexShader, nullptr, 0);
    context->PSSetShader(GameComponent::pixelShader, nullptr, 0);
    BindMaterialResources(context);
    context->DrawIndexed(indexCount, 0, 0);
    if (GamePtr) GamePtr->CountDraw(indexCount);
    UnbindMaterialResources(context);
}

void SkeletalMeshComponent::RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass)
{
    Material* material = GamePtr ? GamePtr->PrepareMaterial(this) : nullptr;
    const auto variant = GetMaterialVariant();
    if (!material || !material->HasShadowShaders(variant) || !CastsShadow() || !BindGeometry(context)) return;
    context->VSSetShader(material->GetShadowVertexShader(variant), nullptr, 0);
    context->PSSetShader(material->GetShadowPixelShader(variant), nullptr, 0);
    // A masked surface also sees the component texture. Do not bind the shadow SRV
    // here: the same resource is currently the depth output.
    ID3D11ShaderResourceView* texture = textureSRV.Get();
    context->PSSetShaderResources(0, 1, &texture);
    context->PSSetSamplers(0, 1, samplerState.GetAddressOf());
    material->BindPassTransform(context, shadowPass.LightViewProjection);
    context->DrawIndexed(indexCount, 0, 0);
    if (GamePtr) GamePtr->CountDraw(indexCount);
    material->Unbind(context);
    texture = nullptr;
    context->PSSetShaderResources(0, 1, &texture);
    context->PSSetShader(nullptr, nullptr, 0);
}

DirectX::XMFLOAT4X4 SkeletalMeshComponent::GetTraceMatrix() {
        auto world=GetWorldMatrix();DirectX::XMFLOAT4X4 result;
        DirectX::XMStoreFloat4x4(&result,DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(modelYawDegrees))*DirectX::XMLoadFloat4x4(&world));return result;
    }
