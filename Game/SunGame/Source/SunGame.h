#pragma once
#include <Engine/MainGame/BaseGameClass/Game.h>
#include "SunGameConfig.h"
#include <filesystem>

/** Example-specific scene. Game supplies the window, device, renderer and frame loop. */
class SunGame : public Game
{
public:
    explicit SunGame(const SunGameConfig& config, int frameLimit = 0)
        : Config(config), RemainingFrames(frameLimit) {}
    void BuildScene(const std::filesystem::path& contentRoot);
protected:
    std::array<float, 4> GetClearColor() const override { return {0.001f, 0.001f, 0.001f, 1.0f}; }
    void PreUpdate(float deltaTime) override;
private:
    SunGameConfig Config;
    int RemainingFrames = 0; // Nonzero only for the explicit --smoke run.
    static constexpr int ActeroidCount = 500;
    static constexpr int ActeroidSunCount = 900;
    static constexpr int StarCount = 500;
    static constexpr float StartsItensity = 2.1f;
};
