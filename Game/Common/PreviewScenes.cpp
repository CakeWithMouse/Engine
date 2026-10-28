#include "PreviewScenes.h"
#include "../BasicExamples/Source/EditorPreview.h"
#include "../Pong/Source/PongGame.h"
#include "../SunGame/Source/SunGame.h"
#include "../Battle/Source/BattleGame.h"
#include <stdexcept>

namespace
{
class PongPreview final : public PongGame
{
    void AfterInitialize() override
    {
        BuildScene();
        GetPlayer()->SetPosition({0.0f, 0.0f, -16.0f});
        GetPlayer()->Rotate(DirectX::XM_PI, 0.0f);
    }
};

class SunPreview final : public SunGame
{
    std::filesystem::path content;
public:
    explicit SunPreview(std::filesystem::path root) : SunGame(SunGameConfig{}), content(std::move(root)) {}
    void AfterInitialize() override
    {
        BuildScene(content);
        SetDeferredDebugOverlay(false);
        GetPlayer()->SetCameraMode(CameraMode::Orbital);
        GetPlayer()->SetOrbitTarget({0.0f, 0.0f, 0.0f});
        GetPlayer()->SetOrbitRadius(1800.0f);
        GetPlayer()->SetRotate(0.6f, 0.8f);
    }
};

class BattlePreview final : public BattleGame
{
    std::filesystem::path content;
public:
    explicit BattlePreview(std::filesystem::path root) : content(std::move(root)) {}
    void AfterInitialize() override
    {
        Settings settings;
        settings.ContentRoot = content;
        BuildScene(settings);
        SetDeferredDebugOverlay(false);
    }
};
}

std::function<std::unique_ptr<Game>()> FindPreviewScene(
    const std::string& identifier, const std::filesystem::path& contentRoot)
{
    if (identifier.empty()) return {};
    if (identifier == "BasicExamples") return [] { return std::make_unique<BasicExamplesPreview>(); };
    if (identifier == "Pong") return [] { return std::make_unique<PongPreview>(); };
    if (identifier == "SunGame") return [contentRoot] { return std::make_unique<SunPreview>(contentRoot); };
    if (identifier == "Battle") return [contentRoot] { return std::make_unique<BattlePreview>(contentRoot); };
    throw std::runtime_error("Unsupported preview scene: " + identifier);
}
