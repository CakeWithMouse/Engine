#include "InputCube.h"
#include <stdexcept>

void InputCube::BeginPlay()
{
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

void InputCube::Tick(float deltaTime)
{
    // Keep parent updates, but pass zero time while rotation is disabled.
    RotatingCube::Tick(spinning_ ? deltaTime : 0.0f);
}

void InputCube::SpawnCube()
{
    auto& scene = GetGame()->GetScene();
    const auto number = nextCube_++;
    std::string name = "SpawnedCube" + std::to_string(number);
    while (scene.FindComponentByName(name))
        name = "SpawnedCube" + std::to_string(nextCube_++);
    std::unique_ptr<GameComponent> cube = std::make_unique<RotatingCube>();
    cube->SetPosition(GetLocalPosition() + glm::vec3(2.0f * number, 0, 0));
    const auto added = scene.AddComponent(name, cube, "RotatingCube");
    if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
        throw std::runtime_error("Cannot spawn cube");
}
