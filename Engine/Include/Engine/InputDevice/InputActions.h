#pragma once
#include <Engine/InputDevice/Keys.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Input
{
enum class Context { Game, Editor, UI };
enum class Phase { Pressed, Held, Released, Axis };
enum class Source { Keys, MouseX, MouseY, Wheel };
struct Event { float value = 0, deltaTime = 0; bool cancelled = false; };
struct Binding
{
    std::string action;
    Context context = Context::Game;
    std::vector<Keys> chord;
    float scale = 1;
    int priority = 0;
    bool consume = true;
    Source source = Source::Keys;
};
// Engine-thread only. Keep Scope as a member, declared AFTER fields used by callbacks.
// Neither the owner nor the input system needs to outlive the other.
class Scope final
{
    struct State { bool enabled = true; unsigned generation = 0; };
    std::shared_ptr<State> state_ = std::make_shared<State>();
    friend class System;
    friend class Connection;
public:
    Scope() = default;
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    void SetEnabled(bool enabled) { if (state_->enabled != enabled) { state_->enabled = enabled; ++state_->generation; } }
    bool IsEnabled() const { return state_->enabled; }
};
class Connection final
{
    friend class System;
    struct Slot;
    std::shared_ptr<Slot> slot_;
    explicit Connection(std::shared_ptr<Slot> slot) : slot_(std::move(slot)) {}
public:
    Connection() = default;
    ~Connection();
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    void Disconnect();
};
class System final
{
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    System();
    ~System();
    System(const System&) = delete;
    System& operator=(const System&) = delete;
    Connection Subscribe(Scope&, Context, std::string action, Phase, std::function<void(const Event&)>);
    void Bind(Binding);
    void Unbind(Context, const std::string& action);
    void ClearBindings(Context);
    void SetContextEnabled(Context, bool);
    bool IsContextEnabled(Context) const;
    void SetContextPriority(Context, int);
    void SetKey(Keys, bool down); // repeated downs are ignored
    void AddPointer(float x, float y, float wheel = 0);
    void Tick(float deltaTime); // drains edges, then one Held/Axis event per frame
    void Reset(); // cancel actions, suppress held keys until their physical release
    void ReleaseAll(); // focus lost: also forget physical device state
};
}
