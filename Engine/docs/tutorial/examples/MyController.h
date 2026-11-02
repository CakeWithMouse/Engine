#pragma once
#include <Engine/MainGame/CharacterController.h>
#include <Engine/MainGame/BaseGameClass/Game.h>

// Optional: replaces the standard character bindings with I = forward.
class MyController : public CharacterController {
public:
    using CharacterController::CharacterController;
    void BindInput() override {
        game_.GetInput().Bind({"Go", Input::Context::Game, {Keys::I}});
        connections_.push_back(game_.GetInput().Subscribe(
            inputScope_, Input::Context::Game, "Go", Input::Phase::Axis,
            [this](const Input::Event& e) { forward_ = e.value; }));
    }
};
