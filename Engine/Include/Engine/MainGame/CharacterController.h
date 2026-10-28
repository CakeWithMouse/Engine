#pragma once
#include <Engine/InputDevice/InputActions.h>
#include <Engine/Scene/Scene.h>

class Game;
class Character;
class Player;
class PlaySettings;
class CharacterController
{
protected:
    Game& game_;
    ComponentId character_ = 0, camera_ = 0;
    float right_=0, forward_=0, yaw_=-1.5707963f, pitch_=0;
    float distance_=5, height_=2, sensitivity_=0.005f;
    bool enabled_=true;
    std::vector<Input::Connection> connections_;
    Input::Scope inputScope_; // destroyed first: no callback can reach dead state
public:
    explicit CharacterController(Game&);
    virtual ~CharacterController() = default;
    CharacterController(const CharacterController&) = delete;
    CharacterController& operator=(const CharacterController&) = delete;
    virtual void BindInput();
    virtual void Tick(float seconds);
    virtual void UpdateCamera(Player&);
    void Possess(ComponentId character, ComponentId camera = 0);
    void Configure(const PlaySettings&);
    void SetEnabled(bool);
    Character* GetCharacter() const; // resolves ID on every access; never owns a raw object pointer
    ComponentId GetCharacterId() const { return character_; }
};
