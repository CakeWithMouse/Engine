#pragma once
#include <Engine/Components/ComponentType.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/Light/DirectionalLightComponent.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include <Engine/Components/Light/CubeMap.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Components/Character.h>

// CPU geometry only. Scene/Game creates GPU resources when a viewport is attached.
namespace SceneProject
{
class Sparks final : public ParticleSystemComponent
{
public:
    Sparks() {
        SetParticleCount(512); SetBaseLifetime(1.2f); SetSpreadRadius(0.1f);
        SetRadialSpeed(1.3f); SetBaseVelocity({0,3,0}); SetAcceleration({0,-3,0});
        SetMaxDistance(6); SetBaseSize(0.055f); SetBrightness(2);
        SetColor({1,0.42f,0.06f,0.9f}); SetSimulationRate(1); SetDepthCollision(false);
    }
};
class Smoke final : public ParticleSystemComponent
{
public:
    Smoke() {
        SetParticleCount(256); SetBaseLifetime(3.5f); SetSpreadRadius(0.25f);
        SetRadialSpeed(0.15f); SetBaseVelocity({0,0.8f,0}); SetAcceleration({0.08f,0.12f,0});
        SetMaxDistance(6); SetBaseSize(0.35f); SetBrightness(0.7f);
        SetColor({0.65f,0.68f,0.72f,0.18f}); SetSimulationRate(1); SetDepthCollision(false);
    }
};
class Sky final : public SkyboxComponent
{
public:
    Sky() { InitCube(); const auto shader=EnginePaths::Shader("BasicSky.hlsl").u8string(); SetShaderNames(shader,shader); }
    bool CastsShadow() const override { return false; }
};
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
        {"Light.Point", "Точечный свет", EngineComponentDetail::Create<PointLight>},
        {"Light.Directional", "Направленный свет", EngineComponentDetail::Create<DirectionalLightComponent>},
        {"Particle.Sparks", "Искры", EngineComponentDetail::Create<Sparks>},
        {"Particle.Smoke", "Дым", EngineComponentDetail::Create<Smoke>},
        {"Environment.Sky", "Базовое небо", EngineComponentDetail::Create<Sky>},
        {"Gameplay.Character", "Персонаж", EngineComponentDetail::Create<Character>},
        {"Gameplay.Camera", "Камера Play", EngineComponentDetail::Create<SceneCamera>},
        {"Gameplay.PlaySettings", "Настройки Play", EngineComponentDetail::Create<PlaySettings>}
    };
    return {types, sizeof(types) / sizeof(types[0])};
}
}
