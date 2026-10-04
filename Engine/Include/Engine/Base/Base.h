#pragma once
#include <Engine/Base/Config/BaseResources.h>
#include <Engine/Base/Config/BaseGameConfig.h>

// Foundation for the future engine lifecycle. Methods are not implemented yet;
// the current application lifecycle remains in Game.

class BaseEngine
{
    BaseResources Resources;
    BaseGameConfig Config;
public:
    bool StartUp();
    bool Play();
    bool IsPlaying();
    void Stop();
    bool Initialize(BaseResources Resource, const BaseGameConfig& GameConfig);
};
