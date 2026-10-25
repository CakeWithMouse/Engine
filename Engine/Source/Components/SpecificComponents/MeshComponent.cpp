#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Render/Material.h>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
    constexpr UINT MeshVertexStride = sizeof(MeshVertex);
}

MeshComponent::MeshComponent() : GameComponent()
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
}

MeshComponent::MeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
    : GameComponent(pos, rot, scale)
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
}

MeshComponent::MeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color)
    : GameComponent(pos, rot, scale, color)
{
    ObjectType = GameComponentNames::GeometryType::Object3D;
}

void MeshComponent::SetMesh(MeshLodData mesh)
{
    Mesh = std::move(mesh);
    indexCount = static_cast<int>(Mesh.Indices.size());

    // Culling uses a sphere around the local origin, not around the centre of the bounds.
    float maxRadiusSq = 0.0f;
    for (const MeshVertex& vertex : Mesh.Vertices)
    {
        const DirectX::XMFLOAT4& p = vertex.Position;
        maxRadiusSq = std::max(maxRadiusSq, p.x * p.x + p.y * p.y + p.z * p.z);
    }
    LocalBoundingRadius = std::sqrt(maxRadiusSq);

    bBuffersDirty = true;
    if (GamePtr && GamePtr->GetDevice())
    {
        CreateBuffers(GamePtr->GetDevice());
    }
}

void MeshComponent::CreateBuffers(ID3D11Device* Device)
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
            std::cout << "MeshComponent: failed to create constant buffer." << std::endl;
            return;
        }
    }

    if (!bBuffersDirty)
    {
        return;
    }
    vb.Reset();
    ib.Reset();
    bBuffersDirty = false;
    if (Mesh.Vertices.empty() || Mesh.Indices.empty())
    {
        return;
    }

    D3D11_BUFFER_DESC vertexDesc = {};
    vertexDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDesc.ByteWidth = static_cast<UINT>(sizeof(MeshVertex) * Mesh.Vertices.size());
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = Mesh.Vertices.data();

    D3D11_BUFFER_DESC indexDesc = {};
    indexDesc.Usage = D3D11_USAGE_DEFAULT;
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * Mesh.Indices.size());
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = Mesh.Indices.data();

    if (FAILED(Device->CreateBuffer(&vertexDesc, &vertexData, vb.GetAddressOf())) ||
        FAILED(Device->CreateBuffer(&indexDesc, &indexData, ib.GetAddressOf())))
    {
        std::cout << "MeshComponent: failed to create mesh buffers." << std::endl;
        vb.Reset();
        ib.Reset();
    }
}

float MeshComponent::GetBoundingRadius() const
{
    const float maxScale = std::max(std::max(std::abs(ComponentScale.x), std::abs(ComponentScale.y)), std::abs(ComponentScale.z));
    return LocalBoundingRadius * maxScale;
}

void MeshComponent::BindGeometry(ID3D11DeviceContext* context, ID3D11InputLayout* layout)
{
    context->IASetInputLayout(layout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* vbArray[] = {vb.Get()};
    const UINT stride = MeshVertexStride;
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, vbArray, &stride, &offset);
    context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);
}

void MeshComponent::DrawGeometry(ID3D11DeviceContext* context)
{
    if (InstanceArray.empty())
    {
        context->DrawIndexed(static_cast<UINT>(indexCount), 0, 0);
        if (GamePtr) GamePtr->CountDraw(static_cast<uint32_t>(indexCount));
        return;
    }

    // Copies made with CreateInstance: one draw, their transforms and colours from the stream at t0.
    if (!UploadInstanceData(context))
    {
        return;
    }
    ID3D11ShaderResourceView* srv = instanceSRV.Get();
    context->VSSetShaderResources(0, 1, &srv);
    const UINT instanceCount = static_cast<UINT>(InstanceArray.size());
    context->DrawIndexedInstanced(static_cast<UINT>(indexCount), instanceCount, 0, 0, 0);
    if (GamePtr) GamePtr->CountDraw(static_cast<uint32_t>(indexCount), instanceCount);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->VSSetShaderResources(0, 1, &nullSRV);
}

void MeshComponent::Render(ID3D11DeviceContext* Context)
{
    if (Context == nullptr || !vb || !ib || !cb || vertexShader == nullptr || pixelShader == nullptr || indexCount <= 0)
    {
        return;
    }

    BindGeometry(Context, GamePtr ? GamePtr->GetInputLayout(VertexFormat::Mesh) : nullptr);
    Context->VSSetShader(vertexShader, nullptr, 0);
    Context->PSSetShader(pixelShader, nullptr, 0);
    BindMaterialResources(Context);

    // With copies the owner's constants still carry the camera, lights and the base colour.
    Context->UpdateSubresource(cb.Get(), 0, nullptr, &ConstantPositionBuffer, 0, 0);
    Context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    Context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
    DrawGeometry(Context);

    UnbindMaterialResources(Context);
}

void MeshComponent::RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass)
{
    if (context == nullptr || !CastsShadow() || !vb || !ib || indexCount <= 0 ||
        shadowPass.VertexShader == nullptr || shadowPass.ConstantBuffer == nullptr || shadowPass.MeshLayout == nullptr)
    {
        return;
    }
    const bool bInstanced = !InstanceArray.empty();

    // A masked material clips its own silhouette: its shaders read the full object constants.
    // Shadows are drawn before the colour passes, so a changed material is compiled here.
    Material* material = GamePtr ? GamePtr->PrepareMaterial(this) : nullptr;
    if (material && material->HasShadowShaders() && cb)
    {
        // Not shadowPass.MeshLayout: it was built for the position-only depth shader, and drivers may
        // skip attributes absent from that signature (the surface would get zero UVs and normals).
        BindGeometry(context, GamePtr->GetInputLayout(VertexFormat::Mesh));
        context->VSSetShader(material->GetShadowVertexShader(bInstanced), nullptr, 0);
        context->PSSetShader(material->GetShadowPixelShader(bInstanced), nullptr, 0);
        context->UpdateSubresource(cb.Get(), 0, nullptr, &ConstantPositionBuffer, 0, 0);
        context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
        material->BindPassTransform(context, shadowPass.LightViewProjection);
        DrawGeometry(context);
        material->Unbind(context);
        context->PSSetShader(nullptr, nullptr, 0);
        return;
    }

    // The world matrix comes from this frame's update phase; copies bring their own.
    ShadowPassBufferData shadowData = {};
    shadowData.worldMatrix = ConstantPositionBuffer.worldMatrix;
    if (bInstanced)
    {
        if (shadowPass.InstancedVertexShader == nullptr)
        {
            return;
        }
        DirectX::XMStoreFloat4x4(&shadowData.worldMatrix, DirectX::XMMatrixIdentity());
    }
    shadowData.lightViewProjection = shadowPass.LightViewProjection;

    BindGeometry(context, shadowPass.MeshLayout);
    context->VSSetShader(bInstanced ? shadowPass.InstancedVertexShader : shadowPass.VertexShader, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    context->UpdateSubresource(shadowPass.ConstantBuffer, 0, nullptr, &shadowData, 0, 0);
    context->VSSetConstantBuffers(0, 1, &shadowPass.ConstantBuffer);
    DrawGeometry(context);
}
