#include "TutorialHud.h"
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <stdexcept>

TutorialHud::TutorialHud(const std::filesystem::path& markupFile) {
    FontAtlasSettings font;
    font.Face = L"Consolas";
    font.PixelHeight = 18;
    SetFont(font);
    if (!LoadMarkup(markupFile.generic_u8string()))
        throw std::runtime_error("Cannot load tutorial HUD: " + markupFile.generic_u8string());
    BindClick("add", [this] { ++clicks_; });
}

void TutorialHud::Tick(float deltaTime) {
    UIComponent::Tick(deltaTime);
    SetLabelText("clicks", "Нажатий: " + std::to_string(clicks_));
    SetBarFill("load", static_cast<float>(clicks_ % 11) / 10.0f);
}

void AttachTutorialHud(Game& game, const std::filesystem::path& markupFile) {
    auto hud = std::make_unique<TutorialHud>(markupFile);
    // UIComponent's constructor selects Shaders/UI.hlsl.
    const auto result = game.RegisterComponent("TutorialHud", hud.get());
    if (result != RegisterResult::Ok)
        throw std::runtime_error("Cannot register TutorialHud");
    hud.release();
}
