// Acceptance: the code of docs/AI-CPP-GUIDE.md, extracted by Extract-Guide.ps1, compiled and run.
#include "GuideExamples.h"
#include "TestSupport.h"

// The guide only declares these machines.
Ai::MachinePtr CombatMachine()
{
    Ai::MachineBuilder m("Combat");
    m.State("Fight").Task<Ai::Tasks::Wait>(1.0f).GoTo(Ai::Target::Finish).OnSucceeded();
    return m.Build();
}
Ai::MachinePtr TurretMachine()
{
    Ai::MachineBuilder m("Turret");
    m.State("Watch").Task<Ai::Tasks::LookAt>(GuardAi::Target);
    return m.Build();
}
Ai::MachinePtr BattleScript()
{
    Ai::MachineBuilder m("Battle");
    m.State("Wave").Task<Ai::Tasks::Wait>(5.0f).GoTo(Ai::Target::Next).OnSucceeded();
    m.State("Final");
    return m.Build();
}
Ai::MachinePtr RegimentMachine()
{
    Ai::MachineBuilder m("Regiment");
    m.State("Hold");
    return m.Build();
}

namespace
{
bool Saw(const Ai::Controller& ai, const char* text)
{
    for (const auto& line : ai.History()) if (line.find(text) != std::string::npos) return true;
    return false;
}
template<class F> bool RunUntil(Game& game, float seconds, F&& done)
{
    for (float t = 0; t < seconds; t += 0.05f) { if (done()) return true; game.StepProjectSimulation(0.05f); }
    return done();
}
} // namespace

void GuideStartsOnPatrolTests() { GuardStartsOnPatrol(); }

void GuideGuardScenarioTests()
{
    Game game;
    Guard& guard = AddToScene<Guard>(game, "Guard", {0, 0, 0});
    Hero& hero = AddToScene<Hero>(game, "Hero", {0, 0, -60});
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    float damage = 0;
    world.SetDamageHandler([&](Ai::ObjectRef victim, Ai::ObjectRef, float amount) { if (victim.id == hero.GetComponentId()) damage += amount; });
    Ai::Controller& ai = *guard.GetAi();
    CHECK(ai.GetPerception().SightRadius() == 20.0f);       // the property reached SetupAi

    Step(game, 1.0f);
    CHECK(ai.ActiveState() == "Patrol/Walk");
    // The hero steps out in front of the guard, who walks along +X.
    hero.SetPosition(guard.GetWorldPosition() + glm::vec3(8, 0, 1));
    CHECK(RunUntil(game, 6, [&] { return ai.ActiveState() == "Combat/Hit"; }));
    CHECK(Saw(ai, "-> Combat/Chase   event Ai.Spotted"));
    Step(game, 1.5f);
    CHECK(damage >= 10);                                     // Attack reported its damage
    CHECK(ai.Memory().Get(GuardAi::Target).id == hero.GetComponentId());

    // The hero vanishes out of sight: the guard searches where it saw him last, then patrols again.
    const glm::vec3 lastSeen = hero.GetWorldPosition();
    hero.SetPosition({-50, 0, 40});
    CHECK(RunUntil(game, 3, [&] { return ai.ActiveState() == "Search"; }));
    CHECK(glm::length(ai.Memory().Get(GuardAi::LastSeen) - lastSeen) < 0.5f);
    CHECK(RunUntil(game, 15, [&] { return ai.ActiveState().rfind("Patrol/", 0) == 0; }));
    CHECK(glm::length(FlatOf(guard.GetWorldPosition()) - FlatOf(lastSeen)) < 2.5f);   // it went there before giving up
}
