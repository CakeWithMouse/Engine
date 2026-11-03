#pragma once
#include <Engine/MainGame/GameSession.h>
#include <stdexcept>
#include <vector>
inline std::vector<int> events;
inline int created = 0, destroyed = 0;
inline bool failInit = false, throwTick = false, stopTick = false, stopReady = false;
inline void (*onTick)() = nullptr;
inline void (*onInitialize)() = nullptr;
inline void (*onDeinitialize)() = nullptr;
ENGINE_SUBSYSTEM()
class Counter final : public GameSubsystem
{
public:
    GameSession* session = nullptr;
    unsigned scenes = 0, ticks = 0;
    float lastDt = 0;
    int identity = ++created;
    ~Counter() override { ++destroyed; events.push_back(5); }
    bool Initialize(SubsystemContext& context, std::string& error) override {
        session = &context.session; events.push_back(1);
        if(onInitialize) onInitialize();
        if (context.GetScene() || session->GetSubsystem<Counter>()) throw std::runtime_error("Initialize isolation");
        if (failInit) { error = "expected init failure"; return false; }
        return true;
    }
    void OnSceneReady(Scene&) override { ++scenes; events.push_back(2); if(stopReady) session->Stop(); }
    void OnSceneClosing(Scene&) override { events.push_back(3); }
    void Tick(float dt) override {
        ++ticks; lastDt = dt;
        if (onTick) onTick();
        if (throwTick) throw std::runtime_error("expected tick failure");
        if (stopTick) { session->Stop(); if (destroyed) throw std::runtime_error("destroyed in callback"); }
    }
    void Deinitialize() noexcept override { events.push_back(4); session->Stop(); if(onDeinitialize) onDeinitialize(); }
};
