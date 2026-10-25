#pragma once
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <filesystem>

/**
 * Material assets on procedural meshes: a lit floor, the default material, gold, an animated
 * emissive, glass, a masked cutout and an additive hologram. Unlike the other BasicExamples
 * scenes it uses the regular renderer (depth, lights, shadows, F5 forward/deferred).
 * The .mat/.hlsl files are in GameContent/Materials; edit them there and press F6.
 */
class MaterialsGame : public Game
{
public:
    /** Call after Initialize. Throws if a material file cannot be loaded. */
    void BuildScene(const std::filesystem::path& gameContent);

protected:
    void PreUpdate(float deltaTime) override;
    std::array<float, 4> GetClearColor() const override { return {0.07f, 0.08f, 0.11f, 1.0f}; }

private:
    float Angle = 0.0f;
};
