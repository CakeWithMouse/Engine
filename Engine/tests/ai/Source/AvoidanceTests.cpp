// Stage 8: units keep apart, pass each other head-on, settle without jitter, go around standing ones.
#include "TestSupport.h"
#include <algorithm>

namespace
{
float MinimumGap(const std::vector<Character*>& units)
{
    float gap = 1e9f;
    for (size_t i = 0; i < units.size(); ++i)
        for (size_t j = i + 1; j < units.size(); ++j)
            gap = std::min(gap, glm::length(FlatOf(units[i]->GetWorldPosition()) - FlatOf(units[j]->GetWorldPosition())));
    return gap;
}
} // namespace

void HeadOnTests()
{
    Game game;
    Character& a = AddToScene<Character>(game, "A", {-6, 0, 0});
    Character& b = AddToScene<Character>(game, "B", {6, 0, 0});
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    Ai::Controller& ca = world.Spawn(a);
    Ai::Controller& cb = world.Spawn(b);
    ca.MoveTo(glm::vec3(6, 0, 0), 0.3f);
    cb.MoveTo(glm::vec3(-6, 0, 0), 0.3f);
    float closest = 1e9f;
    for (int frame = 0; frame < 160; ++frame)
    {
        game.StepProjectSimulation(0.05f);
        closest = std::min(closest, MinimumGap({&a, &b}));
    }
    CHECK(ca.GetMoveStatus() == Ai::MoveStatus::Arrived && cb.GetMoveStatus() == Ai::MoveStatus::Arrived);
    CHECK(closest > 0.6f);                                      // radius 0.4 each: they never really overlapped
}

void CrowdToOnePointTests()
{
    Game game;
    std::vector<Character*> units;
    for (int i = 0; i < 12; ++i)
        units.push_back(&AddToScene<Character>(game, "U" + std::to_string(i), {static_cast<float>(i % 4) * 2 - 3, 0, static_cast<float>(i / 4) * 2 - 12}));
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    std::vector<Ai::Controller*> ais;
    for (Character* u : units) { ais.push_back(&world.Spawn(*u)); ais.back()->MoveTo(glm::vec3(0, 0, 8), 0.5f); }
    Step(game, 12.0f);
    CHECK(MinimumGap(units) > 0.6f);                            // nobody stands inside another
    for (auto* c : ais) CHECK(c->GetMoveStatus() == Ai::MoveStatus::Arrived || c->GetMoveStatus() == Ai::MoveStatus::Stuck);
    // Settled: nobody keeps twitching.
    std::vector<glm::vec3> before;
    for (Character* u : units) before.push_back(u->GetWorldPosition());
    Step(game, 1.0f);
    for (size_t i = 0; i < units.size(); ++i) CHECK(glm::length(units[i]->GetWorldPosition() - before[i]) < 0.05f);
    // Everyone got close to the point.
    for (Character* u : units) CHECK(glm::length(FlatOf(u->GetWorldPosition()) - glm::vec2(0, 8)) < 3.0f);
}

void StandingUnitIsNotPushedTests()
{
    Game game;
    Character& sentry = AddToScene<Character>(game, "Sentry", {0, 0, 0});
    Character& walker = AddToScene<Character>(game, "Walker", {0, 0, -6});
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    world.Spawn(sentry);                                        // has a controller, stands still
    Ai::Controller& w = world.Spawn(walker);
    w.MoveTo(glm::vec3(0, 0, 6), 0.3f);
    float closest = 1e9f;
    for (int frame = 0; frame < 120; ++frame)
    {
        game.StepProjectSimulation(0.05f);
        closest = std::min(closest, MinimumGap({&sentry, &walker}));
    }
    CHECK(w.GetMoveStatus() == Ai::MoveStatus::Arrived);
    CHECK(glm::length(sentry.GetWorldPosition()) < 1e-4f);     // not moved an inch
    CHECK(closest > 0.6f);                                      // went around it
}
