#include "InputCube.h"
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Scene/Scene.h>
#include <memory>
#include <stdexcept>
#include <string>

void InputCube::BeginPlay() {
    RotatingCube::BeginPlay();
    auto& input = GetGame()->GetInput();
    input.Bind({"ToggleRotation", Input::Context::Game, {Keys::R}});
    toggle_ = input.Subscribe(inputScope_, Input::Context::Game,
        "ToggleRotation", Input::Phase::Pressed,
        [this](const Input::Event&) { spinning_ = !spinning_; });
    input.Bind({"SpawnCube", Input::Context::Game, {Keys::N}});
    spawn_ = input.Subscribe(inputScope_, Input::Context::Game,
        "SpawnCube", Input::Phase::Pressed,
        [this](const Input::Event&) { SpawnCube(); });
}

void InputCube::Tick(float deltaTime) {
    RotatingCube::Tick(spinning_ ? deltaTime : 0.0f);
}

void InputCube::SpawnCube() {
    auto& scene = GetGame()->GetScene();
    unsigned number;
    std::string name;
    do {
        number = nextCube_++;
        name = "SpawnedCube" + std::to_string(number);
    } while (scene.FindComponentByName(name));
    std::unique_ptr<GameComponent> cube = std::make_unique<RotatingCube>();
    cube->SetPosition(GetLocalPosition() + glm::vec3(2.0f * number, 0, 0));
    const auto added = scene.AddComponent(name, cube, "RotatingCube");
    if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
        throw std::runtime_error("Cannot spawn " + name);
}
