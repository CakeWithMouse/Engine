#pragma once
#include <Engine/MainGame/GameSubsystem.h>
inline int liveSubsystems = 0, subsystemTicks = 0;
inline bool failSubsystem = false;
ENGINE_SUBSYSTEM()
class SdkCounter final : public GameSubsystem
{
public:
    SdkCounter() { ++liveSubsystems; bTickEnabled=true; }
    ~SdkCounter() override { --liveSubsystems; }
    bool Initialize(SubsystemContext&, std::string&) override { return !failSubsystem; }
    void Tick(float) override { ++subsystemTicks; }
};
