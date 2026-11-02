#include <Engine/MainGame/BaseGameClass/Game.h>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
#define ABSENT_METHOD(Name) \
template<class T, class = void> struct Has##Name : std::false_type {}; \
template<class T> struct Has##Name<T, std::void_t<decltype(&T::Name)>> : std::true_type {}; \
static_assert(!Has##Name<Game>::value);
ABSENT_METHOD(SetVSync)
ABSENT_METHOD(SetRenderingType)
ABSENT_METHOD(GetRenderingType)
ABSENT_METHOD(SetAntialiasing)
ABSENT_METHOD(GetAntialiasing)
ABSENT_METHOD(RequestWindowClose)
#undef ABSENT_METHOD
static_assert(std::is_same_v<decltype(std::declval<Game&>().GetSettings()), GameSettings&>);
static_assert(std::is_same_v<decltype(std::declval<const Game&>().GetSettings()), const GameSettings&>);
static_assert(!std::is_copy_constructible_v<GameSettings> && !std::is_move_constructible_v<GameSettings>);

class DispatchGame final : public Game
{
public:
    RenderType drawn = Custom;
    using Game::RenderFrame;
    void DrawForward() override { drawn = Forward; }
    void DrawDeffered() override { drawn = Deffered; }
    void Draw() override { drawn = Custom; }
};
void Require(bool value) { if (!value) throw std::runtime_error("GameSettings contract failed"); }
}

void GameSettingsTests()
{
    DispatchGame first, second;
    auto& settings = first.GetSettings();
    const Game& view = first;
    Require(&settings == &view.GetSettings() && &settings != &second.GetSettings());
    Require(settings.IsVSyncEnabled() && settings.GetRenderingType() == Forward && settings.GetAntialiasing() == 0);
    settings.SetVSync(false);
    settings.SetAntialiasing(1);
    Require(!view.GetSettings().IsVSyncEnabled() && view.GetSettings().GetAntialiasing() == 1);
    for (auto type : {Deffered, Custom, Forward})
    {
        settings.SetRenderingType(type);
        first.RenderFrame();
        Require(first.drawn == type && view.GetSettings().GetRenderingType() == type);
    }
    Require(second.GetSettings().IsVSyncEnabled() && second.GetSettings().GetAntialiasing() == 0);
}
