// Generated from docs/AI-CPP-GUIDE.md by Extract-Guide.ps1. Change the guide, not this file.
#pragma once
#include <Engine/AI/Ai.h>
// ---- guide block 1
// Game/MyGame/Source/GuardAi.h
#include <Engine/AI/Ai.h>

namespace Teams
{
inline constexpr Ai::Team Player{1};
inline constexpr Ai::Team Guards{2};
}

namespace GuardAi
{
// Ключ — это имя и тип. Положить в него значение другого типа не получится.
inline constexpr Ai::Key<Ai::ObjectRef> Target{"Target"};   // за кем гонимся
inline constexpr Ai::Key<glm::vec3> LastSeen{"LastSeen"};   // где видели в последний раз

Ai::MachinePtr Machine();
}
// ---- guide block 4
// Game/MyGame/Source/Guard.h
#include <Engine/Components/PropertyMacros.h>

ENGINE_COMPONENT("Стражник")
class Guard final : public AiCharacter
{
public:
    ENGINE_GENERATED_PROPERTIES(AiCharacter)

protected:
    void SetupAi(Ai::Controller& ai) override
    {
        const glm::vec3 home = GetWorldPosition();
        ai.SetRoute({home + glm::vec3(patrolLength, 0, 0), home});   // туда и обратно

        Ai::Perception& senses = ai.GetPerception();
        senses.SetTeam(Teams::Guards);
        senses.EnableSight(sightRadius, 120.0f);
        senses.WriteTargetTo(GuardAi::Target);
        senses.WriteLastSeenTo(GuardAi::LastSeen);

        ai.Run(GuardAi::Machine());
    }

private:
    ENGINE_PROPERTY("Радиус зрения", 1, 200)
    float sightRadius = 20;

    ENGINE_PROPERTY("Длина маршрута", 1, 1000)
    float patrolLength = 10;
};
// ---- guide block 5
// Game/MyGame/Source/Hero.h
ENGINE_COMPONENT("Герой")
class Hero final : public PlayerCharacter
{
public:
    void BeginPlay() override
    {
        PlayerCharacter::BeginPlay();
        Ai::Get(*GetGame()).AddTarget(*this, Teams::Player);
    }
};
// ---- guide block 6
struct SprintData { float normalSpeed = 0; };

class Sprint final : public Ai::Task<SprintData>
{
public:
    explicit Sprint(float factor) : factor_(factor) {}

    Ai::Status Enter(Ai::Context& ctx, SprintData& data) const override
    {
        auto* body = ctx.Owner<Character>();
        if (!body) return Ai::Status::Failed;
        data.normalSpeed = body->GetMoveSpeed();
        body->SetMoveSpeed(data.normalSpeed * factor_);
        return Ai::Status::Running;
    }

    void Exit(Ai::Context& ctx, SprintData& data, Ai::Status) const override
    {
        if (auto* body = ctx.Owner<Character>()) body->SetMoveSpeed(data.normalSpeed);
    }

private:
    float factor_;
};
// ---- guide block 7
inline constexpr Ai::Key<glm::vec3> CoverPoint{"CoverPoint"};

class RunToCover final : public Ai::Task<>
{
public:
    Ai::Status Enter(Ai::Context& ctx, Ai::NoData&) const override
    {
        ctx.Self().MoveTo(ctx.Memory().Get(CoverPoint), 1.0f);
        return Ai::Status::Running;
    }

    Ai::Status Tick(Ai::Context& ctx, Ai::NoData&, float) const override
    {
        switch (ctx.Self().GetMoveStatus())
        {
        case Ai::MoveStatus::Arrived:     return Ai::Status::Succeeded;
        case Ai::MoveStatus::Unreachable:
        case Ai::MoveStatus::Stuck:       return Ai::Status::Failed;
        default:                          return Ai::Status::Running;
        }
    }

    void Exit(Ai::Context& ctx, Ai::NoData&, Ai::Status result) const override
    {
        if (result == Ai::Status::Running) ctx.Self().StopMovement();   // прервали на ходу
    }
};
// ---- guide block 8
inline constexpr Ai::Key<float> Health{"Health"};

static bool IsWounded(Ai::Context& ctx) { return ctx.Memory().Get(Health, 100.0f) < 30; }
// ---- guide block 11
inline constexpr Ai::Event Alarm{"Alarm"};
// ---- guide block 14
inline constexpr Ai::Key<float> Danger{"Danger"};

class UpdateDanger final : public Ai::Service<>
{
public:
    void Tick(Ai::Context& ctx, Ai::NoData&, float) const override
    {
        ctx.Memory().Set(Danger, static_cast<float>(ctx.Self().GetPerception().Known().size()));
    }
};
// ---- guide block 19
static Ai::QueryPtr CoverQuery()
{
    Ai::QueryBuilder q("Cover");
    q.Points(Ai::Generators::Ring(6.0f, 18.0f, 24));              // кандидаты вокруг юнита
    q.Filter(Ai::Tests::Reachable());                             // можно дойти
    q.Filter(Ai::Tests::HiddenFrom(GuardAi::Target));             // враг не видит
    q.Score(Ai::Tests::Distance(GuardAi::Target), Ai::Prefer::Far);
    return q.Build();
}
// ---- guide block 22
#include <Engine/Components/SpecificComponents/CubeComponent.h>

Ai::MachinePtr TurretMachine();

ENGINE_COMPONENT("Турель")
class Turret final : public CubeComponent
{
public:
    void BeginPlay() override
    {
        CubeComponent::BeginPlay();
        Ai::Controller& ai = Ai::Get(*GetGame()).Spawn(*this);
        ai.GetPerception().EnableSight(30.0f, 360.0f);
        ai.Run(TurretMachine());
    }
};
// ---- guide block 23
Ai::MachinePtr BattleScript();
// ---- guide block 25
class GuardController final : public Ai::Controller
{
public:
    using Ai::Controller::Controller;

    void OnPerception(const Ai::Stimulus& stimulus) override
    {
        if (stimulus.kind == Ai::Stimulus::Kind::Damage) ++woundsTaken_;
    }

    void SerializeSaveGame(SaveArchive& archive) override
    {
        Ai::Controller::SerializeSaveGame(archive);
        archive.Field("woundsTaken", woundsTaken_);
    }

private:
    int woundsTaken_ = 0;
};

class EliteGuard final : public AiCharacter
{
protected:
    std::unique_ptr<Ai::Controller> CreateController(Ai::World& world) override
    {
        return std::make_unique<GuardController>(world);
    }
};
// ---- guide block 26
Ai::MachinePtr RegimentMachine();
// ---- guide block 29
#include <stdexcept>

inline void GuardStartsOnPatrol()
{
    Game game;
    std::unique_ptr<GameComponent> unit = std::make_unique<Guard>();
    const AddComponentResult added = game.GetScene().AddComponent("Guard", unit);

    game.InitializeGameplay();
    for (int frame = 0; frame < 10; ++frame) game.StepProjectSimulation(0.1f);

    const Ai::Controller* ai = Ai::Get(game).Find(added.id);
    if (!ai || ai->ActiveState() != "Patrol/Walk") throw std::runtime_error("Guard is not patrolling");
}
