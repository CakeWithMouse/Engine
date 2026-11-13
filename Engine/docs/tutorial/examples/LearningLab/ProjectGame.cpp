#include "ProjectGame.h"
#include "InputCube.h"
#include "AudioTutorial.h"
#include "ModelExample.h"
#include "TutorialHud.h"
#include "MyController.h" // Optional controller; compiled here, default input stays active.
#include <Engine/Runtime/ModuleVfs.h>
#include <filesystem>
#include <stdexcept>

void ProjectGame::AfterInitialize() {
    Game::AfterInitialize();
    SetSimulationTimeScale(1.0f);
    auto& scene = GetScene();
    if (!scene.FindComponentByName("LearningCube")) {
        std::unique_ptr<GameComponent> cube = std::make_unique<InputCube>();
        cube->SetPosition({2.0f, 1.0f, 0.0f});
        AssignMaterial(*cube, "/Game/Materials/Gold.mat");
        AssignTexture(*cube, "/Game/Textures/checker.png");
        const auto added = scene.AddComponent("LearningCube", cube, "InputCube");
        if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
            throw std::runtime_error("Cannot add LearningCube");
    }
    if (!scene.FindComponentByName("LearningModel"))
        SpawnModel(*this, "LearningModel", "/Game/Models/pyramid.obj");
    if (!scene.FindComponentByName("LearningAudio")) {
        std::unique_ptr<GameComponent> sound = std::make_unique<AudioTutorial>();
        const auto added = scene.AddComponent("LearningAudio", sound, "Tutorial.Audio");
        if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
            throw std::runtime_error("Cannot add LearningAudio");
    }
    AttachTutorialHud(*this, std::filesystem::u8path(
        EngineModuleVfs::Resolve("/Game/UI/Hud.ui")));
}
void ProjectGame::PreUpdate(float deltaTime) { Game::PreUpdate(deltaTime); }
void ProjectGame::TickComponents(float deltaTime) { Game::TickComponents(deltaTime); }
