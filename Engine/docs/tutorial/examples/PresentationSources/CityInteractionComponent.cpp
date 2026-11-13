#include "CityInteractionComponent.h"
#include "CityEconomySubsystem.h"
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/SaveGame/SaveGame.h>
#include <Engine/Scene/Trace.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
    void RecordUiClick(const char* event)
    {
        const char* path = std::getenv("CURIOUS_CITY_LOG");
        if (!path) return;
        std::ofstream log(std::filesystem::u8path(path), std::ios::app);
        if (log) log << "event=" << event << '\n';
    }
}

void CityInteractionComponent::BeginPlay()
{
    GameComponent::BeginPlay();
    auto* game = GetGame();
    auto* economy = game ? game->GetSubsystem<CityEconomySubsystem>() : nullptr;
    auto* hud = game ? game->GetProjectHud() : nullptr;
    if (!economy) return;
    if (hud) {
        economy->AttachHud(hud);
        hud->BindClick("buildHouse", [economy, game] { RecordUiClick("build_ui_click"); economy->BuildOneHouse(game->GetScene()); });
        hud->BindClick("saveCity", [game] { RecordUiClick("save_ui_click"); game->Saves().RequestSave("city"); });
        hud->BindClick("loadCity", [game] { RecordUiClick("load_ui_click"); game->Saves().RequestLoad("city"); });
    }
    Input::Binding place;
    place.action = "CuriousCity.PlaceHouse";
    place.context = Input::Context::Game;
    place.chord = {Keys::LeftButton};
    game->GetInput().Bind(place);
    placeConnection = game->GetInput().Subscribe(placeScope, Input::Context::Game,
        "CuriousCity.PlaceHouse", Input::Phase::Pressed,
        [economy, game](const Input::Event& event) {
            if (event.cancelled) return;
            const auto hit = game->RaycastCursor();
            if (!hit || !hit.hit) return;
            auto* target = game->GetScene().FindComponentByName("Ground");
            if (!target || target->GetComponentId() != hit.hit->componentId) return;
            RecordUiClick("ground_raycast_place_click");
            economy->BuildOneHouse(game->GetScene(), hit.hit->position + glm::vec3(0.0f, 0.75f, 0.0f));
        });
}
