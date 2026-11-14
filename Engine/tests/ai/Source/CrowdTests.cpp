// Stage 9: squads of MeshInstance soldiers: one flow field, slots, no overlap, machines, teams.
#include "TestSupport.h"
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <algorithm>

namespace
{
constexpr float Pi = 3.14159265f;

InstancedMeshComponent& Batch(Game& game, const std::string& name, glm::vec2 corner, int columns, int rows, float spacing)
{
    InstancedMeshComponent& batch = AddToScene<InstancedMeshComponent>(game, name);
    for (int z = 0; z < rows; ++z)
        for (int x = 0; x < columns; ++x)
        {
            MeshInstance instance;
            instance.Position = glm::vec3(corner.x + x * spacing, 0, corner.y + z * spacing);
            batch.AddInstance(instance);
        }
    return batch;
}

float MinimumGap(const Ai::Group& group)
{
    float gap = 1e9f;
    const auto& m = group.Members();
    for (size_t i = 0; i < m.size(); ++i)
        for (size_t j = i + 1; j < m.size(); ++j)
            if (m[i].alive && m[j].alive) gap = std::min(gap, glm::length(m[i].position - m[j].position));
    return gap;
}
} // namespace

void SquadThroughGapTests()
{
    Game game;
    NavVolume& bounds = AddToScene<NavVolume>(game, "Bounds");
    bounds.mode = 0;
    bounds.SetScale({200, 1, 200});
    NavVolume& south = AddToScene<NavVolume>(game, "WallSouth", {0, 0, -53});   // a wall at x = 0, open for |z| < 6
    south.SetScale({2, 1, 94});
    NavVolume& north = AddToScene<NavVolume>(game, "WallNorth", {0, 0, 53});
    north.SetScale({2, 1, 94});
    InstancedMeshComponent& batch = Batch(game, "Tanks", {-60, -9}, 10, 10, 2);
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    Ai::Group& squad = world.CreateGroup("Regiment");
    for (size_t i = 0; i < 100; ++i) squad.AddInstance(batch.GetComponentId(), i, 0.8f, 6);
    squad.SetFormation({2.5f, 10});
    squad.SetFacing(Pi / 2);                                     // the new front looks along +X
    squad.MoveTo(glm::vec3(50, 0, 0), 3);
    uint32_t fields = 0;
    for (int frame = 0; frame < 1200 && squad.GetMoveStatus() != Ai::MoveStatus::Arrived; ++frame)
    {
        game.StepProjectSimulation(0.05f);
        fields += world.GetStats().fieldsFinished;
        for (const auto& m : squad.Members())
            if (!world.Navigation().IsWalkable(m.position, m.radius * 0.5f)) throw std::runtime_error("a soldier entered the wall");
    }
    CHECK(squad.GetMoveStatus() == Ai::MoveStatus::Arrived);
    CHECK(fields == 1);                                          // one field for a hundred soldiers
    CHECK(world.GetStats().soldiers == 100);
    size_t across = 0;
    for (const auto& m : squad.Members()) across += m.position.x > 30;
    CHECK(across >= 95);                                         // arrival needs 95%; they really crossed
    // Two seconds to settle, then nobody stands inside another and nobody twitches.
    Step(game, 2.0f);
    std::vector<glm::vec2> before;
    for (const auto& m : squad.Members()) before.push_back(m.position);
    Step(game, 1.0f);
    const float gap = MinimumGap(squad);
    if (!(gap > 1.5f)) throw std::runtime_error("soldiers overlap: closest pair " + std::to_string(gap) + " m apart");   // radius 0.8
    for (size_t i = 0; i < before.size(); ++i) CHECK(glm::length(squad.Members()[i].position - before[i]) < 0.05f);
    // The bodies are the instances: positions and yaw were written into the component.
    const auto& first = squad.Members().front();
    CHECK(glm::length(FlatOf(batch.GetMeshInstance(first.index).Position) - first.position) < 1e-3f);
    CHECK(std::abs(batch.GetMeshInstance(first.index).Yaw - Pi / 2) < 0.3f || !first.settled);
}

// The same gap at 60 FPS: soldiers choose their velocity every fourth frame, in turns.
void StaggeredReplanTests()
{
    Game game;
    NavVolume& bounds = AddToScene<NavVolume>(game, "Bounds");
    bounds.mode = 0;
    bounds.SetScale({200, 1, 200});
    NavVolume& south = AddToScene<NavVolume>(game, "WallSouth", {0, 0, -53});
    south.SetScale({2, 1, 94});
    NavVolume& north = AddToScene<NavVolume>(game, "WallNorth", {0, 0, 53});
    north.SetScale({2, 1, 94});
    InstancedMeshComponent& batch = Batch(game, "Tanks", {-60, -9}, 10, 10, 2);
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    Ai::Group& squad = world.CreateGroup("Regiment");
    for (size_t i = 0; i < 100; ++i) squad.AddInstance(batch.GetComponentId(), i, 0.8f, 6);
    squad.SetFormation({2.5f, 10});
    squad.SetFacing(Pi / 2);
    squad.MoveTo(glm::vec3(50, 0, 0), 3);
    const float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < 3600 && squad.GetMoveStatus() != Ai::MoveStatus::Arrived; ++frame)
    {
        game.StepProjectSimulation(dt);
        for (const auto& m : squad.Members())
            if (!world.Navigation().IsWalkable(m.position, m.radius * 0.5f)) throw std::runtime_error("a soldier entered the wall");
    }
    CHECK(world.CrowdStride() == 4);
    CHECK(squad.GetMoveStatus() == Ai::MoveStatus::Arrived);
    size_t across = 0;
    for (const auto& m : squad.Members()) across += m.position.x > 30;
    CHECK(across >= 95);
    Step(game, 2.0f, dt);
    std::vector<glm::vec2> before;
    for (const auto& m : squad.Members()) before.push_back(m.position);
    Step(game, 1.0f, dt);
    const float gap = MinimumGap(squad);
    if (!(gap > 1.5f)) throw std::runtime_error("soldiers overlap: closest pair " + std::to_string(gap) + " m apart");
    for (size_t i = 0; i < before.size(); ++i) CHECK(glm::length(squad.Members()[i].position - before[i]) < 0.05f);
}

void SharedFieldAndOpenFieldTests()
{
    Game game;
    NavVolume& bounds = AddToScene<NavVolume>(game, "Bounds");
    bounds.mode = 0;
    bounds.SetScale({300, 1, 300});
    InstancedMeshComponent& a = Batch(game, "A", {-100, -10}, 5, 4, 3);
    InstancedMeshComponent& b = Batch(game, "B", {-100, 20}, 5, 4, 3);
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    Ai::Group& first = world.CreateGroup("First");
    Ai::Group& second = world.CreateGroup("Second");
    for (size_t i = 0; i < 20; ++i) { first.AddInstance(a.GetComponentId(), i, 1, 5); second.AddInstance(b.GetComponentId(), i, 1, 5); }
    first.MoveTo(glm::vec3(80, 0, 0), 3);
    second.MoveTo(glm::vec3(83, 0, 4), 3);                       // a nearby goal: the same coarse cell
    CHECK(world.Fields().Size() == 1);

    // Without a grid a squad walks straight.
    Game open;
    InstancedMeshComponent& c = Batch(open, "C", {0, 0}, 4, 4, 3);
    open.InitializeGameplay();
    Ai::World& openWorld = Ai::Get(open);
    Ai::Group& squad = openWorld.CreateGroup();
    for (size_t i = 0; i < 16; ++i) squad.AddInstance(c.GetComponentId(), i, 1, 5);
    squad.MoveTo(glm::vec3(40, 0, 40), 2);
    Step(open, 20.0f);
    CHECK(squad.GetMoveStatus() == Ai::MoveStatus::Arrived);
    CHECK(glm::length(FlatOf(squad.Position()) - glm::vec2(40, 40)) < 3);
}

void SquadMachineAndTeamsTests()
{
    Game game;
    InstancedMeshComponent& red = Batch(game, "Red", {0, 0}, 5, 5, 3);
    InstancedMeshComponent& blue = Batch(game, "Blue", {0, 60}, 5, 5, 3);
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    Ai::Group& left = world.CreateGroup("Left");
    Ai::Group& right = world.CreateGroup("Right");
    for (size_t i = 0; i < 25; ++i) { left.AddInstance(red.GetComponentId(), i, 1, 6); right.AddInstance(blue.GetComponentId(), i, 1, 6); }
    left.GetPerception().SetTeam({1});
    right.GetPerception().SetTeam({2});
    game.StepProjectSimulation(0.05f);
    CHECK(left.FindHostile(100) == &right && left.FindHostile(30) == nullptr);

    // The same standard task drives a squad: MoveTo uses the squad's flow, not A*.
    const Ai::Key<glm::vec3> Objective{"Objective"};
    Ai::MachineBuilder m("Regiment");
    m.State("March").Task<Ai::Tasks::MoveTo>(Objective, 4.0f).GoTo("Hold").OnSucceeded();
    m.State("Hold");
    left.Memory().Set(Objective, glm::vec3(6, 0, 40));
    left.Run(m.Build());
    for (int frame = 0; frame < 600 && left.ActiveState() != "Hold"; ++frame) game.StepProjectSimulation(0.05f);
    CHECK(left.ActiveState() == "Hold");
    CHECK(left.FindHostile(30) == &right);                       // now close to the enemy

    // A dead soldier is hidden and no longer counted.
    left.RemoveMember(3);
    CHECK(left.Size() == 24 && !red.GetMeshInstance(left.Members()[3].index).bVisible);
}

void WindowedFieldTests()
{
    // A short march on a big map pays for the cells around it, not for the map.
    {
        Game game;
        NavVolume& bounds = AddToScene<NavVolume>(game, "Bounds");
        bounds.mode = 0;
        bounds.cellSize = 2;
        bounds.SetScale({600, 1, 600});
        InstancedMeshComponent& batch = Batch(game, "Batch", {-10, -10}, 5, 5, 3);
        game.InitializeGameplay();
        Ai::World& world = Ai::Get(game);
        Ai::Group& squad = world.CreateGroup();
        for (size_t i = 0; i < 25; ++i) squad.AddInstance(batch.GetComponentId(), i, 1, 6);
        squad.MoveTo(glm::vec3(50, 0, 0), 3);
        const size_t grid = static_cast<size_t>(world.Navigation().Width()) * world.Navigation().Height();
        CHECK(world.Fields().Cells() * 5 < grid);           // under a fifth of the map
        for (int frame = 0; frame < 600 && squad.GetMoveStatus() != Ai::MoveStatus::Arrived; ++frame) game.StepProjectSimulation(0.05f);
        CHECK(squad.GetMoveStatus() == Ai::MoveStatus::Arrived);
    }
    // The only way round a long wall leaves the window: the squad widens it and still arrives.
    {
        Game game;
        NavVolume& bounds = AddToScene<NavVolume>(game, "Bounds");
        bounds.mode = 0;
        bounds.cellSize = 2;
        bounds.SetScale({400, 1, 400});
        NavVolume& wall = AddToScene<NavVolume>(game, "Wall", {0, 0, 10});    // z from -200 to 180: open only near z = 190
        wall.SetScale({2, 1, 380});
        InstancedMeshComponent& batch = Batch(game, "Batch", {-40, -6}, 5, 5, 3);
        game.InitializeGameplay();
        Ai::World& world = Ai::Get(game);
        Ai::Group& squad = world.CreateGroup();
        for (size_t i = 0; i < 25; ++i) squad.AddInstance(batch.GetComponentId(), i, 1, 8);
        squad.MoveTo(glm::vec3(30, 0, 0), 3);
        uint32_t fields = 0;
        for (int frame = 0; frame < 2400 && squad.GetMoveStatus() != Ai::MoveStatus::Arrived; ++frame)
        {
            game.StepProjectSimulation(0.05f);
            fields += world.GetStats().fieldsFinished;
        }
        CHECK(squad.GetMoveStatus() == Ai::MoveStatus::Arrived);
        CHECK(fields >= 2);                                   // the first window was too small
        CHECK(glm::length(FlatOf(squad.Position()) - glm::vec2(30, 0)) < 8);
    }
}
