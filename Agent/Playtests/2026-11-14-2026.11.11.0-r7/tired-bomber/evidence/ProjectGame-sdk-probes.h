#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/MainGame/GameSubsystem.h>
#include <Engine/SaveGame/SaveArchive.h>
#include <cstddef>
#include <vector>

class UIComponent;
class InstancedMeshComponent;
class SceneCamera;

ENGINE_SUBSYSTEM()
class BomberSessionDiagnostics final : public GameSubsystem
{
public:
    BomberSessionDiagnostics() { bTickEnabled = true; }
    bool Initialize(SubsystemContext&, std::string& error) override;
    void OnSceneReady(Scene&) override;
    void OnSceneClosing(Scene&) override;
    void Tick(float deltaTime) override;
    void SerializeSaveGame(SaveArchive& archive) override;
    void OnSaveGameLoaded(Scene& scene) override;
private:
    float elapsed_ = 0.0f;
    uint64_t ticks_ = 0;
};

ENGINE_GAME("Tired Bomber", "TiredBomber.Game")
class ProjectGame : public Game
{
public:
    void AfterInitialize() override;
    void SerializeSaveGame(SaveArchive& archive) override;
    void OnSaveGameLoaded() override;
protected:
    void PreUpdate(float deltaTime) override;
private:
    void DropBomb();
    float elapsed_ = 0.0f;
    float bombTimer_ = 1.0f;
    int hits_ = 0;
    int misses_ = 0;
    std::vector<ComponentId> units_;
    ComponentId player_ = 0;
    ComponentId floor_ = 0;
    ComponentId cameraId_ = 0;
    ComponentId activeBomb_ = 0;
    size_t targetIndex_ = 0;
    UIComponent* hud_ = nullptr;
    std::vector<Input::Connection> inputConnections_;
    Input::Scope inputScope_;
    InstancedMeshComponent* markers_ = nullptr;
    SceneCamera* diagnosticsCamera_ = nullptr;
    ComponentId sparksId_ = 0;
    float sparksTimer_ = 0.0f;
    uint32_t lastDrawnMarkers_ = UINT32_MAX;
    bool sessionObserved_ = false;
    float diagnosticsTimer_ = 0.0f;
    float playerLogTimer_ = 0.0f;
    float roleTimeProbeAccum_ = 0.0f;
    int pendingHitDumpFrames_ = -1;
    uint32_t diagnosticsFrame_ = 0;
};

