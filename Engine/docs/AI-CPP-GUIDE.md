# Как программировать ИИ на C++

Актуализировано для 0.4.0, 18.11.2026. API реализован в `Engine/Include/Engine/AI`
и `Engine/Source/AI`. `Engine/tests/ai/run.ps1` собирает и запускает сохранённые
примеры, включая стражника из шагов 1–4. Регенерация примеров через `-RefreshGuide`
нужна только сопровождающему с исходным руководством; для обычного прогона она не требуется.
Помимо C++ доступны [файлы .fsm и редактор автоматов](AI-FSM.md).
Архитектура и замеры: [AI-DESIGN.md](AI-DESIGN.md).

Всё лежит в пространстве имён `Ai`, как `Input` и `Trace`. Один заголовок: `#include <Engine/AI/Ai.h>`. Объявлять подсистему в игре не нужно: движок создаёт ИИ сам.

## Пять понятий

| Понятие | Что это | В Unreal |
|---|---|---|
| Контроллер `Ai::Controller` | Мозг юнита: запускает автомат, хранит память, видит и двигает | AIController |
| Автомат `Ai::StateMachine` | Дерево состояний, одно на тип юнита | StateTree |
| Задача `Ai::Task` | Что юнит делает в состоянии | StateTree Task, BT Task |
| Условие | Функция `bool`: можно ли войти или перейти | Condition, Decorator |
| Память `Ai::Blackboard` | Именованные значения конкретного юнита | Blackboard |

Главное правило: **автомат один на всех, поэтому в объекте задачи нельзя хранить данные юнита.** Методы задачи объявлены `const`, и компилятор сам не даст ошибиться. Всё, что относится к конкретному юниту, лежит в памяти или в структуре `Data` задачи.

## Первый юнит за четыре шага

Стражник ходит по маршруту, отдыхает, замечает игрока, гонится, бьёт, а потеряв цель, идёт туда, где видел её в последний раз.

### Шаг 1. Память и команды

```cpp
// Game/MyGame/Source/GuardAi.h
#pragma once
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
```

### Шаг 2. Своя задача

Движение, ожидание и поворот уже есть в движке. Писать нужно только то, что относится к правилам игры.

```cpp
// Game/MyGame/Source/GuardAi.cpp
#include "GuardAi.h"

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
```

### Шаг 3. Автомат

```cpp
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
```

`static` здесь нужен, чтобы автомат строился один раз и все стражники делили его.

### Шаг 4. Юнит

```cpp
// Game/MyGame/Source/Guard.h
#pragma once
#include "GuardAi.h"
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
```

Чтобы стражник видел игрока, персонаж игрока сообщает о себе:

```cpp
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
```

Соберите проект. Класс появится в палитре редактора, как любой `ENGINE_COMPONENT`, а свойства — в инспекторе. Поставьте стражника в сцену и нажмите Play. Дальше всё идёт само:

1. `AiCharacter::BeginPlay` создаёт контроллер и вызывает ваш `SetupAi`.
2. Автомат выбирает состояние: `Patrol`, затем его первого ребёнка `Walk`.
3. Зрение заметило героя: записало `Target` и послало событие `Spotted`.
4. Переход у `Patrol` срабатывает, автомат выходит из `Walk` и входит в `Combat`, затем в `Chase`.

## Задачи

| Метод | Когда вызывается | Что возвращает |
|---|---|---|
| `Enter` | При входе в состояние | `Running`, если нужно продолжать. `Succeeded` или `Failed`, если всё уже готово |
| `Tick` | При каждом обдумывании, пока задача работает | `Running`, `Succeeded` или `Failed` |
| `Exit` | При выходе из состояния, всегда, даже при прерывании | Ничего. `result` равен `Running`, если задачу прервали |

Переопределять нужно только то, что используете. Все задачи состояния работают одновременно, первая завершившаяся завершает состояние.

Пример с парой `Enter` и `Exit`: ускорение на время состояния. `Exit` вернёт скорость, даже если состояние прервут.

```cpp
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
```

Пример длительного действия: дойти до точки и понять, чем всё кончилось. Если данных нет, пишите `Ai::Task<>`.

```cpp
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
```

## Условия

Условие — обычная функция или лямбда. Подходит везде, где нужно решение «да или нет».

```cpp
inline constexpr Ai::Key<float> Health{"Health"};

static bool IsWounded(Ai::Context& ctx) { return ctx.Memory().Get(Health, 100.0f) < 30; }
```

```cpp
// фрагмент
s.GoTo("Flee").If(IsWounded);
s.GoTo("Hit").If([](Ai::Context& ctx) { return ctx.DistanceTo(GuardAi::Target) < 2; });
s.GoTo("Chase").If(Ai::All(Ai::Conditions::HasKey(GuardAi::Target), Ai::Not(IsWounded)));
s.EnterIf(Ai::Conditions::HasKey(GuardAi::Target));   // без цели в это состояние не войти
```

`EnterIf` решает, можно ли войти в состояние. Родитель пробует детей по порядку объявления и входит в первого, кто разрешил. Так в автомате получается выбор как в Behavior Tree: «есть цель — бой, нет цели — патруль».

## Переходы

| Запись | Когда срабатывает |
|---|---|
| `.OnSucceeded()` | Задача состояния завершилась успешно |
| `.OnFailed()` | Задача завершилась неудачей |
| `.OnCompleted()` | Задача завершилась, неважно как |
| `.OnEvent(event)` | Пришло событие |
| `.If(condition)` | Проверяется при каждом обдумывании |
| `.OnEvent(event).If(condition)` | Пришло событие и выполнено условие |
| `.After(seconds)` | Добавка к любому из вариантов: переход случится с задержкой |

Цель — имя состояния, `Ai::Target::Next` (следующий сосед) или `Ai::Target::Finish` (остановить автомат).

Порядок проверки такой же, как в StateTree:

- Переходы проверяются от листа к корню, внутри состояния — в порядке объявления. Срабатывает первый подходящий.
- Если цель имеет детей, автомат спускается к первому подходящему ребёнку. Если войти некуда, юнит остаётся в текущем состоянии.
- Если состояние завершилось и подходящего перехода нет ни у него, ни у родителей, автомат выбирает состояние заново от корня.
- За одно обдумывание происходит один переход, поэтому зациклиться внутри кадра нельзя.
- Событие живёт одно обдумывание. Если в нём сработал другой переход, событие пропадёт. Поэтому то, что должно случиться наверняка, проверяйте условием: стражник уходит из `Chase` в `Search` по `Not(CanSee(Target))`, а не только по событию `Lost`.

## Память

Поддерживаются `bool`, `int`, `float`, `glm::vec3`, `std::string` и `Ai::ObjectRef`. Память сохраняется в SaveGame сама.

```cpp
// фрагмент
Ai::Blackboard& memory = ctx.Memory();
memory.Set(Health, 80.0f);
const float hp = memory.Get(Health, 100.0f);     // второй аргумент — значение по умолчанию
if (memory.Has(GuardAi::Target)) memory.Clear(GuardAi::Target);

// ObjectRef хранит ID, а не указатель: объект могли удалить.
if (auto* enemy = ctx.Resolve<Character>(memory.Get(GuardAi::Target)))
    enemy->GetWorldPosition();
```

## События

Событие будит автомат сразу, не дожидаясь следующего обдумывания. Своё событие — это одна строка.

```cpp
inline constexpr Ai::Event Alarm{"Alarm"};
```

```cpp
// фрагмент
ai.SendEvent(Alarm);                                 // одному юниту
world.Broadcast(Alarm, {10, 0, 5}, 30.0f);           // всем в радиусе 30 м
```

Стандартные события: `Ai::Events::Spotted`, `Lost`, `Heard`, `Damaged`.

## Подсостояния и повторное использование

Задачи родителя работают, пока активен любой его ребёнок. Это удобно для общего поведения: `LookAt` в `Combat` держит взгляд на цели и в `Chase`, и в `Hit`.

Общий кусок поведения можно описать один раз и вставлять в разные автоматы:

```cpp
// Game/MyGame/Source/Archer.cpp
Ai::MachinePtr CombatMachine();   // общий бой

Ai::MachinePtr ArcherMachine()
{
    Ai::MachineBuilder m("Archer");
    m.State("Patrol").Task<Ai::Tasks::FollowRoute>();
    m.State("Combat").Linked(CombatMachine());
    return m.Build();
}
```

## Службы

Служба работает в фоне с заданным интервалом и обновляет память. Это аналог Service из Behavior Tree и Global Task из StateTree.

```cpp
inline constexpr Ai::Key<float> Danger{"Danger"};

class UpdateDanger final : public Ai::Service<>
{
public:
    void Tick(Ai::Context& ctx, Ai::NoData&, float) const override
    {
        ctx.Memory().Set(Danger, static_cast<float>(ctx.Self().GetPerception().Known().size()));
    }
};
```

```cpp
// фрагмент
m.Service<UpdateDanger>(0.5f);     // всё время работы автомата
s.Service<UpdateDanger>(0.5f);     // только пока активно состояние
```

## Движение

Из автомата проще всего использовать `Ai::Tasks::MoveTo`. Из своего кода:

```cpp
// фрагмент
ai.MoveTo(glm::vec3(10, 0, 5));                      // в точку
ai.MoveTo(ctx.Memory().Get(GuardAi::Target), 2.0f);  // за объектом: путь обновляется сам
ai.SetFocus(ctx.Memory().Get(GuardAi::Target));      // смотреть на цель на ходу
ai.StopMovement();
```

| `GetMoveStatus()` | Значение |
|---|---|
| `Idle` | Приказа нет |
| `Waiting` | Путь ещё считается |
| `Moving` | Идёт |
| `Arrived` | Дошёл |
| `Unreachable` | Цель недостижима, идёт к ближайшей достижимой точке |
| `Stuck` | Нет продвижения, новые попытки не помогли |
| `Unavailable` | Владелец не умеет ходить: турель, скрипт |

Сетку навигации задают компоненты сцены `NavVolume` с режимом «границы», «препятствие» или «стоимость», как NavMeshBoundsVolume и NavModifierVolume в Unreal. То же можно сделать кодом. `BeginPlay` идёт уже после подготовки ИИ, поэтому сетка там доступна:

```cpp
// фрагмент
Ai::NavGrid& nav = Ai::Get(game).Navigation();
nav.SetBounds({-100, -100}, {100, 100}, 1.0f);       // XZ, размер клетки 1 м
nav.BlockBox({-5, -1}, {5, 1});                      // стена
nav.SetCost({20, 20}, {40, 40}, 5);                  // болото: проходимо, но дорого
```

## Восприятие

Зрение, слух и урон работают как AI Perception. Настройка — в `SetupAi`.

```cpp
// фрагмент
Ai::Perception& senses = ai.GetPerception();
senses.SetTeam(Teams::Guards);
senses.EnableSight(25.0f, 120.0f, 5.0f);   // радиус, угол обзора, сколько помнит
senses.EnableHearing(40.0f);
senses.WriteTargetTo(GuardAi::Target);     // лучшая цель сама попадает в память

world.ReportNoise({3, 0, 7}, 1.0f);        // выстрел, шаги, взрыв
world.SetAttitude(Teams::Guards, Teams::Player, Ai::Attitude::Neutral);   // перемирие
```

По умолчанию разные команды враждебны, команда 0 нейтральна. Восприятие посылает события `Spotted`, `Lost`, `Heard`, `Damaged`, а список известных целей лежит в `Known()` и `Best()`.

## Поиск лучшей точки

`Ai::Query` — это аналог EQS. Он создаёт точки-кандидаты, отсеивает негодные, остальным ставит оценку и отдаёт лучшую.

```cpp
static Ai::QueryPtr CoverQuery()
{
    Ai::QueryBuilder q("Cover");
    q.Points(Ai::Generators::Ring(6.0f, 18.0f, 24));              // кандидаты вокруг юнита
    q.Filter(Ai::Tests::Reachable());                             // можно дойти
    q.Filter(Ai::Tests::HiddenFrom(GuardAi::Target));             // враг не видит
    q.Score(Ai::Tests::Distance(GuardAi::Target), Ai::Prefer::Far);
    return q.Build();
}
```

```cpp
// фрагмент
auto& hide = s.State("Hide");
hide.Task<Ai::Tasks::RunQuery>(CoverQuery(), CoverPoint);
hide.GoTo("RunToCover").OnSucceeded();
```

## Умные объекты

Лавка, пушка, колодец — объект сцены, которым юнит умеет пользоваться, как Smart Objects в Unreal. В палитре редактора это «Умный объект»: у него есть метка («Seat», «Cannon») и число мест. Задача `UseSmartObject` находит ближайший объект с меткой и свободным местом, занимает место, доходит и пользуется им заданное время:

```cpp
// фрагмент
auto& rest = s.State("Rest");
rest.Task<Ai::Tasks::UseSmartObject>("Seat", 5.0f, 30.0f);  // метка, секунды, радиус поиска
rest.GoTo("Wander").OnCompleted();                             // посидел или мест нет
```

- Место занимается сразу при входе в состояние. Пока юнит идёт, второй к той же лавке не пойдёт.
- Место освобождается в любом случае: использование кончилось, состояние прервал переход или событие, юнит удалён.
- Задача проваливается, если свободных мест нет, дойти нельзя или объект убрали из сцены, пока к нему шли.
- Из кода: `world.FindSmartObject(tag, from, radius)`, `object.Claim(ai.Serial())`, `Release`, `FreeSlots`.

## Не только юниты

Автомат не привязан к персонажу. Им можно управлять чем угодно.

Турель — обычный компонент без `Character`. Она видит и думает, но не ходит:

```cpp
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
```

Логика вообще без объекта сцены: сценарий боя, волны врагов, фазы босса.

```cpp
Ai::MachinePtr BattleScript();
```

```cpp
// фрагмент
world.SpawnDirector().Run(BattleScript());
```

## Свой контроллер

Обычно хватает `SetupAi`. Если нужны свои реакции и поля, наследуйтесь от контроллера. Это тот же приём, что `Game::CreateCharacterController`.

```cpp
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
```

## Отряды и толпа

Стратегический слой — это те же автоматы, задачи и память. Отряд `Ai::Group` является контроллером, поэтому `Ai::Tasks::MoveTo` работает и для него, только вместо A* используется поле направлений. Автомат один на отряд, бойцы только двигаются.

```cpp
Ai::MachinePtr RegimentMachine();
```

```cpp
// фрагмент
Ai::Group& regiment = world.CreateGroup();
for (size_t i = 0; i < 100; ++i)
    regiment.AddInstance(batchId, i, 1.8f, 6.0f);    // экземпляр MeshInstance
regiment.Run(RegimentMachine());
regiment.SetFacing(0.0f);
regiment.MoveTo(glm::vec3(200, 0, 50), 5.0f);        // приказ игрока
```

Что полезно знать о цене толпы (замеры — в [AI-DESIGN.md](AI-DESIGN.md#замеры)):

- 30 000 бойцов в 324 полках стоят около 4 мс на кадр в Release. Больше всего уходит на обход соседей.
- Боец выбирает скорость не чаще 15 раз в секунду, бойцы по очереди: `world.GetBudget().crowdReplanInterval`. Двигается он каждый кадр.
- Поле направлений строится частями: 25 000 клеток за кадр (`fieldCellsPerFrame`). Пока поле не готово, отряд в статусе `Waiting`. Если приказ получат сразу сотни отрядов, последние тронутся через несколько секунд.
- Поле строится только в окне вокруг отряда и цели и расширяется само, если путь выходит за окно. Отряды с одной целью делят одно поле.
- `FindHostile` перебирает все отряды. На сотни отрядов вызывайте его из службы с интервалом, как в `Battle`, а не каждый кадр.

## Сохранение

Обычно ничего писать не нужно. Движок сохраняет активные состояния (по именам), память, приказы и восприятие.

- `Data` задач не сохраняется. После загрузки автомат возвращается в те же состояния и заново вызывает `Enter`, поэтому `MoveTo` запросит путь снова, а `Wait` начнёт отсчёт заново.
- Свои поля контроллера дописывайте в `SerializeSaveGame`.
- Ссылки на объекты храните в `Ai::ObjectRef`: при загрузке их перевяжет движок.

## Отладка и тесты

Текущее состояние и журнал переходов можно получить из кода:

```cpp
// фрагмент
world.Debug().SetVisible(true);        // над юнитами: состояние, путь, конус зрения
world.Debug().Select(ai);              // подробности: память, задачи, журнал
const std::string state = ai.ActiveState();          // "Combat/Chase"
const std::vector<std::string>& log = ai.History();  // последние переходы и причины
```

Журнал выглядит так:

```text
  1.25  Patrol/Walk -> Combat/Chase   event Ai.Spotted
  3.40  Combat/Chase -> Combat/Hit   task succeeded
```

ИИ работает без окна и GPU, поэтому поведение проверяется обычным тестом:

```cpp
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
```

Тесты самого ИИ лежат в `Engine/tests/ai` и запускаются одной командой; фильтр — часть имени теста:

```text
powershell -File Engine/tests/ai/run.ps1                     # Debug, все тесты
powershell -File Engine/tests/ai/run.ps1 -Filter stage12     # только умные объекты
powershell -File Engine/tests/ai/run.ps1 -Configuration Release
```

Замер толпы без окна: `Engine/build/x64/Release/AiTests.exe --bench 30000 60` — 30 000 бойцов, 60 секунд игры по 1/60 с. Флаг `--profile` в конце добавляет профиль: самые дорогие функции и строки, отдельно для медленных кадров.

В лаунчере, который открывает `Game/Start.cmd`, выберите **Battle — ИИ полков** и нажмите «Играть». Задача `FireAtEnemy` отвечает за залпы при контакте, а `BattleGame` применяет урон и выводит из строя уничтоженные машины через `Ai::Group::RemoveMember`. HUD показывает бой и потери, а `F9` включает линии целей и отметки строя. Для запуска из консоли используйте `Battle.exe --ai --hud`. Флаг `--ai-debug` включает отладчик с первого кадра. В строке результата есть `ai_ms`, `regiments`, `ai_states`, `shots`, `hits`, `destroyed`, `alive_blue` и `alive_red`.

## Частые ошибки

| Ошибка | Что будет | Как правильно |
|---|---|---|
| Хранить поле юнита в объекте задачи | Не скомпилируется: методы `const` | Положить в `Data` или в память |
| Хранить `Character*` | Падение после удаления объекта | `Ai::ObjectRef` и `ctx.Resolve` |
| Вызывать `Ai::Get` на каждого юнита каждый кадр | Лишний перебор подсистем | Внутри задач есть `ctx.GetWorld()` |
| Строить автомат для каждого юнита | Лишняя память и время | `static` внутри функции, как в примере |
| Считать, что `dt` — настоящие секунды | Таймеры спешат в сценах с масштабом времени | Проектам масштаб 1.0 ставит движок, в своём `Game` ставьте `SetSimulationTimeScale(1.0f)` |
| Менять состояние прямо из задачи | Такого API нет намеренно | Вернуть статус или послать событие, переход решает автомат |
| Двигать юнита сам, пока идёт `MoveTo` | Юниты дёргаются | Сначала `StopMovement` |
| Ждать `Spotted` без цели | Никого не видно | `AddTarget` у того, кого должны видеть |
| Назвать переменную `near` или `far` | Непонятные C2059 и C2143 там, где виден `<windows.h>`: это его макросы | Другие имена: `closest`, `distant` |
| Ждать, что задача провалится и переход случится в том же кадре | Переход наступает на следующем обдумывании | Проверять состояние через кадр или по условию |

## Автомат по имени

Автомат всегда собирается в C++. Чтобы не заводить класс-наследник ради каждого юнита, автомату можно дать имя, а юниту в сцене — выбрать его по имени. Это заменяет файл автомата из Unreal: графического редактора нет, решение от 2026-11-13.

```cpp
// Game/MyGame/Source/Machines.cpp
#include "GuardAi.h"

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
```

Дальше поставьте в сцену «ИИ-персонаж» и впишите `Sentry` в свойство «Автомат (имя из C++)». Остальные свойства — команда, зрение, слух — там же.

- `ENGINE_AI_MACHINE` регистрирует автомат при запуске программы. Писать его нужно в `.cpp` самого игрового проекта: из статической библиотеки линкер может выбросить такой файл. Явный вариант — вызвать `Ai::RegisterMachine("Sentry", &BuildSentry)` до начала игры.
- Автомат строится один раз, при первом запросе, и общий для всех юнитов с этим именем.
- Если `SetupAi` наследника уже запустил свой автомат, свойство не используется.
- Неизвестное имя останавливает запуск игры с сообщением: кто его просил и какие имена есть.
- Из кода: `Ai::FindMachine(name)` (или `nullptr`), `Ai::GetMachine(name)` (или исключение), `Ai::MachineNames()`.

## Справочник

| Стандартная задача | Что делает |
|---|---|
| `Ai::Tasks::Wait(seconds)` | Ждёт |
| `Ai::Tasks::WaitRandom(min, max)` | Ждёт случайное время |
| `Ai::Tasks::MoveTo(key, radius)` | Идёт к точке или объекту из памяти |
| `Ai::Tasks::FollowRoute()` | Идёт к следующей точке маршрута из `SetRoute` |
| `Ai::Tasks::LookAt(key)` | Смотрит на объект, пока активно состояние |
| `Ai::Tasks::SendEvent(event)` | Посылает событие себе |
| `Ai::Tasks::ClearKey(key)` | Очищает значение памяти |
| `Ai::Tasks::RunQuery(query, key)` | Ищет лучшую точку и пишет её в память |
| `Ai::Tasks::Log(text)` | Пишет в журнал |
| `Ai::Tasks::UseSmartObject(tag, seconds, radius = 50)` | Занимает место у ближайшего умного объекта, доходит и пользуется |

| Стандартное условие | Истинно, если |
|---|---|
| `Ai::Conditions::HasKey(key)` | Значение есть в памяти |
| `Ai::Conditions::IsTrue(key)` | Логическое значение памяти истинно |
| `Ai::Conditions::Near(key, distance)` | Цель ближе расстояния |
| `Ai::Conditions::CanSee(key)` | Цель видна прямо сейчас |
| `Ai::Conditions::TimeInState(seconds)` | Юнит в состоянии дольше этого времени |
| `Ai::Conditions::Chance(probability)` | Случай: 0.3 означает 30% |
| `Ai::Not(c)`, `Ai::All(a, b)`, `Ai::Any(a, b)` | Отрицание, «и», «или» |

| Автоматы по имени | Что делает |
|---|---|
| `Ai::RegisterMachine(name, factory)` | Регистрирует или заменяет автомат; фабрика вызывается один раз |
| `ENGINE_AI_MACHINE(name, function)` | То же при запуске программы, в `.cpp` игрового проекта |
| `Ai::FindMachine(name)` | Автомат или `nullptr` |
| `Ai::GetMachine(name)` | Автомат или исключение со списком имён |
| `Ai::MachineNames()` | Зарегистрированные имена |
