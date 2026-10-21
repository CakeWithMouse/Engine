#include "InstancedMeshInternal.h"

using namespace InstancedMeshDetail;

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
    AnimationTextureSRV = std::move(srv);
    AnimationClips = clips;
    if (GamePtr && GamePtr->GetDevice())
    {
        UpdateAnimClipConstantBuffer(GamePtr->GetDevice());
    }
}

void InstancedMeshComponent::SetAnimationTexture(ID3D11ShaderResourceView* srv, const std::vector<BakedClipInfo>& clips)
{
    SetAnimationTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>(srv), clips);
}

bool InstancedMeshComponent::SetAnimatedModel(ID3D11Device* device, const SkeletalModel& model, float sampleFps)
{
    if (device == nullptr)
    {
        std::cerr << "InstancedMeshComponent::SetAnimatedModel: device is null\n";
        return false;
    }

    const SkeletalBakeResult bakeResult = BakeSkeletalModel(model, sampleFps);
    if (!bakeResult.Success)
    {
        std::cerr << "InstancedMeshComponent::SetAnimatedModel: animation bake failed: " << bakeResult.ErrorMessage << "\n";
        return false;
    }

    std::vector<CrowdSkinnedVertex> crowdVertices;
    std::vector<uint32_t> crowdIndices;
    model.FlattenCrowd(crowdVertices, crowdIndices);

    if (!SetSkinnedLod(0, crowdVertices, crowdIndices))
    {
        std::cerr << "InstancedMeshComponent::SetAnimatedModel: SetSkinnedLod failed\n";
        return false;
    }

    // Generate simplified LOD levels for crowd instances
    std::vector<CrowdSkinnedVertex> lod1Vertices;
    std::vector<uint32_t> lod1Indices;
    if (GenerateCrowdSkinnedLod(crowdVertices, crowdIndices, 0.45f, lod1Vertices, lod1Indices))
    {
        SetSkinnedLod(1, lod1Vertices, lod1Indices);
    }

    std::vector<CrowdSkinnedVertex> lod2Vertices;
    std::vector<uint32_t> lod2Indices;
    if (GenerateCrowdSkinnedLod(crowdVertices, crowdIndices, 0.18f, lod2Vertices, lod2Indices))
    {
        SetSkinnedLod(2, lod2Vertices, lod2Indices);
    }


    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    if (!CreateBakedTextureResource(device, bakeResult.Texture, AnimationTexture, srv))
    {
        std::cerr << "InstancedMeshComponent::SetAnimatedModel: CreateBakedTextureResource failed\n";
        return false;
    }

    SetAnimationTexture(srv, bakeResult.Texture.Clips);
    return true;
}

bool InstancedMeshComponent::CreateSkinnedLodBuffers(ID3D11Device* device, LodLevel& lod)
{
    if (device == nullptr || lod.Data.Indices.empty())
    {
        return false;
    }

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
