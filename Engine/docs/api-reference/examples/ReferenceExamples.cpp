#include "ReferenceComponent.h"
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Components/Character.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Runtime/ModuleVfs.h>
#include <fstream>
#include <filesystem>
#include <stdexcept>

namespace ReferenceExamples {
// example:settings
void ConfigureGame(Game& game)
{
    auto& settings = game.GetSettings();
    settings.SetVSync(true);
    settings.SetRenderingType(Forward);
    settings.SetAntialiasing(1); // 0 = Off, 1 = FXAA. Applied by subsequent rendering.
}

void OnExitButton(Game& game)
{
    game.GetSettings().RequestWindowClose(); // Standalone; ignored by embedded Play.
}
// endexample

// example:scene
ComponentId AddCube(Scene& scene)
{
    std::unique_ptr<GameComponent> cube = std::make_unique<ReferenceComponent>();
    cube->SetPosition({0, 1, 0});
    auto result = scene.AddComponent("reference-cube", cube, "Reference.Cube");
    if (result.result != SceneResult::Ok && result.result != SceneResult::Deferred)
        throw std::runtime_error("Cannot add reference-cube");
    // cube is now null. For Deferred, resolve the ID only after the update boundary.
    return result.id;
}
// endexample

// example:world
Trace::Result WorldRaycast(Scene& scene, ComponentId ignored)
{
    Trace::Ray ray{{0, 10, 0}, {0, -1, 0}, 100};
    Trace::Query query;
    query.maxDistance = 100;
    query.ignore = {ignored};
    query.geometry = Trace::Geometry::Exact;
    auto result = scene.Raycast(ray, query);
    if (result) {
        // Resolve briefly when needed. Never retain this pointer across reload/removal.
        auto* component = scene.FindComponent(result.hit->componentId);
        (void)component;
    }
    return result; // Miss and InvalidRay are different statuses.
}
// endexample

// example:screen
Trace::Result PixelRaycast(Game& game)
{
    Trace::Query query;
    query.maxDistance = 500;
    // Physical viewport pixel (120, 80): sample its center at (120.5, 80.5).
    return game.RaycastScreen(Trace::ViewportPoint::Pixel(120, 80), query);
}

Trace::Result CursorRaycast(Game& game)
{
    auto result = game.RaycastCursor();
    if (result.status == Trace::Status::InputBlocked) {
        // UI owns the pointer, or the Game input context is disabled.
    }
    return result;
}

Trace::RayResult ScreenRay(Game& game)
{
    // Fractional coordinates are preserved. The origin is on the near plane.
    return game.ScreenPointToRay({120.25f, 80.75f}, 500);
}
// endexample

// example:mesh
ComponentId AddGround(Scene& scene)
{
    auto mesh = std::make_unique<MeshComponent>();
    mesh->SetPrimitive(MeshPrimitives::Plane(20, 20, 1)); // XZ, +Y normal.
    std::unique_ptr<GameComponent> component = std::move(mesh);
    auto result = scene.AddComponent("ground", component);
    if (result.result != SceneResult::Ok && result.result != SceneResult::Deferred)
        throw std::runtime_error("Cannot add ground");
    return result.id; // Procedural geometry needs its own save/reconstruction policy.
}
// endexample

// example:input
class ClickAction
{
public:
    explicit ClickAction(Game& game) : game_(game)
    {
        Input::Binding binding;
        binding.action = "Reference.Select";
        binding.context = Input::Context::Game;
        binding.chord = {Keys::LeftButton};
        game_.GetInput().Bind(binding);
        connection_ = game_.GetInput().Subscribe(scope_, Input::Context::Game,
            "Reference.Select", Input::Phase::Pressed, [this](const Input::Event& event) {
                if (!event.cancelled) lastHit_ = game_.RaycastCursor();
            });
    }
    ~ClickAction() { game_.GetInput().Unbind(Input::Context::Game, "Reference.Select"); }
private:
    Game& game_; // The Game must outlive this helper.
    Trace::Result lastHit_;
    Input::Connection connection_; // Keep the subscription alive.
    Input::Scope scope_; // Last field: disables callbacks before other fields die.
};
// endexample

// example:ui
void BuildHud(UIComponent& ui)
{
    UIRect rect;
    rect.X = 16; rect.Y = 16; rect.Width = 200; rect.Height = 32;
    auto* label = ui.AddLabel(rect, "Ready", {1, 1, 1, 1});
    label->Name = "status";
    // Named updates remain useful when markup replaces the tree.
    ui.SetLabelText("status", "Running");
}
// endexample

// example:resources
std::string ReadGameLine()
{
    const auto path = EngineModuleVfs::Resolve("/Game/Config/message.txt");
    if (path.empty()) return {};
    std::ifstream input(std::filesystem::u8path(path));
    std::string line;
    if (!std::getline(input, line)) return {};
    return line;
}
// endexample
} // namespace ReferenceExamples
