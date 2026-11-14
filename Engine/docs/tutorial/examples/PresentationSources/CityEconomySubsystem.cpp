#include "CityEconomySubsystem.h"
#include "HouseComponent.h"
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/SaveGame/SaveArchive.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
    void Record(const char* event, int balance, int houses)
    {
        const char* path = std::getenv("CURIOUS_CITY_LOG");
        if (!path) return;
        std::ofstream log(std::filesystem::u8path(path), std::ios::app);
        if (log) log << "event=" << event << " balance=" << balance << " houses=" << houses << '\n';
    }
}

CityEconomySubsystem::CityEconomySubsystem() { bTickEnabled = true; }

void CityEconomySubsystem::OnSceneReady(Scene& scene)
{
    if (auto* cube = scene.FindComponentByName("Cube")) scene.RemoveComponent(cube->GetComponentId());
    houses = 0;
    for (const auto& entry : scene.GetComponents())
        if (dynamic_cast<HouseComponent*>(entry.second.get())) ++houses;
    Record("scene_ready", balance, houses);
    UpdateHud();
}

void CityEconomySubsystem::OnSceneClosing(Scene&) { hud = nullptr; }

void CityEconomySubsystem::AttachHud(UIComponent* component)
{
    hud = component;
    Record("hud_reference_rebound", balance, houses);
    UpdateHud();
}

void CityEconomySubsystem::OnSaveGameLoaded(Scene& scene)
{
    hud = nullptr; // Scene and its UI components have been replaced.
    houses = 0;
    for (const auto& entry : scene.GetComponents())
        if (dynamic_cast<HouseComponent*>(entry.second.get())) ++houses;
    Record("savegame_loaded_references_cleared", balance, houses);
    const char* path = std::getenv("CURIOUS_CITY_LOG");
    if (path) {
        std::ofstream log(std::filesystem::u8path(path), std::ios::app);
        for (const auto& entry : scene.GetComponents()) {
            const auto* skeletal = dynamic_cast<const SkeletalMeshComponent*>(entry.second.get());
            if (!skeletal) continue;
            const auto& slots = skeletal->GetMaterialSlots();
            log << "savegame_skeletal_slots name=" << entry.first << " count=" << slots.size();
            for (const auto& slot : slots) log << " slot_" << slot.first << "=" << slot.second;
            log << '\\n';
        }
    }
}

void CityEconomySubsystem::SerializeSaveGame(SaveArchive& archive)
{
    GameSubsystem::SerializeSaveGame(archive);
    archive.Section("CuriousCity.Economy", [&](SaveArchive& state) {
        state.Version(1);
        state.Field("balance", balance);
        state.Field("houses", houses);
        state.Field("incomeClock", incomeClock);
    });
}

bool CityEconomySubsystem::BuildOneHouse(Scene& scene)
{
    return BuildOneHouse(scene, {static_cast<float>((houses % 5) * 2 - 4), 0.75f, -2.0f});
}

bool CityEconomySubsystem::BuildOneHouse(Scene& scene, const glm::vec3& position)
{
    constexpr int cost = 100;
    if (!hud || balance < cost) {
        Record("purchase_rejected", balance, houses);
        return false;
    }
    std::string name = "House_" + std::to_string(houses + 1);
    for (unsigned suffix = 2; scene.FindComponentByName(name); ++suffix)
        name = "House_" + std::to_string(houses + 1) + "_" + std::to_string(suffix);
    auto house = std::make_unique<HouseComponent>();
    house->SetPosition(position);
    house->SetMaterialAsset(houses % 2 == 0
        ? "/Game/Models/blend-suzanne-multiple/Materials/Material_0.mat"
        : "/Game/Models/blend-suzanne-multiple/Materials/Material_1.mat");
    std::unique_ptr<GameComponent> component = std::move(house);
    const auto added = scene.AddComponent(name, component, "CuriousCity.House");
    if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred) {
        Record("house_add_failed_no_charge", balance, houses);
        return false;
    }
    balance -= cost;
    ++houses;
    Record(added.result == SceneResult::Deferred ? "house_add_deferred_accepted_and_charged" : "house_add_succeeded_and_charged", balance, houses);
    UpdateHud();
    return true;
}

void CityEconomySubsystem::Tick(float deltaTime)
{
    if (deltaTime <= 0.0f) return;
    incomeClock += deltaTime;
    while (incomeClock >= 1.0) {
        incomeClock -= 1.0;
        balance += houses * 10;
        UpdateHud();
    }
}

void CityEconomySubsystem::UpdateHud()
{
    if (!hud) return;
    hud->SetLabelText("balance", "Balance: " + std::to_string(balance));
    hud->SetLabelText("income", "Income: " + std::to_string(houses * 10) + " / second");
    hud->SetLabelText("houses", "Homes: " + std::to_string(houses));
}
