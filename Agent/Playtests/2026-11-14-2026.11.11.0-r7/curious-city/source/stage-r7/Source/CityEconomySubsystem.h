#pragma once
#include <glm/glm.hpp>
#include <Engine/MainGame/GameSubsystem.h>

class Scene;
class UIComponent;
class SaveArchive;
ENGINE_SUBSYSTEM()
class CityEconomySubsystem final : public GameSubsystem
{
public:
    CityEconomySubsystem();
    void OnSceneReady(Scene& scene) override;
    void OnSceneClosing(Scene&) override;
    void OnSaveGameLoaded(Scene& scene) override;
    void Tick(float deltaTime) override;
    void SerializeSaveGame(SaveArchive& archive) override;
    void AttachHud(UIComponent* component);
    bool BuildOneHouse(Scene& scene);
    bool BuildOneHouse(Scene& scene, const glm::vec3& position);
    int Balance() const { return balance; }
    int Houses() const { return houses; }
private:
    void UpdateHud();
    int balance = 500;
    int houses = 0;
    double incomeClock = 0.0;
    UIComponent* hud = nullptr;
};
