#include "ScriptsScene.h"
#include "ExampleStartup.h"
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Script/ScriptRuntime.h>
#include <stdexcept>

namespace
{
const char* const Scripted[] = {"spinner", "bobber", "mover"};
}

void ScriptsGame::BuildScene(const std::filesystem::path& gameContent)
{
    // Scripts speak in seconds; the old examples' scaled scene time would make them several times faster.
    SetSimulationTimeScale(1.0f);
    const std::filesystem::path scripts = gameContent / "Scripts";
    auto add = [&](const std::string& name, MeshLodData mesh, const glm::vec3& position, const glm::vec4& color, const char* script)
    {
        auto* component = new MeshComponent(position, glm::vec3(0.0f), glm::vec3(1.0f), color);
        component->SetMesh(std::move(mesh));
        component->SetMaterial(GetDefaultMaterial());
        if (script)
        {
            const std::filesystem::path file = scripts / script;
            if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("Missing script " + file.generic_u8string());
            // Outside an editor project there is no /Game: the file path itself is the script asset.
            component->SetScriptAsset(file.generic_u8string());
        }
        ExampleStartup::Register(*this, name, component);
    };

    SetDirectionalLight({-0.45f, -1.0f, -0.35f}, {1.0f, 0.96f, 0.9f}, 0.85f);
    SetShadowSettings(true, 80.0f);
    add("floor", MeshPrimitives::Plane(30.0f, 30.0f), {0.0f, 0.0f, 0.0f}, {0.55f, 0.58f, 0.62f, 1.0f}, nullptr);
    add("spinner", MeshPrimitives::Cube(1.4f), {-3.0f, 1.0f, 0.0f}, {0.95f, 0.55f, 0.2f, 1.0f}, "Spin.vscript");
    add("bobber", MeshPrimitives::Sphere(0.8f), {0.0f, 1.2f, 0.0f}, {0.3f, 0.75f, 0.45f, 1.0f}, "Bob.vscript");
    add("mover", MeshPrimitives::Cube(1.0f), {3.0f, 0.5f, 0.0f}, {0.3f, 0.5f, 0.95f, 1.0f}, "Move.vscript");

    GetPlayer()->SetCameraMode(CameraMode::Orbital);
    GetPlayer()->SetOrbitTarget({0.0f, 0.8f, 0.0f});
    GetPlayer()->SetOrbitRadius(10.0f);
    GetPlayer()->SetRotate(1.5f * DirectX::XM_PI, 0.35f);
}

std::string ScriptsGame::Verify() const
{
    for (const char* name : Scripted)
    {
        const GameComponent* component = FindComponent(name);
        const Script::Instance* script = component ? component->GetScriptInstance() : nullptr;
        if (!script) return std::string(name) + ": the script did not start";
        if (script->Failed()) return std::string(name) + ": " + script->Error().message;
    }
    if (FindComponent("spinner")->GetRotation().y <= 0.0f) return "spinner: the script did not turn the cube";
    return {};
}
