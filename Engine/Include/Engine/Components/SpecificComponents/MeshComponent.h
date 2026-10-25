#pragma once

#include <Engine/Components/GameComponents.h>
#include <Engine/Render/MeshData.h>

/**
 * One mesh in VertexFormat::Mesh (position, normal, uv): procedural shapes from MeshPrimitives or
 * any MeshLodData. Draws with the component's shaders or material and casts shadows. Copies made
 * with CreateInstance are drawn in one instanced call (a material then uses its instanced set).
 * For thousands of copies with LODs and culling use InstancedMeshComponent.
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
    bool SupportsMaterials() const override { return true; }

private:
    void BindGeometry(ID3D11DeviceContext* context, ID3D11InputLayout* layout);
    /** One draw of the mesh, or of every copy with the instance stream bound. */
    void DrawGeometry(ID3D11DeviceContext* context);

    MeshLodData Mesh;
    bool bBuffersDirty = true;
};
