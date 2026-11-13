// Battle regiments advance, exchange fire and withdraw. F9 shows their movement orders.
#include "BattleGame.h"
#include <Engine/AI/Ai.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <tuple>

namespace
{
constexpr float Pi = 3.14159265f;
constexpr float RegimentArea = 80.0f;      // vehicles of one regiment start inside this square, metres
// FindHostile measures between regiment centres. The detection radius overlaps the two front
// objectives, allowing for staggered columns and slots displaced by obstacles.
constexpr float FrontLine = 50.0f;
constexpr float EngageDistance = 116.0f;
constexpr float AttackDepth = 120.0f;      // the furthest one attack goes
constexpr float ShotRange = 150.0f;
constexpr float TracerDuration = 0.22f;
constexpr float HitDuration = 0.24f;
constexpr float DestructionDuration = 0.85f;

const Ai::Key<glm::vec3> Home{"Home"};
const Ai::Key<glm::vec3> Front{"Front"};
const Ai::Key<glm::vec3> Objective{"Objective"};
const Ai::Key<glm::vec3> EnemyAt{"EnemyAt"};
const Ai::Key<bool> EnemyNear{"EnemyNear"};
const Ai::Key<bool> Advancing{"Advancing"};

// Every half second: is a hostile regiment close?
class WatchEnemy final : public Ai::Service<>
{
public:
    void Tick(Ai::Context& ctx, Ai::NoData&, float) const override
    {
        auto* regiment = dynamic_cast<Ai::Group*>(&ctx.Self());
        Ai::Group* enemy = regiment ? regiment->FindHostile(EngageDistance) : nullptr;
        ctx.Memory().Set(EnemyNear, enemy != nullptr);
        if (enemy) ctx.Memory().Set(EnemyAt, enemy->Position());
        else ctx.Memory().Clear(EnemyAt);
    }
};

// Alternates the objective: forward towards the enemy, then back home.
class PickObjective final : public Ai::Task<>
{
public:
    Ai::Status Enter(Ai::Context& ctx, Ai::NoData&) const override
    {
        const bool forward = !ctx.Memory().Get(Advancing);
        ctx.Memory().Set(Advancing, forward);
        ctx.Memory().Set(Objective, ctx.Memory().Get(forward ? Front : Home));
        return Ai::Status::Succeeded;
    }
};

class SetHomeObjective final : public Ai::Task<>
{
public:
    Ai::Status Enter(Ai::Context& ctx, Ai::NoData&) const override
    {
        ctx.Memory().Set(Advancing, false);
        ctx.Memory().Set(Objective, ctx.Memory().Get(Home));
        return Ai::Status::Succeeded;
    }
};

} // namespace

// An attack stops the regiment, turns it towards its current target and fires several salvos.
class BattleGame::FireAtEnemy final : public Ai::Task<float>
{
public:
    explicit FireAtEnemy(BattleGame& game) : game_(game) {}
    Ai::Status Enter(Ai::Context& ctx, float& untilShot) const override
    {
        ctx.Self().StopMovement();
        untilShot = ctx.Random(0.1f, 0.5f);
        return Ai::Status::Running;
    }
    Ai::Status Tick(Ai::Context& ctx, float& untilShot, float dt) const override
    {
        auto& regiment = static_cast<Ai::Group&>(ctx.Self());
        Ai::Group* enemy = regiment.FindHostile(EngageDistance);
        if (!enemy) return Ai::Status::Succeeded;
        const glm::vec3 to = enemy->Position() - regiment.Position();
        if (std::abs(to.x) + std::abs(to.z) > 1e-3f) regiment.SetFacing(std::atan2(to.x, to.z));
        untilShot -= dt;
        if (untilShot <= 0.0f)
        {
            game_.FireSalvo(ctx, regiment, *enemy);
            untilShot = ctx.Random(0.65f, 1.05f);
        }
        return Ai::Status::Running;
    }
    std::string Name() const override { return "FireAtEnemy"; }
private:
    BattleGame& game_;
};

void BattleGame::SetupAi()
{
    bAiReady = true;
    Ai::World& world = Ai::Get(*this);

    // Navigation: the field with a margin; buildings are obstacles.
    const float half = FieldHalfSize + 40.0f;
    Ai::NavGrid& nav = world.Navigation();
    nav.SetBounds({-half, -half}, {half, half}, 2.0f);
    for (const auto& entry : GetScene().GetComponents())
    {
        const std::string& name = entry.first;
        if (name.rfind("Scenery_", 0) != 0) continue;
        auto* scenery = dynamic_cast<InstancedMeshComponent*>(entry.second.get());
        if (!scenery) continue;
        for (size_t i = 0; i < scenery->GetMeshInstanceCount(); ++i)
        {
            const MeshInstance& building = scenery->GetMeshInstance(i);
            const glm::vec2 at(building.Position.x, building.Position.z);
            if (name == "Scenery_Wall")
            {
                // Twelve metres long along its own X axis: a row of small circles.
                const glm::vec2 along(std::cos(building.Yaw), -std::sin(building.Yaw));
                for (float t = -5.0f; t <= 5.0f; t += 2.5f) nav.BlockCircle(at + along * t, 1.2f);
            }
            else nav.BlockCircle(at, name == "Scenery_Tower" ? 3.5f : 6.0f);
        }
    }

    // Regiments: vehicles that start in one square of the field, one side each.
    std::map<std::tuple<int, int, int>, Ai::Group*> regiments;
    std::map<Ai::Group*, glm::vec3> sums;
    std::map<Ai::Group*, int> counts;
    const bool people = Config.Animated;
    for (const UnitBatch& batch : UnitBatches)
    {
        const float speed = people ? 1.6f : 6.0f * UnitMesh::VehicleSpeedScale(batch.Type);
        const float radius = people ? 0.45f : 2.2f;
        for (size_t i = 0; i < batch.Component->GetMeshInstanceCount(); ++i)
        {
            const glm::vec3 p = batch.Component->GetMeshInstance(i).Position;
            const int team = p.z > 0.0f ? 2 : 1;
            const auto key = std::make_tuple(static_cast<int>(std::floor(p.x / RegimentArea)), static_cast<int>(std::floor(p.z / RegimentArea)), team);
            Ai::Group*& regiment = regiments[key];
            if (!regiment)
            {
                regiment = &world.CreateGroup("Regiment " + std::to_string(std::get<0>(key)) + "," + std::to_string(std::get<1>(key)));
                regiment->GetPerception().SetTeam({static_cast<uint8_t>(team)});
                regiment->SetFormation({people ? 1.5f : Config.Spacing, 10});
                regiment->SetFacing(team == 1 ? 0.0f : Pi);
                regiment->SetThinkInterval(0.2f);
            }
            regiment->AddInstance(batch.Component->GetComponentId(), i, radius, speed);
            sums[regiment] += p;
            ++counts[regiment];
            ++AliveByTeam[team - 1];
        }
    }

    Ai::MachineBuilder m("Regiment");
    m.Service<WatchEnemy>(0.5f);
    auto& maneuver = m.State("Maneuver");
    maneuver.GoTo("Engage").If(Ai::Conditions::IsTrue(Advancing)).If(Ai::Conditions::IsTrue(EnemyNear));
    maneuver.State("Hold").Task<Ai::Tasks::WaitRandom>(1.0f, 4.0f).GoTo("Pick").OnSucceeded();
    maneuver.State("Pick").Task<PickObjective>().GoTo("March").OnSucceeded();
    auto& march = maneuver.State("March").Task<Ai::Tasks::MoveTo>(Objective, 8.0f);
    march.GoTo("Hold").OnCompleted();
    march.GoTo("Hold").After(45.0f);
    auto& engage = m.State("Engage").Task<FireAtEnemy>(*this);
    engage.GoTo("Retreat").OnSucceeded();
    engage.GoTo("Retreat").After(6.0f);
    m.State("Retreat").Task<SetHomeObjective>().GoTo("Withdraw").OnSucceeded();
    auto& withdraw = m.State("Withdraw").Task<Ai::Tasks::MoveTo>(Objective, 8.0f);
    withdraw.GoTo("Maneuver").OnCompleted();
    withdraw.GoTo("Maneuver").After(60.0f);
    const Ai::MachinePtr machine = m.Build();

    for (auto& [key, regiment] : regiments)
    {
        const glm::vec3 home = sums[regiment] / static_cast<float>(counts[regiment]);
        regiment->Memory().Set(Home, home);
        // Forward: towards the enemy, at most AttackDepth, and never past the front line.
        // A regiment that already stands at the line attacks where it is.
        const bool south = std::get<2>(key) == 1;
        const float front = south ? std::max(home.z, std::min(home.z + AttackDepth, -FrontLine))
                                  : std::min(home.z, std::max(home.z - AttackDepth, FrontLine));
        regiment->Memory().Set(Front, glm::vec3(home.x, 0.0f, front));
        HealthByRegiment.emplace(regiment->Serial(), std::vector<int>(regiment->Size(), 2));
        regiment->Run(machine);
    }
    Regiments = regiments.size();
    if (Config.AiDebug) ShowAiDebug(true);
    std::printf("[Info] AI: %zu regiments, navigation %dx%d cells of %.1f m\n", Regiments, nav.Width(), nav.Height(), nav.CellSize());
}

void BattleGame::FireSalvo(Ai::Context& ctx, Ai::Group& regiment, Ai::Group& enemy)
{
    const auto& shooters = regiment.Members();
    const auto& targets = enemy.Members();
    const Ai::NavGrid& nav = ctx.GetWorld().Navigation();
    const float gunHeight = Config.Animated ? 1.4f : 2.2f;
    const auto aimPoint = [gunHeight](const Ai::Group::Member& member) {
        return glm::vec3(member.position.x, member.height + gunHeight, member.position.y);
    };

    // A few live vehicles fire per salvo. The sample bounds the work even for large regiments.
    const size_t first = std::min(shooters.size() - 1, static_cast<size_t>(ctx.Random(0, static_cast<float>(shooters.size()))));
    int fired = 0, attempts = 0;
    for (size_t offset = 0; offset < shooters.size() && fired < 5 && attempts < 20; ++offset)
    {
        const auto& shooter = shooters[(first + offset) % shooters.size()];
        if (!shooter.alive) continue;
        ++attempts;
        const glm::vec3 from = aimPoint(shooter);
        size_t victim = targets.size();
        float closest = ShotRange * ShotRange;
        const size_t start = std::min(targets.size() - 1, static_cast<size_t>(ctx.Random(0, static_cast<float>(targets.size()))));
        for (size_t sample = 0; sample < std::min<size_t>(targets.size(), 12); ++sample)
        {
            const size_t index = (start + sample) % targets.size();
            if (!targets[index].alive) continue;
            const glm::vec3 to = aimPoint(targets[index]);
            const glm::vec3 delta = to - from;
            const float distance = glm::dot(delta, delta);
            if (distance < closest && nav.HasLineOfSight(from, to))
            {
                closest = distance;
                victim = index;
            }
        }
        if (victim == targets.size()) continue;

        ShotEffect shot{from, aimPoint(targets[victim]), enemy.Serial(), victim};
        ++fired;
        ++ShotsFired;
        if (ctx.Random(0.0f, 1.0f) >= 0.7f)
        {
            shot.targetRegiment = 0;
            shot.to += glm::vec3(ctx.Random(-4.0f, 4.0f), 2.0f, ctx.Random(-4.0f, 4.0f));
        }
        ShotEffects.push_back(shot);
    }
}

void BattleGame::UpdateCombatEffects(float deltaTime)
{
    Ai::World& world = Ai::Get(*this);
    std::vector<Ai::Controller*> regiments;
    for (ShotEffect& shot : ShotEffects)
    {
        if (shot.age < TracerDuration)
        {
            const float t = shot.age / TracerDuration;
            DebugDrawLine(glm::mix(shot.from, shot.to, std::max(0.0f, t - 0.22f)),
                          glm::mix(shot.from, shot.to, std::min(1.0f, t + 0.16f)), 0xff50e9ffu);
            if (shot.age + deltaTime >= TracerDuration && shot.targetRegiment != 0)
            {
                if (regiments.empty()) regiments = world.Controllers();
                const auto target = std::find_if(regiments.begin(), regiments.end(), [&](const Ai::Controller* controller) {
                    return controller->Serial() == shot.targetRegiment;
                });
                if (target != regiments.end())
                {
                    auto& enemy = static_cast<Ai::Group&>(**target);
                    const auto& member = enemy.Members()[shot.targetMember];
                    if (member.alive)
                    {
                        shot.hit = true;
                        ++Hits;
                        shot.to.x = member.position.x;
                        shot.to.z = member.position.y;
                        int& health = HealthByRegiment.at(enemy.Serial())[shot.targetMember];
                        if (--health == 0)
                        {
                            enemy.RemoveMember(shot.targetMember);
                            --AliveByTeam[enemy.GetPerception().GetTeam().id - 1];
                            ++Destroyed;
                            shot.destroyed = true;
                        }
                    }
                }
            }
        }
        else if (shot.hit)
        {
            const float time = shot.age - TracerDuration;
            const float progress = time / (shot.destroyed ? DestructionDuration : HitDuration);
            const float size = (shot.destroyed ? 9.0f : 3.0f) * (0.25f + progress);
            const glm::vec3 center = shot.to + glm::vec3(0, shot.destroyed ? progress * 5.0f : 0.0f, 0);
            const uint32_t color = shot.destroyed ? 0xff217dffu : 0xff9af3ffu;
            for (const glm::vec3 direction : {glm::vec3(1, 0.6f, 0), glm::vec3(-1, 0.9f, 0), glm::vec3(0, 0.7f, 1),
                                             glm::vec3(0, 0.5f, -1), glm::vec3(0, 1.3f, 0)})
                DebugDrawLine(center + direction * size * 0.3f, center + direction * size, color);
        }
        shot.age += deltaTime;
    }
    for (Ai::Controller* controller : regiments)
    {
        auto& regiment = static_cast<Ai::Group&>(*controller);
        if (regiment.Size() != 0) continue;
        HealthByRegiment.erase(regiment.Serial());
        world.Destroy(regiment);
    }
    ShotEffects.erase(std::remove_if(ShotEffects.begin(), ShotEffects.end(), [](const ShotEffect& shot) {
        const float impactTime = shot.destroyed ? DestructionDuration : (shot.hit ? HitDuration : 0.0f);
        return shot.age >= TracerDuration + impactTime;
    }), ShotEffects.end());
}

void BattleGame::ShowAiDebug(bool visible)
{
    Ai::World& world = Ai::Get(*this);
    world.Debug().SetVisible(visible);
    if (!visible) return;
    Ai::Controller* nearest = nullptr;
    for (Ai::Controller* controller : world.Controllers())
    {
        if (!nearest || glm::length(controller->Memory().Get(Home)) < glm::length(nearest->Memory().Get(Home)))
            nearest = controller;
    }
    if (nearest) world.Debug().Select(*nearest);
}

void BattleGame::UpdateAiHud()
{
    if (HudAiStats)
    {
        char text[96];
        std::snprintf(text, sizeof(text), "Полков: %zu    ИИ: %.2f мс", Regiments, LastAiMs);
        HudAiStats->SetText(text);
    }
    if (HudAiState)
    {
        int advancing = 0, engaging = 0, withdrawing = 0;
        for (const Ai::Controller* regiment : Ai::Get(*this).Controllers())
        {
            const std::string state = regiment->ActiveState();
            if (state == "Engage") ++engaging;
            else if (state == "Withdraw" || (state == "Maneuver/March" && !regiment->Memory().Get(Advancing))) ++withdrawing;
            else if (state == "Maneuver/March") ++advancing;
        }
        HudAiState->SetText("Идут: " + std::to_string(advancing) + "   Бой: " + std::to_string(engaging) +
                            "   Отход: " + std::to_string(withdrawing));
    }
    if (HudAiAlive)
        HudAiAlive->SetText("Синие: " + std::to_string(AliveByTeam[0]) + "   Красные: " + std::to_string(AliveByTeam[1]));
    if (HudAiCombat)
        HudAiCombat->SetText("Выстрелы: " + std::to_string(ShotsFired) + "   Потери: " + std::to_string(Destroyed));
}

std::string BattleGame::DescribeAiStates() const
{
    auto* ai = GetSubsystem<Ai::Subsystem>();
    if (!ai) return {};
    std::map<std::string, int> states;
    for (Ai::Controller* c : ai->GetWorld().Controllers()) ++states[c->ActiveState()];
    std::string text = " ai_states=";
    for (const auto& [state, count] : states)
    {
        if (text.back() != '=') text += ',';
        text += (state.empty() ? "-" : state) + ":" + std::to_string(count);
    }
    text += " shots=" + std::to_string(ShotsFired) + " hits=" + std::to_string(Hits) +
            " destroyed=" + std::to_string(Destroyed) + " alive_blue=" + std::to_string(AliveByTeam[0]) +
            " alive_red=" + std::to_string(AliveByTeam[1]);
    return text;
}
