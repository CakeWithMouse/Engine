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

// An ObjectRef order must keep tracking its object until it finishes or is cancelled.
void GroupMovingTargetTests()
{
    for (bool navigation : {false, true})
    {
        Game game;
        auto& soldier = AddToScene<Character>(game, "Soldier", {-8, 0, 0});
        soldier.SetMoveSpeed(4);
        auto& target = AddToScene<Character>(game, "Target", {3, 0, 0});
        const auto targetId = target.GetComponentId();
        game.InitializeGameplay();
        auto& world = Ai::Get(game);
        if (navigation) world.Navigation().SetBounds({-20, -20}, {20, 20}, 0.5f);
        auto& group = world.CreateGroup("Hunters");
        group.AddCharacter(soldier);
        group.MoveTo(Ai::ObjectRef{targetId});
        for (int i = 0; i < 60; ++i)
        {
            target.SetPosition({3, 0, 0.05f * (i + 1)});
            // Dynamic obstacles elsewhere must not make us forget which position the slots use.
            if (navigation && i % 10 == 0)
                world.Navigation().SetCost({15, 15}, {16, 16}, i % 20 == 0 ? 2 : 1);
            game.StepProjectSimulation(0.05f);
        }
        CHECK(glm::length(FlatOf(group.Destination()) - FlatOf(target.GetWorldPosition())) < 0.1f);
        Step(game, 5);
        CHECK(group.GetMoveStatus() == Ai::MoveStatus::Arrived);
        CHECK(glm::length(FlatOf(soldier.GetWorldPosition()) - FlatOf(target.GetWorldPosition())) < 0.15f);

        // Replacing a chase with a point order must forget the target.
        target.SetPosition({8, 0, 8});
        group.MoveTo(Ai::ObjectRef{targetId});
        Step(game, 0.2f);
        const glm::vec3 point(-3, 0, -3);
        group.MoveTo(point);
        target.SetPosition({-10, 0, 10});
        Step(game, 8);
        CHECK(group.GetMoveStatus() == Ai::MoveStatus::Arrived);
        CHECK(glm::length(FlatOf(soldier.GetWorldPosition()) - FlatOf(point)) < 0.15f);

        // An object removed during an active chase invalidates the order immediately.
        group.MoveTo(Ai::ObjectRef{targetId});
        Step(game, 0.2f);
        CHECK(game.GetScene().RemoveComponent(targetId) == SceneResult::Ok);
        Step(game, 0.1f);
        CHECK(group.GetMoveStatus() == Ai::MoveStatus::Unreachable);
        const auto stopped = soldier.GetWorldPosition();
        Step(game, 1);
        CHECK(glm::length(soldier.GetWorldPosition() - stopped) < 0.001f);
    }
}

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

// Registering a group member as a perception target must not make it avoid itself.
void GroupCharacterTargetTests()
{
    auto trajectory = [](bool target) {
        Game game;
        auto& soldier = AddToScene<Character>(game, "Soldier", {-3, 0, 0});
        soldier.SetMoveSpeed(3);
        game.InitializeGameplay();
        auto& world = Ai::Get(game);
        if (target) world.AddTarget(soldier, {1});
        auto& group = world.CreateGroup("Selected");
        group.GetPerception().SetTeam({1});
        group.AddCharacter(soldier, 0.4f);
        group.MoveTo(glm::vec3(3, 0, 0));
        std::vector<glm::vec3> positions;
        for (int i = 0; i < 30; ++i) {
            game.StepProjectSimulation(0.05f);
            positions.push_back(soldier.GetWorldPosition());
        }
        if (target) {
            // Disbanding leaves the registered target visible to senses and avoidance.
            world.Destroy(group);
            game.StepProjectSimulation(0.05f);
            CHECK(world.IsTarget(soldier.GetComponentId()));
            const auto& entries = world.Neighbours().Entries();
            CHECK(std::count_if(entries.begin(), entries.end(), [&](const auto& entry) {
                return entry.object == soldier.GetComponentId() && entry.controller == 0;
            }) == 1);
        }
        return positions;
    };
    const auto plain = trajectory(false), target = trajectory(true);
    for (size_t i = 0; i < plain.size(); ++i) {
        const float difference = glm::length(plain[i] - target[i]);
        if (difference > 1e-4f)
            throw std::runtime_error("perception target changes its own group movement at frame " +
                std::to_string(i) + ": difference=" + std::to_string(difference));
    }
}

void GroupUnreachableGoalTests()
{
    Game game;
    auto& soldier = AddToScene<Character>(game, "Soldier", {-5, 0, 0});
    soldier.SetMoveSpeed(3);
    game.InitializeGameplay();
    auto& world = Ai::Get(game);
    auto& grid = world.Navigation();
    grid.SetBounds({-10, -10}, {10, 10}, 0.5f);
    grid.BlockBox({-1, -10}, {1, 10});
    auto& group = world.CreateGroup("Selected");
    group.AddCharacter(soldier, 0.4f);
    // Cache a reachable goal in the same coarse region, across the wall from the next goal.
    group.MoveTo(glm::vec3(-5, 0, -1));
    Step(game, 2);
    CHECK(group.GetMoveStatus() == Ai::MoveStatus::Arrived);
    group.MoveTo(glm::vec3(5, 0, 0));
    Step(game, 5);
    CHECK(soldier.GetWorldPosition().x < -1);
    CHECK(group.GetMoveStatus() == Ai::MoveStatus::Unreachable);
    const auto stopped = soldier.GetWorldPosition();
    Step(game, 0.5f);
    CHECK(glm::length(soldier.GetWorldPosition() - stopped) < 1e-4f);
    // An opened passage and a new order must recover from the failed order.
    grid.ClearBox({-1, -2}, {1, 2});
    group.MoveTo(glm::vec3(5, 0, 0));
    Step(game, 10);
    CHECK(group.GetMoveStatus() == Ai::MoveStatus::Arrived);
    CHECK(glm::length(soldier.GetWorldPosition() - glm::vec3(5, 0, 0)) < 0.4f);
}

void SmallGroupDetourTests()
{
    Game game;
    std::vector<Character*> soldiers;
    for (int i = 0; i < 3; ++i) {
        auto& soldier = AddToScene<Character>(game, "Soldier" + std::to_string(i), {-1.5f + i * 1.5f, 0, -6});
        soldier.SetMoveSpeed(3);
        soldiers.push_back(&soldier);
    }
    game.InitializeGameplay();
    auto& world = Ai::Get(game);
    auto& grid = world.Navigation();
    grid.SetBounds({-3, -10.8f}, {3, 10.8f}, 0.25f);
    grid.BlockBox({-3, 1.1f}, {-1, 2.9f});
    grid.BlockBox({1, 1.1f}, {3, 2.9f});
    grid.BlockBox({-0.9f, 1.3f}, {0.9f, 2.7f});
    grid.BlockBox({-1.2f, -2.35f}, {1.2f, -1.65f});
    auto& group = world.CreateGroup("Selected");
    group.GetPerception().SetTeam({1});
    group.SetFacing(Pi / 2);
    group.SetFormation({0.9f, 1});
    for (auto* soldier : soldiers) {
        world.AddTarget(*soldier, {1});
        group.AddCharacter(*soldier, 0.4f);
    }
    group.MoveTo(glm::vec3(0, 0, -0.2625f));
    for (int frame = 0; frame < 1200 && group.GetMoveStatus() != Ai::MoveStatus::Arrived; ++frame) {
        game.StepProjectSimulation(0.016f);
        for (auto* soldier : soldiers) {
            const auto p = FlatOf(soldier->GetWorldPosition());
            const auto nearest = glm::clamp(p, glm::vec2(-1.2f, -2.35f), glm::vec2(1.2f, -1.65f));
            CHECK(glm::length(p - nearest) >= 0.399f);
        }
    }
    if (group.GetMoveStatus() != Ai::MoveStatus::Arrived) {
        std::string positions;
        for (const auto& m : group.Members()) positions += " [" + std::to_string(m.position.x) + "," + std::to_string(m.position.y) + "]";
        throw std::runtime_error("small group did not finish its detour:" + positions);
    }
    CHECK(MinimumGap(group) >= 0.79f);
}

void SmallGroupFormationChangeTests()
{
    Game game;
    std::vector<Character*> soldiers;
    for (int i = 0; i < 3; ++i) {
        auto& soldier = AddToScene<Character>(game, "Soldier" + std::to_string(i), {-1.5f + i * 1.5f, 0, -6});
        soldier.SetMoveSpeed(3);
        soldiers.push_back(&soldier);
    }
    game.InitializeGameplay();
    auto& world = Ai::Get(game);
    auto& grid = world.Navigation();
    grid.SetBounds({-3, -10.8f}, {3, 10.8f}, .25f);
    grid.BlockBox({-3, 1.1f}, {-1, 2.9f});
    grid.BlockBox({1, 1.1f}, {3, 2.9f});
    grid.BlockBox({-1.2f, -2.35f}, {1.2f, -1.65f});
    auto& group = world.CreateGroup("Formation change");
    group.SetFormation({.9f, 3});
    for (auto* soldier : soldiers) group.AddCharacter(*soldier, .4f);
    group.MoveTo(glm::vec3(0, 0, -1.04167f));
    Step(game, 96 * .016f, .016f);
    group.SetFormation({.9f, 1});
    group.MoveTo(glm::vec3(0, 0, -.354167f));
    for (int frame = 0; frame < 1800 && group.GetMoveStatus() != Ai::MoveStatus::Arrived; ++frame) {
        game.StepProjectSimulation(.016f);
        for (const auto& m : group.Members()) {
            const auto nearest = glm::clamp(m.position, glm::vec2(-1.2f, -2.35f), glm::vec2(1.2f, -1.65f));
            CHECK(glm::length(m.position - nearest) >= .399f);
        }
    }
    if (group.GetMoveStatus() != Ai::MoveStatus::Arrived) {
        std::string positions;
        for (const auto& m : group.Members()) positions += " [" + std::to_string(m.position.x) + "," + std::to_string(m.position.y) + " -> " + std::to_string(m.slot.x) + "," + std::to_string(m.slot.y) + "]";
        throw std::runtime_error("formation replacement did not arrive:" + positions);
    }
    CHECK(MinimumGap(group) >= .79f);
    // The live physics scene can leave one member close to the cabinet corner.
    // It still needs a route to its slot, even when the other members have settled.
    soldiers[0]->SetPosition({-.09f, 0, .55f});
    soldiers[1]->SetPosition({1.26075f, 0, -1.23988f});
    soldiers[2]->SetPosition({.089f, 0, -.392f});
    game.StepProjectSimulation(.016f);
    group.MoveTo(glm::vec3(0, 0, -.354167f));
    for (int frame = 0; frame < 1250 && group.GetMoveStatus() != Ai::MoveStatus::Arrived; ++frame) {
        game.StepProjectSimulation(.016f);
        for (const auto& m : group.Members()) {
            const auto nearest = glm::clamp(m.position, glm::vec2(-1.2f, -2.35f), glm::vec2(1.2f, -1.65f));
            CHECK(glm::length(m.position - nearest) >= .399f);
        }
    }
    if (group.GetMoveStatus() != Ai::MoveStatus::Arrived) {
        const auto& m = group.Members()[1];
        throw std::runtime_error("corner approach did not arrive: " + std::to_string(m.position.x) + "," + std::to_string(m.position.y)
            + " fieldReaches=" + std::to_string(group.DiagnosticField()->Reaches(grid, m.position))
            + " fieldRadius=" + std::to_string(group.DiagnosticField()->radius)
            + " memberWalkable=" + std::to_string(grid.IsWalkable(m.position, m.radius)));
    }
}

void SmallGroupSharedGoalTests()
{
    {
        Game game;
        auto& left = AddToScene<Character>(game, "Left", {-1.5f, 0, -6});
        auto& right = AddToScene<Character>(game, "Right", {0, 0, -6});
        left.SetMoveSpeed(3); right.SetMoveSpeed(3);
        game.InitializeGameplay();
        auto& world = Ai::Get(game);
        world.Navigation().SetBounds({-3, -10.8f}, {3, 10.8f}, 0.25f);
        auto& a = world.CreateGroup("Left order"); a.AddCharacter(left, 0.4f); a.SetFormation({0.9f, 1});
        auto& b = world.CreateGroup("Right order"); b.AddCharacter(right, 0.4f); b.SetFormation({0.9f, 1});
        a.MoveTo(glm::vec3(-2, 0, -4.5f));
        b.MoveTo(glm::vec3(2, 0, -4.5f));
        Step(game, 0.5f);
        CHECK(right.GetWorldPosition().x > 0.3f); // an independent right-hand order must not follow the left field
        Step(game, 5);
        CHECK(a.GetMoveStatus() == Ai::MoveStatus::Arrived && b.GetMoveStatus() == Ai::MoveStatus::Arrived);
    }
    Game game;
    auto& soldier = AddToScene<Character>(game, "Soldier", {0, 0, -6});
    soldier.SetMoveSpeed(3);
    game.InitializeGameplay();
    auto& world = Ai::Get(game);
    world.Navigation().SetBounds({-3, -10.8f}, {3, 10.8f}, 0.25f);
    auto& group = world.CreateGroup("Selected");
    group.AddCharacter(soldier, 0.4f);
    group.SetFormation({0.9f, 1});
    group.MoveTo(glm::vec3(-2, 0, -4.5f));
    Step(game, 3);
    CHECK(group.GetMoveStatus() == Ai::MoveStatus::Arrived);
    group.MoveTo(glm::vec3(2, 0, -4.5f));
    Step(game, 6);
    if (group.GetMoveStatus() != Ai::MoveStatus::Arrived) {
        const auto p = soldier.GetWorldPosition();
        throw std::runtime_error("replacement order followed old shared goal: " + std::to_string(p.x) + "," + std::to_string(p.z));
    }
    CHECK(glm::length(soldier.GetWorldPosition() - glm::vec3(2, 0, -4.5f)) < 0.11f);
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
