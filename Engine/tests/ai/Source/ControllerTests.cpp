// Stage 3: controllers in a real scene of a headless Game; the built-in subsystem drives them.
#include "TestSupport.h"
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <cmath>

namespace
{
const Ai::Key<glm::vec3> Goal{"Goal"};
const Ai::Key<Ai::ObjectRef> Target{"Target"};

// A character whose machine is given by the test.
Ai::MachinePtr nextMachine;
class Walker : public AiCharacter
{
public:
    std::vector<glm::vec3> route;
protected:
    void SetupAi(Ai::Controller& ai) override
    {
        if (!route.empty()) ai.SetRoute(route);
        if (nextMachine) ai.Run(nextMachine);
    }
};

// Its body refuses to move: the controller must notice and report Stuck.
class Blocked final : public Walker
{
public:
    void Move(const glm::vec3&, float) override {}
};

// Walks through every cell it occupies: fails the test if it ever stands in a blocked one.
void RequireFree(Ai::World& world, Character& unit, float radius)
{
    const glm::vec3 p = unit.GetWorldPosition();
    if (!world.Navigation().IsWalkable({p.x, p.z}, radius * 0.5f))
        throw std::runtime_error("unit entered a blocked cell at (" + std::to_string(p.x) + ", " + std::to_string(p.z) + ")");
}

Ai::MachinePtr Machine(std::function<void(Ai::MachineBuilder&)> describe)
{
    Ai::MachineBuilder m("Test");
    describe(m);
    return m.Build();
}
} // namespace

void SubsystemAvailabilityTests()
{
    {
        Game edit;
        CHECK(Throws([&] { Ai::Get(edit); }, "only during gameplay"));
    }
    Game game;
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    CHECK(&world == &Ai::Get(game));
    CHECK(world.GetScene() == &game.GetScene());
    game.StepProjectSimulation(0.05f);                        // an empty world costs nothing and does not fail
    CHECK(world.GetStats().controllers == 0);
}

void RouteAndRestTests()
{
    Game game;
    nextMachine = Machine([](Ai::MachineBuilder& m) {
        auto& patrol = m.State("Patrol");
        patrol.State("Walk").Task<Ai::Tasks::FollowRoute>().GoTo("Rest").OnSucceeded();
        patrol.State("Rest").Task<Ai::Tasks::Wait>(1.0f).GoTo("Walk").OnCompleted();
    });
    Walker& guard = AddToScene<Walker>(game, "Guard", {0, 0, 0});
    guard.route = {{0, 0, 0}, {6, 0, 0}};
    guard.SetMoveSpeed(3);
    game.InitializeGameplay();
    Ai::Controller* ai = guard.GetAi();
    CHECK(ai && ai->Route().size() == 2);
    // Point 0 is where it stands: Walk ends at once, Rest 1 s, then six metres at 3 m/s.
    float farthest = 0, forwardOnTheWay = 0;
    for (int frame = 0; frame < 100; ++frame)
    {
        game.StepProjectSimulation(0.05f);
        const glm::vec3 p = guard.GetWorldPosition();
        if (p.x > 3 && forwardOnTheWay == 0) forwardOnTheWay = (guard.GetLocalRotationQuat() * glm::vec3(0, 0, 1)).x;
        farthest = std::max(farthest, p.x);
    }
    CHECK(farthest > 5.4f && farthest < 6.2f);              // reached point 1 and did not overshoot
    CHECK(forwardOnTheWay > 0.9f);                         // faced +X while walking there
    int rests = 0;
    for (const auto& line : ai->History()) rests += line.find("-> Patrol/Rest") != std::string::npos;
    CHECK(rests >= 2);
    CHECK(ai->ActiveState().rfind("Patrol/", 0) == 0);
}

void PathAroundVolumesTests()
{
    Game game;
    // Bounds 30 x 30 around the origin, a wall at x = 0 from z = -15 to z = 10.
    NavVolume& bounds = AddToScene<NavVolume>(game, "Bounds", {0, 0, 0});
    bounds.mode = 0;
    bounds.cellSize = 0.5f;
    bounds.SetScale({30, 1, 30});
    NavVolume& wall = AddToScene<NavVolume>(game, "Wall", {0, 0, -2.5f});
    wall.mode = 1;
    wall.SetScale({1, 1, 25});
    nextMachine = Machine([](Ai::MachineBuilder& m) {
        m.State("Go").Task<Ai::Tasks::MoveTo>(Goal, 0.5f).GoTo("Done").OnSucceeded();
        m.State("Done");
    });
    Walker& unit = AddToScene<Walker>(game, "Unit", {-8, 0, -8});
    unit.SetMoveSpeed(5);
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    CHECK(world.Navigation().IsConfigured() && world.Navigation().Width() == 60);
    CHECK(!world.Navigation().IsWalkable({0, -5}, 0.1f));
    Ai::Controller& ai = *unit.GetAi();
    ai.Memory().Set(Goal, glm::vec3(8, 0, -8));
    for (int frame = 0; frame < 400 && ai.ActiveState() != "Done"; ++frame)
    {
        game.StepProjectSimulation(0.05f);
        RequireFree(world, unit, ai.Radius());
    }
    CHECK(ai.ActiveState() == "Done");
    CHECK(glm::length(FlatOf(unit.GetWorldPosition()) - glm::vec2(8, -8)) <= 0.6f);
}

void ChaseMovingTargetTests()
{
    Game game;
    nextMachine = Machine([](Ai::MachineBuilder& m) {
        m.State("Chase").Task<Ai::Tasks::MoveTo>(Target, 1.0f).GoTo("Caught").OnSucceeded();
        m.State("Caught");
    });
    Walker& hunter = AddToScene<Walker>(game, "Hunter", {0, 0, 0});
    hunter.SetMoveSpeed(4);
    Character& prey = AddToScene<Character>(game, "Prey", {10, 0, 0});
    game.InitializeGameplay();
    Ai::Controller& ai = *hunter.GetAi();
    ai.Memory().Set(Target, Ai::ObjectRef{prey.GetComponentId()});
    for (int frame = 0; frame < 200 && ai.ActiveState() != "Caught"; ++frame)
    {
        prey.SetPosition(prey.GetLocalPosition() + glm::vec3(0, 0, 0.05f));   // runs away at 1 m/s
        game.StepProjectSimulation(0.05f);
    }
    CHECK(ai.ActiveState() == "Caught");
    CHECK(glm::length(FlatOf(hunter.GetWorldPosition()) - FlatOf(prey.GetWorldPosition())) <= 1.3f);
    CHECK(prey.GetWorldPosition().z > 2);                                      // the target really moved
}

void UnreachableAndStuckTests()
{
    Game game;
    NavVolume& bounds = AddToScene<NavVolume>(game, "Bounds");
    bounds.mode = 0;
    bounds.SetScale({20, 1, 20});
    for (const glm::vec3 side : {glm::vec3(5, 0, 3), glm::vec3(5, 0, 7), glm::vec3(3, 0, 5), glm::vec3(7, 0, 5)})
    {
        NavVolume& piece = AddToScene<NavVolume>(game, "Room" + std::to_string(side.x) + std::to_string(side.z), side);
        piece.SetScale(side.x == 5 ? glm::vec3(5, 1, 1) : glm::vec3(1, 1, 5));
    }
    nextMachine = Machine([](Ai::MachineBuilder& m) {
        m.State("Go").Task<Ai::Tasks::MoveTo>(Goal, 0.3f).GoTo("Failed").OnFailed();
        m.State("Failed");
    });
    Walker& outside = AddToScene<Walker>(game, "Outside", {-5, 0, -5});
    outside.SetMoveSpeed(6);
    game.InitializeGameplay();
    Ai::Controller& ai = *outside.GetAi();
    ai.Memory().Set(Goal, glm::vec3(5, 0, 5));           // inside a closed room
    for (int frame = 0; frame < 400 && ai.ActiveState() != "Failed"; ++frame) game.StepProjectSimulation(0.05f);
    CHECK(ai.ActiveState() == "Failed");
    CHECK(ai.GetMoveStatus() == Ai::MoveStatus::Unreachable);
    CHECK(glm::length(FlatOf(outside.GetWorldPosition()) - glm::vec2(5, 5)) < 4);   // went as close as possible

    // A body that cannot move: three checks without progress give Stuck.
    Game second;
    nextMachine = Machine([](Ai::MachineBuilder& m) {
        m.State("Go").Task<Ai::Tasks::MoveTo>(Goal, 0.3f).GoTo("Failed").OnFailed();
        m.State("Failed");
    });
    Blocked& blocked = AddToScene<Blocked>(second, "Blocked");
    second.InitializeGameplay();
    Ai::Controller& stuck = *blocked.GetAi();
    stuck.Memory().Set(Goal, glm::vec3(10, 0, 0));
    for (int frame = 0; frame < 200 && stuck.ActiveState() != "Failed"; ++frame) second.StepProjectSimulation(0.05f);
    CHECK(stuck.ActiveState() == "Failed" && stuck.GetMoveStatus() == Ai::MoveStatus::Stuck);
}

void BodyRemovedTests()
{
    Game game;
    nextMachine = Machine([](Ai::MachineBuilder& m) { m.State("Wait").Task<Ai::Tasks::Wait>(100.0f); });
    Walker& unit = AddToScene<Walker>(game, "Unit");
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    Step(game, 0.1f);
    CHECK(world.Find(unit.GetComponentId()) != nullptr);
    const ComponentId id = unit.GetComponentId();
    CHECK(game.GetScene().RemoveComponent(id) == SceneResult::Ok);
    Step(game, 0.1f);
    CHECK(world.Find(id) == nullptr && world.GetStats().controllers == 0);
}

void TurretAndPlayerTests()
{
    class Turret final : public CubeComponent
    {
    public:
        Ai::Controller* ai = nullptr;
        void BeginPlay() override
        {
            CubeComponent::BeginPlay();
            ai = &Ai::Get(*GetGame()).Spawn(*this);
        }
    };
    Game game;
    Turret& turret = AddToScene<Turret>(game, "Turret");
    PlayerCharacter& player = AddToScene<PlayerCharacter>(game, "Player", {0, 0, 5});
    AiCharacter& npcPlayer = AddToScene<AiCharacter>(game, "NpcPlayer", {3, 0, 3});
    npcPlayer.SetPlayerControlled(false);
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    CHECK(turret.ai && !turret.ai->CanMove());
    turret.ai->MoveTo(glm::vec3(5, 0, 5));
    CHECK(turret.ai->GetMoveStatus() == Ai::MoveStatus::Unavailable);
    turret.ai->SetFocus({player.GetComponentId()});
    Step(game, 1.0f);
    const glm::vec3 forward = turret.GetLocalRotationQuat() * glm::vec3(0, 0, 1);
    CHECK(forward.z > 0.95f);                                     // turned to the player at +Z
    CHECK(world.Find(player.GetComponentId()) == nullptr);        // the player is not an AI unit
    CHECK(npcPlayer.GetAi() != nullptr);
    CHECK(Throws([&] { world.Spawn(turret); }, "already has an AI controller"));
}
