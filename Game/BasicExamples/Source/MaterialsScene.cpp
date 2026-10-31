#include "MaterialsScene.h"
#include "ExampleStartup.h"
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Render/Material.h>
#include <Engine/Render/MeshPrimitives.h>
#include <random>
#include <stdexcept>

namespace
{
const char* const Showcase[] = {"default", "gold", "pulse", "glass", "cutout", "hologram"};
}

void MaterialsGame::BuildScene(const std::filesystem::path& gameContent)
{
    const std::filesystem::path materials = gameContent / "Materials";
    auto load = [&](const char* file)
    {
        std::string error;
        std::shared_ptr<Material> material = LoadMaterial((materials / file).generic_u8string(), &error);
        if (!material) throw std::runtime_error(error);
        return material;
    };
    auto add = [&](const std::string& name, MeshLodData mesh, const glm::vec3& position, std::shared_ptr<Material> material,
                   const glm::vec4& color = glm::vec4(1.0f))
    {
        auto* component = new MeshComponent(position, glm::vec3(0.0f), glm::vec3(1.0f), color);
        component->SetMesh(std::move(mesh));
        component->SetMaterial(std::move(material));
        ExampleStartup::Register(*this, name, component);
    };

    SetDirectionalLight({-0.45f, -1.0f, -0.35f}, {1.0f, 0.96f, 0.9f}, 0.85f);
    SetShadowSettings(true, 150.0f);

    add("floor", MeshPrimitives::Plane(60.0f, 60.0f), {0.0f, 0.0f, 0.0f}, load("Floor.mat"));
    const float spacing = 2.4f;
    const float left = -spacing * 2.5f;
    add("default", MeshPrimitives::Sphere(0.8f), {left, 1.0f, 0.0f}, GetDefaultMaterial(), {0.85f, 0.25f, 0.3f, 1.0f});
    add("gold", MeshPrimitives::Sphere(0.8f), {left + spacing, 1.0f, 0.0f}, load("Gold.mat"));
    add("pulse", MeshPrimitives::Sphere(0.8f), {left + spacing * 2.0f, 1.0f, 0.0f}, load("Pulse.mat"));
    add("glass", MeshPrimitives::Sphere(0.8f), {left + spacing * 3.0f, 1.0f, 0.0f}, load("Glass.mat"));
    add("cutout", MeshPrimitives::Cube(1.3f), {left + spacing * 4.0f, 1.0f, 0.0f}, load("Cutout.mat"));
    add("hologram", MeshPrimitives::Sphere(0.8f, 32, 16), {left + spacing * 5.0f, 1.0f, 0.0f}, load("Hologram.mat"));

    // A box behind the glass shows what translucency lets through.
    add("behind-glass", MeshPrimitives::Cube(0.9f), {left + spacing * 3.0f, 0.45f, 1.6f}, GetDefaultMaterial(), {0.2f, 0.45f, 0.95f, 1.0f});

    // Thousands of copies behind the row: one instanced component, one material, two mesh levels and
    // a sprite level baked through the material. Each copy has its colour and a flicker phase (custom data).
    auto* crowd = new InstancedMeshComponent();
    crowd->SetLod(0, MeshPrimitives::Sphere(0.14f, 16, 8));
    crowd->SetLod(1, MeshPrimitives::Sphere(0.14f, 8, 4));
    crowd->SetLodScreenHeights({18.0f, 7.0f});
    crowd->SetImpostorEnabled(true);
    std::mt19937 random(7);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    for (float z = 3.0f; z < 18.0f; z += 0.4f)
    {
        for (float x = -14.0f; x < 14.0f; x += 0.4f)
        {
            MeshInstance copy;
            copy.Scale = 0.8f + 0.5f * unit(random);
            copy.Position = {x + 0.15f * unit(random), 0.14f * copy.Scale, z + 0.15f * unit(random)};
            const glm::vec3 hue = glm::clamp(glm::abs(glm::fract(unit(random) + glm::vec3(0.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f) - 1.0f,
                                             0.0f, 1.0f);
            copy.Color = glm::vec4(glm::mix(glm::vec3(1.0f), hue, 0.65f), 1.0f);
            copy.CustomData = {unit(random), 0.0f, 0.0f};
            crowd->AddInstance(copy);
        }
    }
    crowd->SetMaterial(load("Crowd.mat"));
    ExampleStartup::Register(*this, "crowd", crowd);

    ExampleStartup::Register(*this, "warm-light", new PointLightComponent({-4.0f, 2.5f, -2.5f}, {1.0f, 0.55f, 0.25f}, 2.0f, 9.0f));
    ExampleStartup::Register(*this, "cold-light", new PointLightComponent({4.5f, 2.5f, -2.5f}, {0.3f, 0.55f, 1.0f}, 2.0f, 9.0f));

    GetPlayer()->SetCameraMode(CameraMode::Orbital);
    GetPlayer()->SetOrbitTarget({0.0f, 0.8f, 0.0f});
    GetPlayer()->SetOrbitRadius(11.0f);
    GetPlayer()->SetRotate(1.5f * DirectX::XM_PI, 0.35f); // in front of the row (-Z), slightly above
}

void MaterialsGame::PreUpdate(float deltaTime)
{
    // Scene time is scaled; a slow spin keeps the highlights and the cutout readable.
    Angle += deltaTime * 2.0f;
    for (const char* name : Showcase)
    {
        if (GameComponent* component = FindComponent(name))
        {
            component->SetRotation({0.0f, Angle, 0.0f});
        }
    }
}
