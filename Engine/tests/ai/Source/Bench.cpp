// Stage 10: the AI of a Battle-sized army without a window or GPU.
//   AiTests.exe --bench [soldiers] [seconds] [--profile]
// The scene repeats Battle --ai: soldiers on a lattice 8 m apart, two armies split by z = 0, regiments
// cut by 80 m squares, a 2 m grid with 300 buildings, and the regiment machine of
// Game/Battle/Source/BattleAi.cpp. Every World::Tick of 1/60 s is timed by its own counters.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "SamplingProfiler.h"
#include "TestSupport.h"
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <random>
#include <tuple>

namespace
{
constexpr float Pi = 3.14159265f;
constexpr float Spacing = 8.0f;
constexpr float RegimentArea = 80.0f;
constexpr float FrontLine = 50.0f;
constexpr float EngageDistance = 100.0f;
constexpr float AttackDepth = 120.0f;

const Ai::Key<glm::vec3> Home{"Home"};
const Ai::Key<glm::vec3> Front{"Front"};
const Ai::Key<glm::vec3> Objective{"Objective"};
const Ai::Key<glm::vec3> EnemyAt{"EnemyAt"};
const Ai::Key<bool> EnemyNear{"EnemyNear"};
const Ai::Key<bool> Forward{"Forward"};

// The regiment machine of BattleAi.cpp, unchanged in behaviour.
class WatchEnemy final : public Ai::Service<>
{
public:
    void Tick(Ai::Context& ctx, Ai::NoData&, float) const override
    {
        auto* regiment = dynamic_cast<Ai::Group*>(&ctx.Self());
        Ai::Group* enemy = regiment ? regiment->FindHostile(EngageDistance) : nullptr;
        ctx.Memory().Set(EnemyNear, enemy != nullptr);
        if (enemy) ctx.Memory().Set(EnemyAt, enemy->Position());
    }
};

class PickObjective final : public Ai::Task<>
{
public:
    Ai::Status Enter(Ai::Context& ctx, Ai::NoData&) const override
    {
        const bool forward = !ctx.Memory().Get(Forward);
        ctx.Memory().Set(Forward, forward);
        ctx.Memory().Set(Objective, ctx.Memory().Get(forward ? Front : Home));
        return Ai::Status::Succeeded;
    }
};

class SetHomeObjective final : public Ai::Task<>
{
public:
    Ai::Status Enter(Ai::Context& ctx, Ai::NoData&) const override
    {
        ctx.Memory().Set(Forward, false);
        ctx.Memory().Set(Objective, ctx.Memory().Get(Home));
        return Ai::Status::Succeeded;
    }
};

class FaceEnemy final : public Ai::Task<float>
{
public:
    explicit FaceEnemy(float seconds) : seconds_(seconds) {}
    Ai::Status Enter(Ai::Context& ctx, float& left) const override
    {
        ctx.Self().StopMovement();
        left = seconds_;
        return Ai::Status::Running;
    }
    Ai::Status Tick(Ai::Context& ctx, float& left, float dt) const override
    {
        auto* regiment = dynamic_cast<Ai::Group*>(&ctx.Self());
        if (regiment && ctx.Memory().Has(EnemyAt))
        {
            const glm::vec3 to = ctx.Memory().Get(EnemyAt) - regiment->Position();
            if (std::abs(to.x) + std::abs(to.z) > 1e-3f) regiment->SetFacing(std::atan2(to.x, to.z));
        }
        left -= dt;
        return left > 0 ? Ai::Status::Running : Ai::Status::Succeeded;
    }
private:
    float seconds_;
};

Ai::MachinePtr RegimentMachine()
{
    Ai::MachineBuilder m("Regiment");
    m.Service<WatchEnemy>(0.5f);
    auto& maneuver = m.State("Maneuver");
    maneuver.GoTo("Engage").If(Ai::Conditions::IsTrue(EnemyNear));
    maneuver.State("Hold").Task<Ai::Tasks::WaitRandom>(2.0f, 8.0f).GoTo("Pick").OnSucceeded();
    maneuver.State("Pick").Task<PickObjective>().GoTo("March").OnSucceeded();
    maneuver.State("March").Task<Ai::Tasks::MoveTo>(Objective, 8.0f).GoTo("Hold").OnCompleted();
    m.State("Engage").Task<FaceEnemy>(15.0f).GoTo("Retreat").OnSucceeded();
    m.State("Retreat").Task<SetHomeObjective>().GoTo("Withdraw").OnSucceeded();
    m.State("Withdraw").Task<Ai::Tasks::MoveTo>(Objective, 8.0f).GoTo("Maneuver").OnCompleted();
    return m.Build();
}

struct Series
{
    std::vector<double> values;
    void Add(double v) { values.push_back(v); }
    double Average() const
    {
        double sum = 0;
        for (double v : values) sum += v;
        return values.empty() ? 0 : sum / values.size();
    }
    double Percentile(double q) const
    {
        if (values.empty()) return 0;
        std::vector<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        return sorted[std::min(sorted.size() - 1, static_cast<size_t>(q * sorted.size()))];
    }
    double Max() const { return values.empty() ? 0 : *std::max_element(values.begin(), values.end()); }
};

// Windows may slow down a console that runs in the background (power throttling); a game window in
// front is not slowed down. The measurement asks for full speed. Returns false when that was refused.
bool RunAtFullSpeed()
{
#ifdef _WIN32
    PROCESS_POWER_THROTTLING_STATE state{};
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    state.StateMask = 0;
    const bool process = SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state)) != 0;
    const bool thread = SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) != 0;
    return process && thread;
#else
    return true;
#endif
}

double MsSince(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
} // namespace

int RunBench(int soldiers, float seconds, bool profile)
{
    const bool fullSpeed = RunAtFullSpeed();
    const auto setupStart = std::chrono::steady_clock::now();
    Game game;
    const int perRow = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(soldiers))));
    InstancedMeshComponent& units = AddToScene<InstancedMeshComponent>(game, "Units");
    std::mt19937 rng(1337u);
    std::uniform_real_distribution<float> jitter(-0.3f, 0.3f);
    for (int i = 0; i < soldiers; ++i)
    {
        const int row = i / perRow, column = i % perRow;
        MeshInstance instance;
        instance.Position = glm::vec3((column - perRow * 0.5f + (row % 2) * 0.5f + jitter(rng)) * Spacing, 0,
                                      (row - perRow * 0.5f + jitter(rng) * 0.4f) * Spacing);
        units.AddInstance(instance);
    }
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);

    const float fieldHalf = perRow * Spacing * 0.5f + 20.0f;   // as Battle's FieldHalfSize
    const float half = fieldHalf + 40.0f;
    Ai::NavGrid& nav = world.Navigation();
    nav.SetBounds({-half, -half}, {half, half}, 2.0f);
    std::uniform_real_distribution<float> place(-fieldHalf, fieldHalf), turn(0.0f, 6.283f);
    for (int i = 0; i < 300; ++i)
    {
        const glm::vec2 at(place(rng), place(rng));
        switch (i % 3)
        {
        case 0: nav.BlockCircle(at, 6.0f); break;              // house
        case 1: nav.BlockCircle(at, 3.5f); break;              // tower
        default:                                                // wall
        {
            const float yaw = turn(rng);
            const glm::vec2 along(std::cos(yaw), -std::sin(yaw));
            for (float t = -5.0f; t <= 5.0f; t += 2.5f) nav.BlockCircle(at + along * t, 1.2f);
        }
        }
    }

    std::map<std::tuple<int, int, int>, Ai::Group*> regiments;
    std::map<Ai::Group*, glm::vec3> sums;
    std::map<Ai::Group*, int> counts;
    for (size_t i = 0; i < units.GetMeshInstanceCount(); ++i)
    {
        const glm::vec3 p = units.GetMeshInstance(i).Position;
        const int team = p.z > 0.0f ? 2 : 1;
        const auto key = std::make_tuple(static_cast<int>(std::floor(p.x / RegimentArea)), static_cast<int>(std::floor(p.z / RegimentArea)), team);
        Ai::Group*& regiment = regiments[key];
        if (!regiment)
        {
            regiment = &world.CreateGroup();
            regiment->GetPerception().SetTeam({static_cast<uint8_t>(team)});
            regiment->SetFormation({Spacing, 10});
            regiment->SetFacing(team == 1 ? 0.0f : Pi);
            regiment->SetThinkInterval(0.2f);
        }
        regiment->AddInstance(units.GetComponentId(), i, 2.2f, 6.0f);
        sums[regiment] += p;
        ++counts[regiment];
    }
    const Ai::MachinePtr machine = RegimentMachine();
    for (auto& [key, regiment] : regiments)
    {
        const glm::vec3 home = sums[regiment] / static_cast<float>(counts[regiment]);
        regiment->Memory().Set(Home, home);
        const bool south = std::get<2>(key) == 1;
        const float front = south ? std::max(home.z, std::min(home.z + AttackDepth, -FrontLine))
                                  : std::min(home.z, std::max(home.z - AttackDepth, FrontLine));
        regiment->Memory().Set(Front, glm::vec3(home.x, 0.0f, front));
        regiment->Run(machine);
    }
    const double setupMs = MsSince(setupStart);

    // The first frames only start the machines; the measurement covers the whole run anyway.
    const float dt = 1.0f / 60.0f;
    const int steps = static_cast<int>(seconds / dt);
    Series total, steady, sync, perception, think, navigation, movement, wall;   // steady: after the first second
    uint64_t fieldCells = 0, fieldsFinished = 0, pathNodes = 0, sightChecks = 0;
    size_t peakFieldCells = 0, peakFields = 0;
    int stepsOverBudget = 0;
#ifdef _WIN32
    SamplingProfiler profiler;
    std::atomic<int> currentStep{0};
    if (profile) profiler.Start(&currentStep);
#endif
    for (int step = 0; step < steps; ++step)
    {
        currentStep = step;
        const auto stepStart = std::chrono::steady_clock::now();
        game.StepProjectSimulation(dt);
        wall.Add(MsSince(stepStart));
        const Ai::Stats& s = world.GetStats();
        total.Add(s.totalMs);
        if (step >= 60) steady.Add(s.totalMs);
        sync.Add(s.syncMs);
        perception.Add(s.perceptionMs);
        think.Add(s.thinkMs);
        navigation.Add(s.navigationMs);
        movement.Add(s.movementMs);
        fieldCells += s.fieldCells;
        fieldsFinished += s.fieldsFinished;
        pathNodes += s.pathNodes;
        sightChecks += s.sightChecks;
        stepsOverBudget += s.totalMs > 4.0;
        peakFieldCells = std::max(peakFieldCells, world.Fields().Cells());
        peakFields = std::max(peakFields, world.Fields().Size());
    }
#ifdef _WIN32
    profiler.Stop();
#endif

    // Did the marches end well? A march leaves by arriving (task succeeded), by failing
    // (unreachable or stuck) or because the enemy came close (a condition).
    std::map<std::string, int> states;
    size_t moved = 0, arrived = 0, failed = 0, interrupted = 0;
    for (Ai::Controller* c : world.Controllers())
    {
        ++states[c->ActiveState()];
        auto* regiment = dynamic_cast<Ai::Group*>(c);
        if (regiment && glm::length(regiment->Position() - sums[regiment] / static_cast<float>(counts[regiment])) > 10.0f) ++moved;
        for (const std::string& line : c->History())
        {
            if (line.find("March -> ") == std::string::npos && line.find("Withdraw -> ") == std::string::npos) continue;
            if (line.find("task succeeded") != std::string::npos) ++arrived;
            else if (line.find("task failed") != std::string::npos) ++failed;
            else ++interrupted;
        }
    }
    std::string stateText;
    for (const auto& [state, count] : states) stateText += (stateText.empty() ? "" : ",") + state + ":" + std::to_string(count);

    std::printf("ai-bench soldiers=%d regiments=%zu grid=%dx%d seconds=%.0f steps=%d setup_ms=%.0f full_speed=%d\n",
                soldiers, regiments.size(), nav.Width(), nav.Height(), seconds, steps, setupMs, fullSpeed ? 1 : 0);
    const size_t worst = static_cast<size_t>(std::max_element(total.values.begin(), total.values.end()) - total.values.begin());
    std::printf("  tick_ms    avg=%.3f p50=%.3f p95=%.3f p99=%.3f max=%.3f (step %zu) over_4ms=%d of %d\n",
                total.Average(), total.Percentile(0.5), total.Percentile(0.95), total.Percentile(0.99), total.Max(), worst, stepsOverBudget, steps);
    std::printf("  steady_ms  avg=%.3f p50=%.3f p95=%.3f p99=%.3f max=%.3f (without the first second)\n",
                steady.Average(), steady.Percentile(0.5), steady.Percentile(0.95), steady.Percentile(0.99), steady.Max());
    std::printf("  avg_ms     sync=%.3f perception=%.3f think=%.3f navigation=%.3f movement=%.3f\n",
                sync.Average(), perception.Average(), think.Average(), navigation.Average(), movement.Average());
    std::printf("  max_ms     sync=%.3f perception=%.3f think=%.3f navigation=%.3f movement=%.3f\n",
                sync.Max(), perception.Max(), think.Max(), navigation.Max(), movement.Max());
    std::printf("  work       fields=%llu field_cells=%llu peak_fields=%zu peak_field_cells=%zu (%.1f MB) path_nodes=%llu sight_checks=%llu\n",
                static_cast<unsigned long long>(fieldsFinished), static_cast<unsigned long long>(fieldCells), peakFields, peakFieldCells,
                peakFieldCells * 5.0 / (1024 * 1024), static_cast<unsigned long long>(pathNodes), static_cast<unsigned long long>(sightChecks));
    std::printf("  outcome    moved=%zu of %zu states=%s\n", moved, regiments.size(), stateText.c_str());
    std::printf("  marches    arrived=%zu failed=%zu interrupted=%zu\n", arrived, failed, interrupted);
    std::printf("  step_ms    avg=%.3f (the whole StepProjectSimulation)\n", wall.Average());
#ifdef _WIN32
    if (profile)
    {
        profiler.Report(25);
        // The slow frames on their own: what makes the spikes.
        const double slow = std::max(8.0, steady.Percentile(0.99) * 0.7);
        std::printf("  slow frames: total over %.1f ms after the first second\n", slow);
        profiler.Report(25, [&](int step) { return step >= 60 && total.values[static_cast<size_t>(step)] > slow; });
    }
#endif
    return 0;
}
