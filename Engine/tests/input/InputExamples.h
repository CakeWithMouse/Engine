#pragma once
#include <Engine/Components/Character.h>

// Ordinary C++ class: no engine base class or globally stored callback required.
class InputShortcutExample
{
    Input::Connection connection_;
    Input::Scope inputScope_;
public:
    int activations=0;
    explicit InputShortcutExample(Input::System& input) {
        input.Bind({"Tools.Mark",Input::Context::Game,{Keys::Control,Keys::Shift,Keys::M}});
        connection_=input.Subscribe(inputScope_,Input::Context::Game,"Tools.Mark",Input::Phase::Pressed,
            [this](const Input::Event&){++activations;});
    }
    void SetEnabled(bool value){inputScope_.SetEnabled(value);}
};

// A project component can be registered with ENGINE_COMPONENT("Action component").
class InputComponentExample : public GameComponent
{
    Input::Connection connection_;
    Input::Scope inputScope_;
public:
    int activations=0;
    void BeginPlay() override {
        GameComponent::BeginPlay();
        GetGame()->GetInput().Bind({"Component.Use",Input::Context::Game,{Keys::U}});
        connection_=GetGame()->GetInput().Subscribe(inputScope_,Input::Context::Game,"Component.Use",Input::Phase::Pressed,
            [this](const Input::Event&){++activations;});
    }
    bool IsRenderable() const override {return false;}
};

// Opt-in custom input in a Character subclass; base Character has no bindings.
class InputCharacterExample : public Character
{
    Input::Connection connection_;
    Input::Scope inputScope_;
public:
    void BeginPlay() override {
        Character::BeginPlay();
        GetGame()->GetInput().Bind({"Hero.Strafe",Input::Context::Game,{Keys::L}});
        connection_=GetGame()->GetInput().Subscribe(inputScope_,Input::Context::Game,"Hero.Strafe",Input::Phase::Held,
            [this](const Input::Event& e){Move({1,0,0},e.deltaTime);});
    }
};
