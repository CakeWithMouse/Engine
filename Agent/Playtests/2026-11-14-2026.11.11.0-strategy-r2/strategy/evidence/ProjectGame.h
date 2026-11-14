#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/AI/AiCharacter.h>
#include <vector>

class CorridorSoldier final : public Character {
public:
    void BeginPlay() override;
};
class CorridorGuard final : public AiCharacter {
protected:
    void SetupAi(Ai::Controller& ai) override;
};

ENGINE_GAME("ProjectGame")
class ProjectGame final : public Game {
public:
    void AfterInitialize() override;
    void PreUpdate(float deltaTime) override;
    void TickComponents(float deltaTime) override;
private:
    struct SquadVisual {
        CorridorSoldier* unit=nullptr;
        CubeComponent* mesh=nullptr;
        CubeComponent* orderMarker=nullptr;
        glm::vec3 orderTarget{0};
        bool hasOrder=false, movingToOrder=false, orderCompleted=false, orderCancelled=false;
    };
    struct GuardVisual { AiCharacter* unit=nullptr; CubeComponent* mesh=nullptr; };
    std::vector<SquadVisual> squad_;
    std::vector<GuardVisual> guards_;
    CubeComponent* crate_=nullptr;
    UIComponent* hud_=nullptr;
    UILabel* hudGoal_=nullptr;
    UILabel* hudFsm_=nullptr;
    UILabel* hudControls_=nullptr;
    UILabel* hudHelp_=nullptr;
    UILabel* hudSelected_=nullptr;
    UILabel* hudOrder_=nullptr;
    int selected_=0;
    uint32_t telemetryFrames_=0;
    std::string lastTraceStatus_="NotRequested";
    bool pointerBlocked_=false;
    bool keys_[5]{};
    bool won_=false, lost_=false, built_=false, impulsePending_=false;
    void EnsureCorridor();
    void BuildCorridor();
    void ApplyTacticalCamera();
    void AddBox(const std::string& name, glm::vec3 pos, glm::vec3 scale, glm::vec4 color, PhysicsBodyType type, float restitution=0.15f, uint32_t traceChannels=Trace::DefaultChannel);
    void UpdateHud();
};

