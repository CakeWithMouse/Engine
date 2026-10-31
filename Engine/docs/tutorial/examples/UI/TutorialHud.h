#pragma once
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <filesystem>

class Game;

// Runtime-only HUD, created by the game in AfterInitialize, not a scene palette type.
class TutorialHud final : public UIComponent {
public:
    explicit TutorialHud(const std::filesystem::path& markupFile);
    void Tick(float deltaTime) override;
private:
    unsigned clicks_ = 0;
};

// markupFile is a real filesystem path, not a /Game virtual path.
// Call once per game instance. Game takes ownership only after successful registration.
void AttachTutorialHud(Game& game, const std::filesystem::path& markupFile);
