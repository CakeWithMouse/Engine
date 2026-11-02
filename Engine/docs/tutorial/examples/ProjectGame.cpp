#include "ProjectGame.h"
#include "RotatingCube.h"
#include <stdexcept>

void ProjectGame::AfterInitialize()
{
    Game::AfterInitialize();
    // One full turn in four real seconds, independent of the frame rate.
    SetSimulationTimeScale(1.0f);
    auto& scene = GetScene();
    // A saved RotatingCube already supplies the example after loading/Play.
    for (const auto& entry : scene.GetComponents())
        if (dynamic_cast<RotatingCube*>(entry.second.get())) return;
    std::unique_ptr<GameComponent> cube = std::make_unique<RotatingCube>();
    cube->SetPosition({2.0f, 1.0f, 0.0f});
    std::string name = "RotatingCube";
    for (unsigned suffix = 2; scene.FindComponentByName(name); ++suffix)
        name = "RotatingCube" + std::to_string(suffix);
    const auto added = scene.AddComponent(name, cube, "RotatingCube");
    if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
        throw std::runtime_error("Cannot add the starter RotatingCube");
}

void ProjectGame::PreUpdate(float deltaTime) { Game::PreUpdate(deltaTime); }
void ProjectGame::TickComponents(float deltaTime) { Game::TickComponents(deltaTime); }
