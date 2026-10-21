#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <Engine/Render/ShaderCompiler.h>
#include <Engine/Render/SkeletalBaker.h>
#include <Engine/Render/TextureLoader.h>
#include <Engine/Resources/Paths.h>
#include <d3dcompiler.h>

namespace
{
    constexpr UINT MeshVertexStride = sizeof(MeshVertex);

    /** Layout of cbuffer ImpostorBuffer (b1) in Shaders/Impostor.hlsl. */
    struct ImpostorConstants
    {
        DirectX::XMFLOAT4 AtlasParams;
        DirectX::XMFLOAT4 ModelParams;
    };

    struct GpuClipInfo
    {
        uint32_t StartRow = 0;
        uint32_t FrameCount = 0;
        float Duration = 0.0f;
        float Fps = 30.0f;
    };

    struct GpuAnimClipBufferData
    {
        GpuClipInfo Clips[16] = {};
        uint32_t ClipCount = 0;
        float Padding[3] = {};
    };

    const glm::vec4 LodDebugColors[InstancedMeshComponent::MaxLodCount] =
    {
        glm::vec4(0.25f, 1.0f, 0.35f, 1.0f),
        glm::vec4(1.0f, 0.85f, 0.2f, 1.0f),
        glm::vec4(1.0f, 0.4f, 0.2f, 1.0f),
        glm::vec4(0.6f, 0.4f, 1.0f, 1.0f)
    };

    /**
     * World matrix of one instance, already transposed for HLSL.
     * Translation, yaw and uniform scale only; this keeps per-instance CPU work small and lets
     * the shader transform normals with the same matrix.
     */
    DirectX::XMFLOAT4X4 MakeInstanceMatrix(const glm::vec3& position, float yaw, float scale)
    {
        const float c = std::cos(yaw) * scale;
        const float s = std::sin(yaw) * scale;
        return DirectX::XMFLOAT4X4(
            c, 0.0f, s, position.x,
            0.0f, scale, 0.0f, position.y,
            -s, 0.0f, c, position.z,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }
}

InstancedMeshComponent::InstancedMeshComponent()
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
    Color = glm::vec4(1.0f);
}

InstancedMeshComponent::InstancedMeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
    : GameComponent(pos, rot, scale)
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
    Color = glm::vec4(1.0f);
}

bool InstancedMeshComponent::SetLod(int level, MeshLodData data)
{
    if (level < 0 || level >= MaxLodCount || data.Vertices.empty() || data.Indices.empty())
    {
        return false;
    }
    if (level > LodCount)
    {
        std::cout << "InstancedMeshComponent: LOD levels must be filled in order, got " << level
                  << " while " << LodCount << " are set." << std::endl;
        return false;
    }

    Lods[level].Data = std::move(data);
    Lods[level].VertexBuffer.Reset();
    Lods[level].IndexBuffer.Reset();
    Lods[level].IndexCount = static_cast<UINT>(Lods[level].Data.Indices.size());
    LodCount = std::max(LodCount, level + 1);

    if (level == 0)
    {
        // Bounds and index count come from the most detailed level.
        float maxRadiusSq = 0.0f;
        for (const MeshVertex& vertex : Lods[0].Data.Vertices)
        {
            const float radiusSq = vertex.Position.x * vertex.Position.x +
                                   vertex.Position.y * vertex.Position.y +
                                   vertex.Position.z * vertex.Position.z;
            maxRadiusSq = std::max(maxRadiusSq, radiusSq);
        }
        MeshRadius = std::sqrt(maxRadiusSq);
        TotalIndexCount = static_cast<int>(Lods[0].Data.Indices.size());
    }

    if (GamePtr && GamePtr->GetDevice())
    {
        CreateLodBuffers(GamePtr->GetDevice(), Lods[level]);
    }
    return true;
}

bool InstancedMeshComponent::SetSkinnedLod(int level, const std::vector<CrowdSkinnedVertex>& vertices, const std::vector<uint32_t>& indices)
{
    if (level < 0 || level >= MaxLodCount || vertices.empty() || indices.empty())
    {
        return false;
    }
    if (level > LodCount)
    {
        std::cout << "InstancedMeshComponent: LOD levels must be filled in order, got " << level
                  << " while " << LodCount << " are set." << std::endl;
        return false;
    }

    bIsSkinned = true;
    bIsCrowdVertex = true;

    Lods[level].Data.Vertices.clear();
    Lods[level].CrowdVertices = vertices;
    Lods[level].SkinnedVertices.clear();
    Lods[level].Data.Indices = indices;
    Lods[level].VertexBuffer.Reset();
    Lods[level].IndexBuffer.Reset();
    Lods[level].IndexCount = static_cast<UINT>(indices.size());
    Lods[level].VertexStride = sizeof(CrowdSkinnedVertex);
    LodCount = std::max(LodCount, level + 1);

    if (level == 0)
    {
        float maxRadiusSq = 0.0f;
        for (const CrowdSkinnedVertex& vertex : vertices)
        {
            const float radiusSq = vertex.Position.x * vertex.Position.x +
                                   vertex.Position.y * vertex.Position.y +
                                   vertex.Position.z * vertex.Position.z;
            maxRadiusSq = std::max(maxRadiusSq, radiusSq);
        }
        MeshRadius = std::sqrt(maxRadiusSq);
        TotalIndexCount = static_cast<int>(indices.size());
    }

    if (GamePtr && GamePtr->GetDevice())
    {
        CreateLodBuffers(GamePtr->GetDevice(), Lods[level]);
    }
    return true;
}

bool InstancedMeshComponent::SetSkinnedLod(int level, const std::vector<SkinnedVertex>& vertices, const std::vector<uint32_t>& indices)
{
    if (level < 0 || level >= MaxLodCount || vertices.empty() || indices.empty())
    {
        return false;
    }
    if (level > LodCount)
    {
        std::cout << "InstancedMeshComponent: LOD levels must be filled in order, got " << level
                  << " while " << LodCount << " are set." << std::endl;
        return false;
    }

    bIsSkinned = true;
    bIsCrowdVertex = false;

    Lods[level].Data.Vertices.clear();
    Lods[level].CrowdVertices.clear();
    Lods[level].SkinnedVertices = vertices;
    Lods[level].Data.Indices = indices;
    Lods[level].VertexBuffer.Reset();
    Lods[level].IndexBuffer.Reset();
    Lods[level].IndexCount = static_cast<UINT>(indices.size());
    Lods[level].VertexStride = sizeof(SkinnedVertex);
    LodCount = std::max(LodCount, level + 1);

    if (level == 0)
    {
        float maxRadiusSq = 0.0f;
        for (const SkinnedVertex& vertex : vertices)
        {
            const float radiusSq = vertex.Position.x * vertex.Position.x +
                                   vertex.Position.y * vertex.Position.y +
                                   vertex.Position.z * vertex.Position.z;
            maxRadiusSq = std::max(maxRadiusSq, radiusSq);
        }
        MeshRadius = std::sqrt(maxRadiusSq);
        TotalIndexCount = static_cast<int>(indices.size());
    }

    if (GamePtr && GamePtr->GetDevice())
    {
        CreateLodBuffers(GamePtr->GetDevice(), Lods[level]);
    }
    return true;
}

void InstancedMeshComponent::SetAnimationTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv, const std::vector<BakedClipInfo>& clips)
{
    AnimationTextureSRV = srv;
    AnimationClips = clips;
    if (GamePtr && GamePtr->GetDevice() && AnimationTextureSRV)
    {
        UpdateAnimClipConstantBuffer(GamePtr->GetDevice());
        EnsureSkinnedShaders(GamePtr->GetDevice());
    }
}

void InstancedMeshComponent::SetAnimationTexture(ID3D11ShaderResourceView* srv, const std::vector<BakedClipInfo>& clips)
{
    SetAnimationTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>(srv), clips);
}

bool InstancedMeshComponent::SetAnimatedModel(ID3D11Device* device, const SkeletalModel& model, float sampleFps)
{
    SkeletalBakeResult bakeResult = BakeSkeletalModel(model, sampleFps);
    if (!bakeResult.Success)
    {
        std::cerr << "InstancedMeshComponent: failed to bake skeletal model: " << bakeResult.ErrorMessage << std::endl;
        return false;
    }

    if (device != nullptr)
    {
        if (!CreateBakedTextureResource(device, bakeResult.Texture, AnimationTexture, AnimationTextureSRV))
        {
            std::cerr << "InstancedMeshComponent: failed to create GPU texture resource" << std::endl;
            return false;
        }
    }

    SetAnimationTexture(AnimationTextureSRV, bakeResult.Texture.Clips);

    std::vector<CrowdSkinnedVertex> crowdVerts;
    std::vector<uint32_t> indices;
    model.FlattenCrowd(crowdVerts, indices);

    return SetSkinnedLod(0, crowdVerts, indices);
}

void InstancedMeshComponent::SetLodDistances(const std::vector<float>& distances)
{
    LodDistances = distances;
    std::sort(LodDistances.begin(), LodDistances.end());
}

void InstancedMeshComponent::SetLodScreenHeights(const std::vector<float>& pixels)
{
    LodScreenHeights = pixels;
    // Descending: the first threshold is the largest on-screen size.
    std::sort(LodScreenHeights.begin(), LodScreenHeights.end(), std::greater<float>());
}

void InstancedMeshComponent::SetImpostorEnabled(bool enabled, const ImpostorAtlasSettings& settings)
{
    bImpostorEnabled = enabled;
    ImpostorSettings = settings;
    bImpostorBuildFailed = false;
    if (enabled && GamePtr && GamePtr->GetDevice())
    {
        BuildImpostorAtlas();
    }
}

int InstancedMeshComponent::GetImpostorLevelIndex() const
{
    return HasImpostor() ? LodCount : -1;
}

int InstancedMeshComponent::GetLevelCount() const
{
    return LodCount + (HasImpostor() ? 1 : 0);
}

void InstancedMeshComponent::BuildImpostorAtlas()
{
    if (!bImpostorEnabled || bImpostorBuildFailed || Impostor.IsValid() || LodCount == 0 || GamePtr == nullptr)
    {
        return;
    }

    ID3D11Device* device = GamePtr->GetDevice();
    if (device == nullptr)
    {
        return;
    }

    // The most detailed level is the source of the sprite, so designers author only that model.
    if (!Impostor.Build(*GamePtr, Lods[0].Data, textureSRV.Get(), Color, ImpostorSettings))
    {
        std::cout << "InstancedMeshComponent: impostor atlas was not built, sprite level disabled." << std::endl;
        bImpostorBuildFailed = true;
        return;
    }

    const ImpostorAtlasSettings& settings = Impostor.GetSettings();
    const float minPitch = DirectX::XMConvertToRadians(settings.MinPitchDegrees);
    const float maxPitch = DirectX::XMConvertToRadians(settings.MaxPitchDegrees);
    const float pitchStep = settings.PitchCount > 1 ? (maxPitch - minPitch) / (settings.PitchCount - 1) : 1.0f;

    ImpostorConstants constants = {};
    constants.AtlasParams = DirectX::XMFLOAT4(static_cast<float>(settings.YawCount),
                                              static_cast<float>(settings.PitchCount),
                                              minPitch, pitchStep);
    constants.ModelParams = DirectX::XMFLOAT4(Impostor.GetBounds().Radius,
                                              Impostor.GetBounds().Center.y,
                                              Impostor.GetTileInset(), 0.0f);

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(ImpostorConstants);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA initial = {};
    initial.pSysMem = &constants;
    if (FAILED(device->CreateBuffer(&bufferDesc, &initial, ImpostorConstantBuffer.ReleaseAndGetAddressOf())) ||
        !TextureLoader::CreateLinearSampler(device, D3D11_TEXTURE_ADDRESS_CLAMP, ImpostorSampler))
    {
        std::cout << "InstancedMeshComponent: failed to create impostor resources." << std::endl;
        bImpostorBuildFailed = true;
    }
}

float InstancedMeshComponent::ComputeScreenFactor() const
{
    if (GamePtr == nullptr || GamePtr->GetDisplay() == nullptr || GamePtr->GetPlayer() == nullptr)
    {
        return 0.0f;
    }
    const float windowHeight = static_cast<float>(GamePtr->GetDisplay()->GetHeight());
    const float tanHalfFov = std::tan(GamePtr->GetPlayer()->GetFovY() * 0.5f);
    return tanHalfFov > 0.0001f ? windowHeight / tanHalfFov : 0.0f;
}

size_t InstancedMeshComponent::AddInstance(const MeshInstance& instance)
{
    Instances.push_back(instance);
    PreparedFrame = UINT64_MAX;
    bStaticShadowDataDirty = true;
    return Instances.size() - 1;
}

void InstancedMeshComponent::SetInstancePosition(size_t index, const glm::vec3& position)
{
    Instances[index].Position = position;
    PreparedFrame = UINT64_MAX;
    bStaticShadowDataDirty = true;
}

void InstancedMeshComponent::SetInstanceYaw(size_t index, float yaw)
{
    Instances[index].Yaw = yaw;
    PreparedFrame = UINT64_MAX;
    bStaticShadowDataDirty = true;
}

void InstancedMeshComponent::SetInstanceColor(size_t index, const glm::vec4& color)
{
    Instances[index].Color = color;
    PreparedFrame = UINT64_MAX;
    bStaticShadowDataDirty = true;
}

void InstancedMeshComponent::SetInstanceVisible(size_t index, bool visible)
{
    Instances[index].bVisible = visible;
    PreparedFrame = UINT64_MAX;
    bStaticShadowDataDirty = true;
}

void InstancedMeshComponent::SetInstanceAnimationPhase(size_t index, float phase)
{
    Instances[index].AnimationPhase = phase;
    PreparedFrame = UINT64_MAX;
}

void InstancedMeshComponent::SetInstancePositionAndPhase(size_t index, const glm::vec3& position, float phase)
{
    Instances[index].Position = position;
    Instances[index].AnimationPhase = phase;
    PreparedFrame = UINT64_MAX;
    bStaticShadowDataDirty = true;
}

void InstancedMeshComponent::ClearInstances()
{
    Instances.clear();
    PreparedFrame = UINT64_MAX;
    bStaticShadowDataDirty = true;
}

uint32_t InstancedMeshComponent::GetDrawnInstancesForLod(int level) const
{
    if (level < 0 || level >= LodCount)
    {
        return 0;
    }
    return static_cast<uint32_t>(Lods[level].ColorStaging.size());
}

void InstancedMeshComponent::UpdateAnimClipConstantBuffer(ID3D11Device* device)
{
    if (device == nullptr) return;

    GpuAnimClipBufferData bufferData = {};
    const size_t count = std::min<size_t>(AnimationClips.size(), 16);
    bufferData.ClipCount = static_cast<uint32_t>(count);
    for (size_t i = 0; i < count; ++i)
    {
        bufferData.Clips[i].StartRow = AnimationClips[i].StartRow;
        bufferData.Clips[i].FrameCount = AnimationClips[i].FrameCount;
        bufferData.Clips[i].Duration = AnimationClips[i].Duration;
        bufferData.Clips[i].Fps = AnimationClips[i].Fps;
    }

    if (!AnimClipConstantBuffer)
    {
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = sizeof(GpuAnimClipBufferData);
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = &bufferData;
        if (FAILED(device->CreateBuffer(&desc, &initData, AnimClipConstantBuffer.ReleaseAndGetAddressOf())))
        {
            std::cerr << "InstancedMeshComponent: failed to create animation clip constant buffer" << std::endl;
        }
    }
    else
    {
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        device->GetImmediateContext(context.GetAddressOf());
        if (context)
        {
            context->UpdateSubresource(AnimClipConstantBuffer.Get(), 0, nullptr, &bufferData, 0, 0);
        }
    }
}

bool InstancedMeshComponent::EnsureSkinnedShaders(ID3D11Device* device)
{
    if (device == nullptr) return false;
    if (SkinnedVertexShader && SkinnedPixelShader && SkinnedInputLayout &&
        SkinnedShadowVertexShader && SkinnedShadowInputLayout)
    {
        return true;
    }

    const std::filesystem::path shaderPath = EnginePaths::Shader("InstancedSkinnedMesh.hlsl");
    const std::wstring shaderWPath = shaderPath.wstring();

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    std::string vsErrors;
    const D3D_SHADER_MACRO vsDefines[] = {
        {"ANIM_LERP", "1"},
        {nullptr, nullptr}
    };
    HRESULT hr = ShaderCompiler::CompileFromFile(
        shaderWPath,
        bAnimationLerp ? vsDefines : nullptr,
        "VSMain", "vs_5_0", vsBlob.ReleaseAndGetAddressOf(), &vsErrors);
    if (FAILED(hr) || !vsBlob)
    {
        std::cerr << "InstancedMeshComponent: failed to compile skinned VSMain: " << vsErrors << std::endl;
        return false;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr,
                                    SkinnedVertexShader.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    if (bIsCrowdVertex)
    {
        D3D11_INPUT_ELEMENT_DESC crowdLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(CrowdSkinnedVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, static_cast<UINT>(offsetof(CrowdSkinnedVertex, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT, 0, static_cast<UINT>(offsetof(CrowdSkinnedVertex, TexCoord)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, static_cast<UINT>(offsetof(CrowdSkinnedVertex, BoneIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDWEIGHT", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, static_cast<UINT>(offsetof(CrowdSkinnedVertex, BoneWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        hr = device->CreateInputLayout(crowdLayout, 5, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                       SkinnedInputLayout.ReleaseAndGetAddressOf());
    }
    else
    {
        D3D11_INPUT_ELEMENT_DESC standardLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, TexCoord)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, static_cast<UINT>(offsetof(SkinnedVertex, BoneIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, BoneWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        hr = device->CreateInputLayout(standardLayout, 5, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                       SkinnedInputLayout.ReleaseAndGetAddressOf());
    }
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    std::string psErrors;
    const bool bDeferred = GamePtr && (GamePtr->GetRenderingType() == RenderType::Deffered);
    const D3D_SHADER_MACRO psDefines[] = {
        {"DEFERRED_GBUFFER", "1"},
        {nullptr, nullptr}
    };
    hr = ShaderCompiler::CompileFromFile(
        shaderWPath,
        bDeferred ? psDefines : nullptr,
        "PSMain", "ps_5_0", psBlob.ReleaseAndGetAddressOf(), &psErrors);
    if (FAILED(hr) || !psBlob)
    {
        std::cerr << "InstancedMeshComponent: failed to compile skinned PSMain: " << psErrors << std::endl;
        return false;
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr,
                                   SkinnedPixelShader.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<ID3DBlob> shadowBlob;
    std::string shadowErrors;
    const D3D_SHADER_MACRO shadowDefines[] = {
        {"SHADOW_PASS", "1"},
        {"ANIM_LERP", bAnimationLerp ? "1" : "0"},
        {nullptr, nullptr}
    };
    hr = ShaderCompiler::CompileFromFile(
        shaderWPath,
        shadowDefines,
        "VSShadowMain", "vs_5_0", shadowBlob.ReleaseAndGetAddressOf(), &shadowErrors);
    if (FAILED(hr) || !shadowBlob)
    {
        std::cerr << "InstancedMeshComponent: failed to compile skinned VSShadowMain: " << shadowErrors << std::endl;
        return false;
    }

    hr = device->CreateVertexShader(shadowBlob->GetBufferPointer(), shadowBlob->GetBufferSize(), nullptr,
                                    SkinnedShadowVertexShader.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    if (bIsCrowdVertex)
    {
        D3D11_INPUT_ELEMENT_DESC crowdLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(CrowdSkinnedVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, static_cast<UINT>(offsetof(CrowdSkinnedVertex, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT, 0, static_cast<UINT>(offsetof(CrowdSkinnedVertex, TexCoord)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, static_cast<UINT>(offsetof(CrowdSkinnedVertex, BoneIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDWEIGHT", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, static_cast<UINT>(offsetof(CrowdSkinnedVertex, BoneWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        hr = device->CreateInputLayout(crowdLayout, 5, shadowBlob->GetBufferPointer(), shadowBlob->GetBufferSize(),
                                       SkinnedShadowInputLayout.ReleaseAndGetAddressOf());
    }
    else
    {
        D3D11_INPUT_ELEMENT_DESC standardLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, TexCoord)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, static_cast<UINT>(offsetof(SkinnedVertex, BoneIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(SkinnedVertex, BoneWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        hr = device->CreateInputLayout(standardLayout, 5, shadowBlob->GetBufferPointer(), shadowBlob->GetBufferSize(),
                                       SkinnedShadowInputLayout.ReleaseAndGetAddressOf());
    }
    return SUCCEEDED(hr);
}

bool InstancedMeshComponent::CreateLodBuffers(ID3D11Device* device, LodLevel& lod)
{
    if (device == nullptr)
    {
        return false;
    }
    if (lod.VertexBuffer && lod.IndexBuffer)
    {
        return true;
    }

    if (bIsSkinned)
    {
        if (lod.Data.Indices.empty()) return false;
        const void* vertexDataPtr = nullptr;
        UINT vertexDataBytes = 0;
        if (bIsCrowdVertex)
        {
            if (lod.CrowdVertices.empty()) return false;
            lod.VertexStride = sizeof(CrowdSkinnedVertex);
            vertexDataPtr = lod.CrowdVertices.data();
            vertexDataBytes = static_cast<UINT>(sizeof(CrowdSkinnedVertex) * lod.CrowdVertices.size());
        }
        else
        {
            if (lod.SkinnedVertices.empty()) return false;
            lod.VertexStride = sizeof(SkinnedVertex);
            vertexDataPtr = lod.SkinnedVertices.data();
            vertexDataBytes = static_cast<UINT>(sizeof(SkinnedVertex) * lod.SkinnedVertices.size());
        }

        D3D11_BUFFER_DESC vertexDesc = {};
        vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertexDesc.ByteWidth = vertexDataBytes;
        D3D11_SUBRESOURCE_DATA vertexData = {};
        vertexData.pSysMem = vertexDataPtr;

        D3D11_BUFFER_DESC indexDesc = {};
        indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        indexDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * lod.Data.Indices.size());
        D3D11_SUBRESOURCE_DATA indexData = {};
        indexData.pSysMem = lod.Data.Indices.data();

        if (FAILED(device->CreateBuffer(&vertexDesc, &vertexData, lod.VertexBuffer.ReleaseAndGetAddressOf())) ||
            FAILED(device->CreateBuffer(&indexDesc, &indexData, lod.IndexBuffer.ReleaseAndGetAddressOf())))
        {
            std::cout << "InstancedMeshComponent: failed to create skinned LOD buffers." << std::endl;
            lod.VertexBuffer.Reset();
            lod.IndexBuffer.Reset();
            return false;
        }
        return true;
    }

    if (lod.Data.Vertices.empty() || lod.Data.Indices.empty())
    {
        return false;
    }

    lod.VertexStride = sizeof(MeshVertex);

    D3D11_BUFFER_DESC vertexDesc = {};
    vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDesc.ByteWidth = static_cast<UINT>(sizeof(MeshVertex) * lod.Data.Vertices.size());
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = lod.Data.Vertices.data();

    D3D11_BUFFER_DESC indexDesc = {};
    indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * lod.Data.Indices.size());
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = lod.Data.Indices.data();

    if (FAILED(device->CreateBuffer(&vertexDesc, &vertexData, lod.VertexBuffer.ReleaseAndGetAddressOf())) ||
        FAILED(device->CreateBuffer(&indexDesc, &indexData, lod.IndexBuffer.ReleaseAndGetAddressOf())))
    {
        std::cout << "InstancedMeshComponent: failed to create LOD buffers." << std::endl;
        lod.VertexBuffer.Reset();
        lod.IndexBuffer.Reset();
        return false;
    }
    return true;
}

void InstancedMeshComponent::CreateBuffers(ID3D11Device* Device)
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
            std::cout << "InstancedMeshComponent: failed to create constant buffer." << std::endl;
            return;
        }
    }

    for (int level = 0; level < LodCount; ++level)
    {
        CreateLodBuffers(Device, Lods[level]);
    }

    BuildImpostorAtlas();

    if (AnimationTextureSRV)
    {
        UpdateAnimClipConstantBuffer(Device);
        EnsureSkinnedShaders(Device);
    }
}

int InstancedMeshComponent::SelectLod(float distance, float screenHeightPixels) const
{
    int level = 0;
    if (!LodScreenHeights.empty())
    {
        // Screen size drives the switch, so the same setting works for every model and window.
        while (level < static_cast<int>(LodScreenHeights.size()) && screenHeightPixels < LodScreenHeights[level])
        {
            ++level;
        }
    }
    else
    {
        while (level < static_cast<int>(LodDistances.size()) && distance >= LodDistances[level])
        {
            ++level;
        }
    }
    // The sprite level is the last one, so the clamp must include it.
    return std::min(level, GetLevelCount() - 1);
}

void InstancedMeshComponent::BuildVisibleSets()
{
    for (int level = 0; level < LodCount; ++level)
    {
        Lods[level].ColorStaging.clear();
        for (int cascade = 0; cascade < MaxShadowCascades; ++cascade)
        {
            Lods[level].ShadowStaging[cascade].clear();
        }
    }
    ImpostorLevel.ColorStaging.clear();
    DrawnInstances = 0;
    CulledInstances = 0;

    const int levelCount = GetLevelCount();
    const int impostorLevelIndex = GetImpostorLevelIndex();

    if (LodCount == 0 || GamePtr == nullptr)
    {
        return;
    }

    const FrameConstants& frame = GamePtr->GetFrameConstants();
    const glm::vec3 cameraPosition(frame.CameraPosition.x, frame.CameraPosition.y, frame.CameraPosition.z);
    const float drawDistanceSq = DrawDistance > 0.0f ? DrawDistance * DrawDistance : 0.0f;
    const float shadowDistanceSq = ShadowDistance > 0.0f ? ShadowDistance * ShadowDistance : 0.0f;
    const bool bCastsShadow = CastsShadow() && GamePtr->AreShadowsActive();
    const float screenFactor = ComputeScreenFactor();
    const DirectX::XMFLOAT4* cascadeSpheres = GamePtr->GetCascadeSpheres();
    const int cascadeLimit = ShadowCascadeLimit > 0 ? std::min(ShadowCascadeLimit, MaxShadowCascades) : MaxShadowCascades;
    const DirectX::XMFLOAT4& lightDirectionValue = GamePtr->GetDirectionalLightDirection();
    glm::vec3 lightDirection(lightDirectionValue.x, lightDirectionValue.y, lightDirectionValue.z);
    const float lightLength = glm::length(lightDirection);
    lightDirection = lightLength > 0.0001f ? lightDirection / lightLength : glm::vec3(0.0f, -1.0f, 0.0f);
    const bool bAnimated = (AnimationTextureSRV != nullptr);

    for (const MeshInstance& instance : Instances)
    {
        if (!instance.bVisible)
        {
            continue;
        }

        const glm::vec3 toCamera = instance.Position - cameraPosition;
        const float distanceSq = glm::dot(toCamera, toCamera);
        if (drawDistanceSq > 0.0f && distanceSq > drawDistanceSq)
        {
            ++CulledInstances;
            continue;
        }

        const float radius = MeshRadius * instance.Scale;
        bool bInsideFrustum = true;
        for (const DirectX::XMFLOAT4& plane : frame.FrustumPlanes)
        {
            if (plane.x * instance.Position.x + plane.y * instance.Position.y +
                plane.z * instance.Position.z + plane.w < -radius)
            {
                bInsideFrustum = false;
                break;
            }
        }

        const bool bWantsShadow = bCastsShadow && (shadowDistanceSq <= 0.0f || distanceSq <= shadowDistanceSq);
        if (!bInsideFrustum && !bWantsShadow)
        {
            ++CulledInstances;
            continue;
        }

        const float distance = std::sqrt(distanceSq);
        // Height of this instance on screen, in pixels of window height.
        const float screenHeight = distance > 0.001f ? radius * screenFactor / distance : screenFactor;

        int level = SelectLod(distance, screenHeight);
        int fadePartner = -1;
        float levelFade = 1.0f;
        float partnerFade = 0.0f;

        // Inside a transition band both levels are drawn and the dither in the shader keeps one
        // of them per pixel, so the switch is not a visible jump.
        if (LodFadeFraction > 0.0f && !LodScreenHeights.empty())
        {
            const int thresholdCount = std::min<int>(static_cast<int>(LodScreenHeights.size()), levelCount - 1);
            for (int index = 0; index < thresholdCount; ++index)
            {
                const float threshold = LodScreenHeights[index];
                const float upper = threshold * (1.0f + LodFadeFraction);
                const float lower = threshold * (1.0f - LodFadeFraction);
                if (screenHeight <= upper && screenHeight >= lower && upper > lower)
                {
                    const float coarseWeight = (upper - screenHeight) / (upper - lower);
                    level = index;
                    levelFade = 1.0f - coarseWeight;
                    fadePartner = std::min(index + 1, levelCount - 1);
                    partnerFade = coarseWeight;
                    break;
                }
            }
        }

        InstData data;
        data.World = MakeInstanceMatrix(instance.Position, instance.Yaw, instance.Scale);

        auto pushColor = [&](int targetLevel, float fade)
        {
            const glm::vec4 color = bLodDebugColors ? LodDebugColors[std::min(targetLevel, MaxLodCount - 1)] : instance.Color;
            data.ObjectColor = DirectX::XMFLOAT4(color.x, color.y, color.z, color.w);
            if (bAnimated)
            {
                data.Params = DirectX::XMFLOAT4(fade, static_cast<float>(instance.ClipId), instance.AnimationPhase, instance.Speed);
            }
            else
            {
                data.Params = DirectX::XMFLOAT4(fade, 0.0f, 0.0f, 0.0f);
            }
            LodLevel& bucket = targetLevel == impostorLevelIndex ? ImpostorLevel : Lods[targetLevel];
            bucket.ColorStaging.push_back(data);
        };

        if (bInsideFrustum)
        {
            pushColor(level, levelFade);
            if (fadePartner >= 0 && fadePartner != level)
            {
                pushColor(fadePartner, partnerFade);
            }
            ++DrawnInstances;
        }
        else
        {
            ++CulledInstances;
        }

        if (bWantsShadow)
        {
            // Off-screen units still cast shadows, so shadow sets are not camera-culled.
            // Sprites never cast: the depth pass always uses a mesh instead.
            data.ObjectColor = DirectX::XMFLOAT4(instance.Color.x, instance.Color.y, instance.Color.z, instance.Color.w);
            if (bAnimated)
            {
                data.Params = DirectX::XMFLOAT4(1.0f, static_cast<float>(instance.ClipId), instance.AnimationPhase, instance.Speed);
            }
            else
            {
                data.Params = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
            }
            const int meshLevel = std::min(level, LodCount - 1);

            for (int cascade = 0; cascade < cascadeLimit; ++cascade)
            {
                const DirectX::XMFLOAT4& sphere = cascadeSpheres[cascade];
                if (sphere.w <= 0.0f)
                {
                    continue;
                }

                // A caster matters for this cascade if it is inside the sphere the cascade covers
                // or above it on the way from the light.
                const glm::vec3 toInstance = instance.Position - glm::vec3(sphere.x, sphere.y, sphere.z);
                const float along = std::max(0.0f, std::min(glm::dot(toInstance, -lightDirection), ShadowCasterExtrusion));
                const glm::vec3 closest = -lightDirection * along;
                const glm::vec3 offset = toInstance - closest;
                const float reach = sphere.w + radius;
                if (glm::dot(offset, offset) > reach * reach)
                {
                    continue;
                }

                // Farther cascades cover more world per texel, so they can use coarser geometry.
                const int shadowLevel = std::min(meshLevel + ShadowLodBias + cascade, LodCount - 1);
                Lods[shadowLevel].ShadowStaging[cascade].push_back(data);
            }
        }
    }

    GamePtr->CountCulledInstances(CulledInstances);
}

bool InstancedMeshComponent::UploadStream(ID3D11DeviceContext* context,
                                          const std::vector<InstData>& staging,
                                          Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer,
                                          Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
                                          size_t& capacity)
{
    if (staging.empty())
    {
        return false;
    }

    if (!buffer || capacity < staging.size())
    {
        size_t newCapacity = std::max<size_t>(256, capacity);
        while (newCapacity < staging.size())
        {
            newCapacity *= 2;
        }

        Microsoft::WRL::ComPtr<ID3D11Device> device;
        context->GetDevice(device.GetAddressOf());

        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.ByteWidth = static_cast<UINT>(sizeof(InstData) * newCapacity);
        bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bufferDesc.StructureByteStride = sizeof(InstData);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.NumElements = static_cast<UINT>(newCapacity);

        srv.Reset();
        buffer.Reset();
        capacity = 0;
        if (FAILED(device->CreateBuffer(&bufferDesc, nullptr, buffer.GetAddressOf())) ||
            FAILED(device->CreateShaderResourceView(buffer.Get(), &srvDesc, srv.GetAddressOf())))
        {
            std::cout << "InstancedMeshComponent: failed to create instance buffer for "
                      << staging.size() << " instances." << std::endl;
            srv.Reset();
            buffer.Reset();
            return false;
        }
        capacity = newCapacity;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return false;
    }
    std::memcpy(mapped.pData, staging.data(), sizeof(InstData) * staging.size());
    context->Unmap(buffer.Get(), 0);
    return true;
}

void InstancedMeshComponent::EnsureFrameData(ID3D11DeviceContext* context)
{
    const uint64_t frameIndex = GamePtr ? GamePtr->GetFrameIndex() : 0;
    if (PreparedFrame == frameIndex)
    {
        return;
    }

    // Visibility, LOD selection and both instance streams are prepared once per frame and
    // reused by the colour pass and by every shadow cascade.
    BuildVisibleSets();
    for (int level = 0; level < LodCount; ++level)
    {
        LodLevel& lod = Lods[level];
        UploadStream(context, lod.ColorStaging, lod.ColorBuffer, lod.ColorSRV, lod.ColorCapacity);
        for (int cascade = 0; cascade < MaxShadowCascades; ++cascade)
        {
            UploadStream(context, lod.ShadowStaging[cascade], lod.ShadowBuffer[cascade],
                         lod.ShadowSRV[cascade], lod.ShadowCapacity[cascade]);
        }
    }
    UploadStream(context, ImpostorLevel.ColorStaging, ImpostorLevel.ColorBuffer,
                 ImpostorLevel.ColorSRV, ImpostorLevel.ColorCapacity);
    PreparedFrame = frameIndex;
}

void InstancedMeshComponent::Render(ID3D11DeviceContext* context)
{
    if (context == nullptr || LodCount == 0 || !cb)
    {
        return;
    }

    EnsureFrameData(context);

    ID3D11Device* device = nullptr;
    context->GetDevice(&device);

    const bool bUseSkinned = (AnimationTextureSRV != nullptr) && EnsureSkinnedShaders(device);

    if (bUseSkinned)
    {
        context->IASetInputLayout(SkinnedInputLayout.Get());
        context->VSSetShader(SkinnedVertexShader.Get(), nullptr, 0);
        context->PSSetShader(SkinnedPixelShader.Get(), nullptr, 0);

        ID3D11ShaderResourceView* animSRV = AnimationTextureSRV.Get();
        context->VSSetShaderResources(1, 1, &animSRV);
        if (AnimClipConstantBuffer)
        {
            ID3D11Buffer* clipCB = AnimClipConstantBuffer.Get();
            context->VSSetConstantBuffers(1, 1, &clipCB);
        }
    }
    else
    {
        if (vertexShader == nullptr || pixelShader == nullptr)
        {
            return;
        }
        context->IASetInputLayout(GamePtr ? GamePtr->GetInputLayout(VertexFormat::Mesh) : nullptr);
        context->VSSetShader(vertexShader, nullptr, 0);
        context->PSSetShader(pixelShader, nullptr, 0);
    }

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    BindMaterialResources(context);
    context->UpdateSubresource(cb.Get(), 0, nullptr, &ConstantPositionBuffer, 0, 0);
    context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());

    const UINT offset = 0;
    for (int level = 0; level < LodCount; ++level)
    {
        LodLevel& lod = Lods[level];
        const UINT instanceCount = static_cast<UINT>(lod.ColorStaging.size());
        if (instanceCount == 0 || !lod.VertexBuffer || !lod.IndexBuffer || !lod.ColorSRV)
        {
            continue;
        }

        ID3D11Buffer* vertexBuffers[] = {lod.VertexBuffer.Get()};
        ID3D11ShaderResourceView* instanceSRV = lod.ColorSRV.Get();
        const UINT stride = lod.VertexStride;
        context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
        context->IASetIndexBuffer(lod.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->VSSetShaderResources(0, 1, &instanceSRV);
        context->DrawIndexedInstanced(lod.IndexCount, instanceCount, 0, 0, 0);
        if (GamePtr) GamePtr->CountDraw(lod.IndexCount, instanceCount);
    }

    DrawImpostors(context);

    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->VSSetShaderResources(0, 2, nullSRVs);
    UnbindMaterialResources(context);
}

void InstancedMeshComponent::DrawImpostors(ID3D11DeviceContext* context)
{
    const UINT instanceCount = static_cast<UINT>(ImpostorLevel.ColorStaging.size());
    if (!HasImpostor() || instanceCount == 0 || !ImpostorLevel.ColorSRV || !ImpostorConstantBuffer || GamePtr == nullptr)
    {
        return;
    }

    // The sprite level has its own material, resolved for the pass that is currently drawing.
    const std::string impostorShader = EnginePaths::Shader("Impostor.hlsl").generic_u8string();
    const ShaderCompileVariant variant = GamePtr->GetCurrentPassVariant();
    ID3D11VertexShader* impostorVS = GamePtr->GetVertexShader(impostorShader, variant);
    ID3D11PixelShader* impostorPS = GamePtr->GetPixelShader(impostorShader, variant);
    if (impostorVS == nullptr || impostorPS == nullptr)
    {
        return;
    }

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(impostorVS, nullptr, 0);
    context->PSSetShader(impostorPS, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
    context->VSSetConstantBuffers(1, 1, ImpostorConstantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(1, 1, ImpostorConstantBuffer.GetAddressOf());

    ID3D11ShaderResourceView* instanceSRV = ImpostorLevel.ColorSRV.Get();
    context->VSSetShaderResources(0, 1, &instanceSRV);
    ID3D11ShaderResourceView* atlas[2] = {Impostor.GetAlbedoSRV(), Impostor.GetNormalSRV()};
    context->PSSetShaderResources(0, 2, atlas);
    context->PSSetSamplers(0, 1, ImpostorSampler.GetAddressOf());

    context->DrawInstanced(4, instanceCount, 0, 0);
    GamePtr->CountDraw(4, instanceCount);

    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->PSSetShaderResources(0, 2, nullSRVs);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

bool InstancedMeshComponent::EnsureStaticShadowData(ID3D11DeviceContext* context)
{
    if (!bStaticShadowDataDirty && StaticShadowSRV && StaticShadowStaging.size() == Instances.size())
    {
        return !StaticShadowStaging.empty();
    }

    // Baked casters are not culled by the camera and keep one detail level for the whole map.
    StaticShadowLevel = std::min(std::max(ShadowLodBias, 0), LodCount - 1);
    StaticShadowStaging.clear();
    StaticShadowStaging.reserve(Instances.size());
    for (const MeshInstance& instance : Instances)
    {
        if (!instance.bVisible)
        {
            continue;
        }
        InstData data;
        data.World = MakeInstanceMatrix(instance.Position, instance.Yaw, instance.Scale);
        data.ObjectColor = DirectX::XMFLOAT4(instance.Color.x, instance.Color.y, instance.Color.z, instance.Color.w);
        data.Params = (AnimationTextureSRV != nullptr)
            ? DirectX::XMFLOAT4(1.0f, static_cast<float>(instance.ClipId), instance.AnimationPhase, instance.Speed)
            : DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
        StaticShadowStaging.push_back(data);
    }

    const bool uploaded = UploadStream(context, StaticShadowStaging, StaticShadowBuffer,
                                       StaticShadowSRV, StaticShadowCapacity);
    bStaticShadowDataDirty = false;
    return uploaded;
}

void InstancedMeshComponent::RenderStaticShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass)
{
    if (!EnsureStaticShadowData(context) || StaticShadowLevel >= LodCount)
    {
        return;
    }

    LodLevel& lod = Lods[StaticShadowLevel];
    if (!lod.VertexBuffer || !lod.IndexBuffer || !StaticShadowSRV)
    {
        return;
    }

    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    const bool bUseSkinned = (AnimationTextureSRV != nullptr) && EnsureSkinnedShaders(device);

    ShadowPassBufferData shadowData = {};
    DirectX::XMStoreFloat4x4(&shadowData.worldMatrix, DirectX::XMMatrixIdentity());
    shadowData.lightViewProjection = shadowPass.LightViewProjection;

    if (bUseSkinned)
    {
        context->IASetInputLayout(SkinnedShadowInputLayout.Get());
        context->VSSetShader(SkinnedShadowVertexShader.Get(), nullptr, 0);
        ID3D11ShaderResourceView* animSRV = AnimationTextureSRV.Get();
        context->VSSetShaderResources(1, 1, &animSRV);
        if (AnimClipConstantBuffer)
        {
            ID3D11Buffer* clipCB = AnimClipConstantBuffer.Get();
            context->VSSetConstantBuffers(1, 1, &clipCB);
        }
    }
    else
    {
        context->IASetInputLayout(shadowPass.MeshLayout);
        context->VSSetShader(shadowPass.InstancedVertexShader, nullptr, 0);
    }

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->PSSetShader(nullptr, nullptr, 0);
    context->UpdateSubresource(shadowPass.ConstantBuffer, 0, nullptr, &shadowData, 0, 0);
    context->VSSetConstantBuffers(0, 1, &shadowPass.ConstantBuffer);

    ID3D11Buffer* vertexBuffers[] = {lod.VertexBuffer.Get()};
    ID3D11ShaderResourceView* instanceSRV = StaticShadowSRV.Get();
    const UINT stride = lod.VertexStride;
    const UINT offset = 0;
    const UINT instanceCount = static_cast<UINT>(StaticShadowStaging.size());
    context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
    context->IASetIndexBuffer(lod.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShaderResources(0, 1, &instanceSRV);
    context->DrawIndexedInstanced(lod.IndexCount, instanceCount, 0, 0, 0);
    if (GamePtr) GamePtr->CountDraw(lod.IndexCount, instanceCount);

    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->VSSetShaderResources(0, 2, nullSRVs);
}

void InstancedMeshComponent::RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass)
{
    if (context == nullptr || LodCount == 0 || !CastsShadow() ||
        shadowPass.ConstantBuffer == nullptr)
    {
        return;
    }

    if (shadowPass.CascadeIndex < 0)
    {
        RenderStaticShadow(context, shadowPass);
        return;
    }

    EnsureFrameData(context);

    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    const bool bUseSkinned = (AnimationTextureSRV != nullptr) && EnsureSkinnedShaders(device);

    if (!bUseSkinned && (shadowPass.InstancedVertexShader == nullptr || shadowPass.MeshLayout == nullptr))
    {
        return;
    }

    ShadowPassBufferData shadowData = {};
    DirectX::XMStoreFloat4x4(&shadowData.worldMatrix, DirectX::XMMatrixIdentity());
    shadowData.lightViewProjection = shadowPass.LightViewProjection;

    if (bUseSkinned)
    {
        context->IASetInputLayout(SkinnedShadowInputLayout.Get());
        context->VSSetShader(SkinnedShadowVertexShader.Get(), nullptr, 0);
        ID3D11ShaderResourceView* animSRV = AnimationTextureSRV.Get();
        context->VSSetShaderResources(1, 1, &animSRV);
        if (AnimClipConstantBuffer)
        {
            ID3D11Buffer* clipCB = AnimClipConstantBuffer.Get();
            context->VSSetConstantBuffers(1, 1, &clipCB);
        }
    }
    else
    {
        context->IASetInputLayout(shadowPass.MeshLayout);
        context->VSSetShader(shadowPass.InstancedVertexShader, nullptr, 0);
    }

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->PSSetShader(nullptr, nullptr, 0);
    context->UpdateSubresource(shadowPass.ConstantBuffer, 0, nullptr, &shadowData, 0, 0);
    context->VSSetConstantBuffers(0, 1, &shadowPass.ConstantBuffer);

    const int cascade = std::max(0, std::min(shadowPass.CascadeIndex, MaxShadowCascades - 1));
    const UINT offset = 0;
    for (int level = 0; level < LodCount; ++level)
    {
        LodLevel& lod = Lods[level];
        // Only the casters this cascade needs, at the detail chosen for it.
        const UINT instanceCount = static_cast<UINT>(lod.ShadowStaging[cascade].size());
        if (instanceCount == 0 || !lod.VertexBuffer || !lod.IndexBuffer || !lod.ShadowSRV[cascade])
        {
            continue;
        }

        ID3D11Buffer* vertexBuffers[] = {lod.VertexBuffer.Get()};
        ID3D11ShaderResourceView* instanceSRV = lod.ShadowSRV[cascade].Get();
        const UINT stride = lod.VertexStride;
        context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
        context->IASetIndexBuffer(lod.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->VSSetShaderResources(0, 1, &instanceSRV);
        context->DrawIndexedInstanced(lod.IndexCount, instanceCount, 0, 0, 0);
        if (GamePtr) GamePtr->CountDraw(lod.IndexCount, instanceCount);
    }

    ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
    context->VSSetShaderResources(0, 2, nullSRVs);
}
