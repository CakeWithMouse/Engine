// Stage 7: queries pick the best point: generators, filters, scores; the RunQuery task.
#include "TestSupport.h"

namespace
{
const Ai::Key<Ai::ObjectRef> Enemy{"Enemy"};
const Ai::Key<glm::vec3> Cover{"Cover"};
}

void QueryTests()
{
    Game game;
    NavVolume& bounds = AddToScene<NavVolume>(game, "Bounds");
    bounds.mode = 0;
    bounds.SetScale({60, 1, 60});
    // A wall east of the unit; the enemy stands further east, so only the west of the wall is hidden.
    NavVolume& wall = AddToScene<NavVolume>(game, "Wall", {4, 0, 0});
    wall.SetScale({1, 1, 12});
    NavVolume& pit = AddToScene<NavVolume>(game, "Closed", {-12, 0, 12});   // a closed block: never reachable
    pit.SetScale({4, 1, 4});
    Character& unit = AddToScene<Character>(game, "Unit", {0, 0, 0});
    Character& enemy = AddToScene<Character>(game, "Foe", {14, 0, 0});
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    Ai::Controller& ai = world.Spawn(unit);
    ai.Memory().Set(Enemy, Ai::ObjectRef{enemy.GetComponentId()});
    Ai::Context ctx(world, ai);
    game.StepProjectSimulation(0.05f);

    // Generators make the requested number of points.
    std::vector<glm::vec3> points;
    Ai::Generators::Ring(2, 10, 30).generate(ctx, points);
    CHECK(points.size() == 30);
    points.clear();
    Ai::Generators::Grid(4, 2).generate(ctx, points);
    CHECK(points.size() == 25);

    Ai::QueryBuilder q("Cover");
    q.Points(Ai::Generators::Ring(3.0f, 9.0f, 36));
    q.Filter(Ai::Tests::Reachable());
    q.Filter(Ai::Tests::HiddenFrom(Enemy));
    q.Score(Ai::Tests::Distance(Enemy), Ai::Prefer::Far);
    const Ai::QueryPtr cover = q.Build();
    const auto items = cover->Run(ctx);
    CHECK(!items.empty());
    for (const auto& item : items)
    {
        CHECK(!world.Navigation().HasLineOfSight(enemy.GetWorldPosition(), item.point));   // hidden
        CHECK(world.Navigation().IsWalkable({item.point.x, item.point.z}, ai.Radius()));    // reachable
    }
    for (size_t i = 1; i < items.size(); ++i) CHECK(items[i - 1].score >= items[i].score); // best first
    // Farthest from the enemy among hidden points: west of the unit.
    CHECK(items.front().point.x < -2);

    // Prefer Near flips the order.
    Ai::QueryBuilder closest("Near");
    closest.Points(Ai::Generators::Ring(3.0f, 9.0f, 36));
    closest.Score(Ai::Tests::Distance(Enemy), Ai::Prefer::Near);
    CHECK(closest.Build()->Run(ctx).front().point.x > 2);

    // Reachable drops points inside a closed block.
    Ai::QueryBuilder grid("Reachable grid");
    grid.Points(Ai::Generators::Grid(20, 2));
    grid.Filter(Ai::Tests::Reachable());
    for (const auto& item : grid.Build()->Run(ctx))
        CHECK(!(std::abs(item.point.x + 12) < 1.5f && std::abs(item.point.z - 12) < 1.5f));

    // The RunQuery task writes the best point to memory; nothing passing is a failure.
    Ai::MachineBuilder m("Hide");
    m.State("Find").Task<Ai::Tasks::RunQuery>(cover, Cover).GoTo("Found").OnSucceeded();
    m.State("Found");
    ai.Run(m.Build());
    Step(game, 0.2f);
    CHECK(ai.ActiveState() == "Found" && ai.Memory().Has(Cover));
    CHECK(glm::length(ai.Memory().Get(Cover) - items.front().point) < 1e-4f);

    Ai::QueryBuilder none("Nothing");
    none.Points(Ai::Generators::Ring(1, 2, 8));
    none.Filter(Ai::QueryTest{"never", [](Ai::Context&, glm::vec3) { return false; }, nullptr});
    Ai::MachineBuilder failing("Fail");
    failing.State("Find").Task<Ai::Tasks::RunQuery>(none.Build(), Cover).GoTo("Failed").OnFailed();
    failing.State("Failed");
    ai.Run(failing.Build());
    Step(game, 0.2f);
    CHECK(ai.ActiveState() == "Failed");

    // Mistakes are reported when the query is built.
    CHECK(Throws([] { Ai::QueryBuilder empty("Empty"); empty.Build(); }, "has no points"));
    CHECK(Throws([] { Ai::QueryBuilder b("B"); b.Filter(Ai::Tests::Distance(Enemy)); }, "cannot filter"));
    CHECK(Throws([] { Ai::QueryBuilder b("B"); b.Score(Ai::Tests::Reachable(), Ai::Prefer::Far); }, "cannot score"));
}
