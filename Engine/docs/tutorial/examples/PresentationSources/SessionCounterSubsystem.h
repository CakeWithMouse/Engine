#pragma once
#include <Engine/MainGame/GameSubsystem.h>

ENGINE_SUBSYSTEM()
class SessionCounterSubsystem final : public GameSubsystem
{
public:
    SessionCounterSubsystem() { bTickEnabled = true; }
    unsigned scenesOpened = 0;
    unsigned ticks = 0;
    void OnSceneReady(Scene&) override { ++scenesOpened; }
    void Tick(float) override { ++ticks; }
};
