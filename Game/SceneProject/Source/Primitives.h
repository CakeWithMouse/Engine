#pragma once
#include <Engine/Components/ComponentType.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Render/MeshPrimitives.h>

// CPU geometry only. Scene/Game creates GPU resources when a viewport is attached.
// The shapes are meshes with normals and UVs: they draw with the default material or an assigned one.
namespace SceneProject
{
class PointLight final : public PointLightComponent
{
public:
    PointLight() : PointLightComponent(glm::vec3(0), glm::vec3(1), 2.0f, 10.0f) {}
};
class Mesh : public MeshComponent
{
protected:
    Mesh(MeshLodData mesh, glm::vec4 color)
    {
        SetMesh(std::move(mesh));
        SetColor(color);
    }
    // Both faces, useful both as a floor and as a rotated wall even with one-sided materials.
    static MeshLodData TwoSided(MeshLodData mesh)
    {
        const auto vertexCount = static_cast<uint32_t>(mesh.Vertices.size());
        const auto indexCount = mesh.Indices.size();
        for (uint32_t i = 0; i < vertexCount; ++i)
        {
            MeshVertex back = mesh.Vertices[i];
            back.Normal = {-back.Normal.x, -back.Normal.y, -back.Normal.z, 0.0f};
            mesh.Vertices.push_back(back);
        }
        for (size_t i = 0; i < indexCount; i += 3)
        {
            mesh.Indices.insert(mesh.Indices.end(), {mesh.Indices[i] + vertexCount, mesh.Indices[i + 2] + vertexCount,
                                                     mesh.Indices[i + 1] + vertexCount});
        }
        return mesh;
    }
};
class Cube final : public Mesh
{
public:
    Cube() : Mesh(MeshPrimitives::Cube(), {0.65f, 0.78f, 0.95f, 1}) {}
};
class Sphere final : public Mesh
{
public:
    Sphere() : Mesh(MeshPrimitives::Sphere(0.5f, 32, 16), {0.9f, 0.65f, 0.3f, 1}) {}
};
class Plane final : public Mesh
{
public:
    Plane() : Mesh(TwoSided(MeshPrimitives::Plane()), {0.4f, 0.58f, 0.42f, 1}) {}
};
class Cylinder final : public Mesh
{
public:
    Cylinder() : Mesh(MeshPrimitives::Cylinder(), {0.45f, 0.7f, 0.85f, 1}) {}
};
class Cone final : public Mesh
{
public:
    Cone() : Mesh(MeshPrimitives::Cone(), {0.9f, 0.5f, 0.35f, 1}) {}
};
class Pyramid final : public Mesh
{
public:
    Pyramid() : Mesh(MeshPrimitives::Pyramid(), {0.75f, 0.55f, 0.85f, 1}) {}
};

inline ComponentTypeCatalog GetComponentTypes()
{
    static const ComponentType types[] = {
        {"Primitive.Cube", "Куб", EngineComponentDetail::Create<Cube>},
        {"Primitive.Sphere", "Сфера", EngineComponentDetail::Create<Sphere>},
        {"Primitive.Plane", "Плоскость", EngineComponentDetail::Create<Plane>},
        {"Primitive.Cylinder", "Цилиндр", EngineComponentDetail::Create<Cylinder>},
        {"Primitive.Cone", "Конус", EngineComponentDetail::Create<Cone>},
        {"Primitive.Pyramid", "Пирамида", EngineComponentDetail::Create<Pyramid>},
        {"Light.Point", "Точечный свет", EngineComponentDetail::Create<PointLight>}
    };
    return {types, sizeof(types) / sizeof(types[0])};
}
}
