#include "MaterialsScene.h"
#include "ExampleStartup.h"
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Render/Material.h>
#include <Engine/Render/MeshPrimitives.h>
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

    add("floor", MeshPrimitives::Plane(24.0f, 24.0f), {0.0f, 0.0f, 0.0f}, load("Floor.mat"));
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
