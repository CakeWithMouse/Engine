#pragma once

#include <Engine/Components/GameComponents.h>
#include <Engine/Render/MeshData.h>

/**
 * One mesh in VertexFormat::Mesh (position, normal, uv): procedural shapes from MeshPrimitives or
 * any MeshLodData. Draws with the component's shaders and casts shadows; instancing is not supported.
 */
class MeshComponent : public GameComponent
{
public:
    MeshComponent();
    MeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    MeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);

    /** Replaces the geometry. GPU buffers are created now if the component has a device, else on registration. */
    void SetMesh(MeshLodData mesh);
    const MeshLodData& GetMesh() const { return Mesh; }

    void CreateBuffers(ID3D11Device* Device) override;
    void Render(ID3D11DeviceContext* Context) override;
    void RenderShadow(ID3D11DeviceContext* context, const ShadowPassContext& shadowPass) override;
    float GetBoundingRadius() const override;

private:
    void BindGeometry(ID3D11DeviceContext* context, ID3D11InputLayout* layout);
    void DrawGeometry(ID3D11DeviceContext* context);

    MeshLodData Mesh;
    bool bBuffersDirty = true;
};
