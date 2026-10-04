#pragma once

#include <string>

class BaseResources;
class BaseGameConfig;

class FirstSemestrLegasy
{
    static constexpr int ActeroidCount = 500;
    static constexpr int ActeroidSunCount = 900;
    static constexpr int StarCount = 500;
    static constexpr float StartsItensity = 2.1f;
    const BaseResources& Resources;
    const BaseGameConfig& Config;
public:
    FirstSemestrLegasy(const BaseResources& resources, const BaseGameConfig& config)
        : Resources(resources), Config(config) {}

    int FirstLabStart_a();
    int FirstLabStart_b();
    int SecondLabStart();
    int ThirdLabStart(std::string Path);
    int ForthLabStart(std::string Path);
};
