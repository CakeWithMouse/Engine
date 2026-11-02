#pragma once
#include <cstdint>

enum RenderType
{
    Forward,
    Deffered, // Historical spelling retained for source compatibility.
    Custom
};

// Settings of one Game session. Obtain by reference through Game::GetSettings().
// Call on the game/render thread. Changes affect subsequent rendering; no Apply is needed.
// These values are not automatically serialized into the scene or project.
class GameSettings final
{
public:
    GameSettings(const GameSettings&) = delete;
    GameSettings& operator=(const GameSettings&) = delete;

    void SetVSync(bool enabled) { vsync_ = enabled; }
    bool IsVSyncEnabled() const { return vsync_; }

    // Shader variants are resolved per pass; the rendering type can change at runtime.
    void SetRenderingType(RenderType type) { renderingType_ = type; }
    RenderType GetRenderingType() const { return renderingType_; }

    // 0 = Off, 1 = FXAA. Other values render without AA (legacy host compatibility).
    void SetAntialiasing(uint32_t mode) { antialiasing_ = mode; }
    uint32_t GetAntialiasing() const { return antialiasing_; }

    // Deferred until the next standalone frame boundary. Idempotent.
    // Embedded editor Play ignores this request; it never closes the editor window.
    void RequestWindowClose() { closeRequested_ = true; }

private:
    friend class Game;
    GameSettings() = default;
    bool vsync_ = true;
    RenderType renderingType_ = Forward;
    uint32_t antialiasing_ = 0;
    bool closeRequested_ = false;
};
