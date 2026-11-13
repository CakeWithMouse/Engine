// Complete compilation units for the short usage snippets on the slides.
#include "SessionCounterSubsystem.h"
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/SaveGame/SaveGame.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Scene/Scene.h>
#include <memory>
#include <utility>

std::pair<unsigned, unsigned> ReadSessionCounters(Game& game) {
    auto* counter = game.GetSubsystem<SessionCounterSubsystem>();
    if (!counter) return {};
    return {counter->scenesOpened, counter->ticks};
}

struct SaveRequestExample {
    uint64_t ticket = 0;
    SaveGameResult Request(Game& game) {
        auto result = game.Saves().RequestSave("city");
        if (result.status == SaveGameResult::Status::Accepted)
            ticket = result.ticket;
        return result; // Caller handles Busy and Failed, including message.
    }
    bool IsSaved(Game& game) const {
        const auto& result = game.Saves().LastResult();
        return ticket != 0 && result.ticket == ticket &&
            result.status == SaveGameResult::Status::Saved;
    }
};

AddComponentResult AddTutorialLamp(Scene& scene) {
    auto lamp = std::make_unique<PointLightComponent>(
        glm::vec3(0, 4, 0), glm::vec3(1, 0.7f, 0.4f), 1.4f, 12.0f);
    lamp->SetCastShadows(true);
    std::unique_ptr<GameComponent> object = std::move(lamp);
    return scene.AddComponent("Lamp", object, "Light.Point");
}
