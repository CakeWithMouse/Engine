// Generated from docs/AI-CPP-GUIDE.md by Extract-Guide.ps1. Change the guide, not this file.
#include "GuideExamples.h"
#include <Engine/MainGame/BaseGameClass/Game.h>
// ---- guide block 2
// Game/MyGame/Source/GuardAi.cpp

namespace GuardAi
{
struct AttackData { float cooldown = 0; };   // у каждого юнита своя копия

class Attack final : public Ai::Task<AttackData>
{
public:
    explicit Attack(float damage) : damage_(damage) {}   // настройка, общая для всех

    Ai::Status Tick(Ai::Context& ctx, AttackData& data, float dt) const override
    {
        data.cooldown -= dt;
        if (data.cooldown > 0) return Ai::Status::Running;

        const Ai::ObjectRef target = ctx.Memory().Get(Target);
        if (!target) return Ai::Status::Failed;

        ctx.GetWorld().ReportDamage(target, ctx.Self().OwnerRef(), damage_);
        data.cooldown = 1.0f;
        return Ai::Status::Running;
    }

private:
    float damage_;
};
}
// ---- guide block 3
// Продолжение GuardAi.cpp
namespace GuardAi
{
Ai::MachinePtr Machine()
{
    static const Ai::MachinePtr machine = [] {
        Ai::MachineBuilder m("Guard");

        auto& patrol = m.State("Patrol");            // родитель: сам не выполняется, выбирает ребёнка
        patrol.GoTo("Chase").OnEvent(Ai::Events::Spotted);

        auto& walk = patrol.State("Walk");           // первый ребёнок: сюда автомат войдёт при старте
        walk.Task<Ai::Tasks::FollowRoute>();
        walk.GoTo("Rest").OnSucceeded();

        auto& rest = patrol.State("Rest");
        rest.Task<Ai::Tasks::Wait>(2.0f);
        rest.GoTo("Walk").OnCompleted();

        auto& combat = m.State("Combat");
        combat.Task<Ai::Tasks::LookAt>(Target);      // работает, пока активен любой ребёнок
        combat.GoTo("Search").OnEvent(Ai::Events::Lost);

        auto& chase = combat.State("Chase");
        chase.Task<Ai::Tasks::MoveTo>(Target, 1.5f);
        chase.GoTo("Hit").OnSucceeded();
        chase.GoTo("Search").If(Ai::Not(Ai::Conditions::CanSee(Target)));   // надёжнее события, см. «Переходы»

        auto& hit = combat.State("Hit");
        hit.Task<Attack>(10.0f);
        hit.GoTo("Chase").If(Ai::Not(Ai::Conditions::Near(Target, 2.5f)));

        auto& search = m.State("Search");
        search.Task<Ai::Tasks::MoveTo>(LastSeen, 1.0f);
        search.GoTo("Chase").OnEvent(Ai::Events::Spotted);
        search.GoTo("Patrol").OnCompleted();

        return m.Build();   // ошибка в имени состояния — исключение с понятным текстом
    }();
    return machine;
}
}
// ---- guide block 9
[[maybe_unused]] void GuideFragment9(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
s.GoTo("Flee").If(IsWounded);
s.GoTo("Hit").If([](Ai::Context& ctx) { return ctx.DistanceTo(GuardAi::Target) < 2; });
s.GoTo("Chase").If(Ai::All(Ai::Conditions::HasKey(GuardAi::Target), Ai::Not(IsWounded)));
s.EnterIf(Ai::Conditions::HasKey(GuardAi::Target));   // без цели в это состояние не войти
}
// ---- guide block 10
[[maybe_unused]] void GuideFragment10(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
Ai::Blackboard& memory = ctx.Memory();
memory.Set(Health, 80.0f);
const float hp = memory.Get(Health, 100.0f);     // второй аргумент — значение по умолчанию
if (memory.Has(GuardAi::Target)) memory.Clear(GuardAi::Target);

// ObjectRef хранит ID, а не указатель: объект могли удалить.
if (auto* enemy = ctx.Resolve<Character>(memory.Get(GuardAi::Target)))
    enemy->GetWorldPosition();
}
// ---- guide block 12
[[maybe_unused]] void GuideFragment12(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
ai.SendEvent(Alarm);                                 // одному юниту
world.Broadcast(Alarm, {10, 0, 5}, 30.0f);           // всем в радиусе 30 м
}
// ---- guide block 13
// Game/MyGame/Source/Archer.cpp
Ai::MachinePtr CombatMachine();   // общий бой

Ai::MachinePtr ArcherMachine()
{
    Ai::MachineBuilder m("Archer");
    m.State("Patrol").Task<Ai::Tasks::FollowRoute>();
    m.State("Combat").Linked(CombatMachine());
    return m.Build();
}
// ---- guide block 15
[[maybe_unused]] void GuideFragment15(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
m.Service<UpdateDanger>(0.5f);     // всё время работы автомата
s.Service<UpdateDanger>(0.5f);     // только пока активно состояние
}
// ---- guide block 16
[[maybe_unused]] void GuideFragment16(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
ai.MoveTo(glm::vec3(10, 0, 5));                      // в точку
ai.MoveTo(ctx.Memory().Get(GuardAi::Target), 2.0f);  // за объектом: путь обновляется сам
ai.SetFocus(ctx.Memory().Get(GuardAi::Target));      // смотреть на цель на ходу
ai.StopMovement();
}
// ---- guide block 17
[[maybe_unused]] void GuideFragment17(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
Ai::NavGrid& nav = Ai::Get(game).Navigation();
nav.SetBounds({-100, -100}, {100, 100}, 1.0f);       // XZ, размер клетки 1 м
nav.BlockBox({-5, -1}, {5, 1});                      // стена
nav.SetCost({20, 20}, {40, 40}, 5);                  // болото: проходимо, но дорого
}
// ---- guide block 18
[[maybe_unused]] void GuideFragment18(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
Ai::Perception& senses = ai.GetPerception();
senses.SetTeam(Teams::Guards);
senses.EnableSight(25.0f, 120.0f, 5.0f);   // радиус, угол обзора, сколько помнит
senses.EnableHearing(40.0f);
senses.WriteTargetTo(GuardAi::Target);     // лучшая цель сама попадает в память

world.ReportNoise({3, 0, 7}, 1.0f);        // выстрел, шаги, взрыв
world.SetAttitude(Teams::Guards, Teams::Player, Ai::Attitude::Neutral);   // перемирие
}
// ---- guide block 20
[[maybe_unused]] void GuideFragment20(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
auto& hide = s.State("Hide");
hide.Task<Ai::Tasks::RunQuery>(CoverQuery(), CoverPoint);
hide.GoTo("RunToCover").OnSucceeded();
}
// ---- guide block 21
[[maybe_unused]] void GuideFragment21(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
auto& rest = s.State("Rest");
rest.Task<Ai::Tasks::UseSmartObject>("Seat", 5.0f, 30.0f);  // метка, секунды, радиус поиска
rest.GoTo("Wander").OnCompleted();                             // посидел или мест нет
}
// ---- guide block 24
[[maybe_unused]] void GuideFragment24(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
world.SpawnDirector().Run(BattleScript());
}
// ---- guide block 27
[[maybe_unused]] void GuideFragment27(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
Ai::Group& regiment = world.CreateGroup();
for (size_t i = 0; i < 100; ++i)
    regiment.AddInstance(batchId, i, 1.8f, 6.0f);    // экземпляр MeshInstance
regiment.Run(RegimentMachine());
regiment.SetFacing(0.0f);
regiment.MoveTo(glm::vec3(200, 0, 50), 5.0f);        // приказ игрока
}
// ---- guide block 28
[[maybe_unused]] void GuideFragment28(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)
{
// фрагмент
world.Debug().SetVisible(true);        // над юнитами: состояние, путь, конус зрения
world.Debug().Select(ai);              // подробности: память, задачи, журнал
const std::string state = ai.ActiveState();          // "Combat/Chase"
const std::vector<std::string>& log = ai.History();  // последние переходы и причины
}
// ---- guide block 30
// Game/MyGame/Source/Machines.cpp

Ai::MachinePtr BuildSentry()
{
    Ai::MachineBuilder m("Sentry");
    auto& watch = m.State("Watch");
    watch.Task<Ai::Tasks::Wait>(3.0f);
    watch.GoTo("Turn").OnSucceeded();
    m.State("Turn").Task<Ai::Tasks::Log>("looking around").GoTo("Watch").OnSucceeded();
    return m.Build();
}

ENGINE_AI_MACHINE("Sentry", BuildSentry)
