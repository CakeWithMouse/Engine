// Stage 1: state machine and memory, no scene. Controllers are directors of a World without Game.
#include "TestSupport.h"
#include <algorithm>

namespace
{
std::vector<std::string> journal;

// Writes every call into the journal; finishes after a number of ticks with the given result.
struct ProbeData { int ticks = 0; };
class Probe final : public Ai::Task<ProbeData>
{
public:
    explicit Probe(std::string name, int finishAfter = -1, Ai::Status result = Ai::Status::Succeeded)
        : name_(std::move(name)), finishAfter_(finishAfter), result_(result) {}
    Ai::Status Enter(Ai::Context&, ProbeData& data) const override
    {
        journal.push_back("enter " + name_);
        data.ticks = 0;
        return finishAfter_ == 0 ? result_ : Ai::Status::Running;
    }
    Ai::Status Tick(Ai::Context&, ProbeData& data, float) const override
    {
        journal.push_back("tick " + name_);
        return ++data.ticks == finishAfter_ ? result_ : Ai::Status::Running;
    }
    void Exit(Ai::Context&, ProbeData&, Ai::Status result) const override
    {
        journal.push_back("exit " + name_ + (result == Ai::Status::Running ? " interrupted" : ""));
    }
private:
    std::string name_;
    int finishAfter_;
    Ai::Status result_;
};

struct CountData { int value = 0; };
class Counter final : public Ai::Task<CountData>
{
public:
    Ai::Status Tick(Ai::Context& ctx, CountData& data, float) const override
    {
        ctx.Memory().Set(Ai::Key<int>{"count"}, ++data.value);
        return Ai::Status::Running;
    }
};

class Tally final : public Ai::Service<>
{
public:
    void Enter(Ai::Context&, Ai::NoData&) const override { journal.push_back("service enter"); }
    void Tick(Ai::Context& ctx, Ai::NoData&, float dt) const override
    {
        ctx.Memory().Set(Ai::Key<int>{"service"}, ctx.Memory().Get(Ai::Key<int>{"service"}) + 1);
        ctx.Memory().Set(Ai::Key<float>{"serviceDt"}, dt);
    }
    void Exit(Ai::Context&, Ai::NoData&) const override { journal.push_back("service exit"); }
};

const Ai::Key<bool> Ready{"Ready"};
const Ai::Event Go{"Go"};

bool JournalHas(const std::vector<std::string>& expected)
{
    return journal == expected;
}
std::string JournalText()
{
    std::string text;
    for (const auto& line : journal) text += line + "; ";
    return text;
}
} // namespace

void BlackboardTests()
{
    Ai::Blackboard memory;
    const Ai::Key<float> health{"Health"};
    const Ai::Key<Ai::ObjectRef> target{"Target"};
    CHECK(memory.Get(health, 100.0f) == 100.0f);
    memory.Set(health, 80.0f);
    CHECK(memory.Has(health) && memory.Get(health) == 80.0f);
    CHECK(!memory.Has(Ai::Key<int>{"Health"}));                 // another type under the same name
    CHECK(memory.Get(Ai::Key<int>{"Health"}, 7) == 7);
    memory.Set(target, Ai::ObjectRef{42});
    CHECK(memory.Get(target).id == 42 && memory.Size() == 2);
    memory.Clear(target);
    CHECK(!memory.Has(target) && memory.Size() == 1);
    memory.Set(Ai::Key<glm::vec3>{"Where"}, glm::vec3(1, 2, 3));
    memory.Set(Ai::Key<std::string>{"Name"}, std::string("guard"));
    CHECK(memory.Describe().find("Where = (1, 2, 3)") != std::string::npos);
    CHECK(memory.Describe().find("Name = \"guard\"") != std::string::npos);
}

void MachineOrderTests()
{
    journal.clear();
    Ai::World world;
    Ai::MachineBuilder m("Order");
    auto& patrol = m.State("Patrol");
    patrol.Task<Probe>("patrol");
    patrol.State("Walk").Task<Probe>("walk", 2).GoTo("Rest").OnSucceeded();
    patrol.State("Rest").Task<Probe>("rest");
    Ai::Controller& ai = world.SpawnDirector("order");
    ai.Run(m.Build());
    CHECK(ai.ActiveState().empty());                 // selection happens on the first think
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Patrol/Walk");
    CHECK(JournalHas({"enter patrol", "enter walk"}));
    world.Tick(0.1f);
    world.Tick(0.1f);                                // walk succeeds on its second tick, transition in the same think
    CHECK(ai.ActiveState() == "Patrol/Rest");
    CHECK(JournalHas({"enter patrol", "enter walk", "tick patrol", "tick walk", "tick patrol", "tick walk", "exit walk", "enter rest"}));
    ai.Stop();
    CHECK(journal.back() == "exit patrol interrupted" && journal[journal.size() - 2] == "exit rest interrupted");
    CHECK(!ai.IsRunning() && ai.ActiveState().empty());
}

void MachineSelectionTests()
{
    journal.clear();
    Ai::World world;
    Ai::MachineBuilder m("Select");
    auto& root = m.State("Root");
    root.State("Fight").EnterIf(Ai::Conditions::IsTrue(Ready)).Task<Probe>("fight");
    auto& calm = root.State("Calm");
    calm.State("Locked").EnterIf([](Ai::Context&) { return false; });
    calm.State("Idle").Task<Probe>("idle");
    root.GoTo("Root").If(Ai::Conditions::IsTrue(Ready));   // re-select when Ready appears
    Ai::Controller& ai = world.SpawnDirector();
    ai.Run(m.Build());
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Root/Calm/Idle");     // Fight refused, Locked refused, Idle entered
    ai.Memory().Set(Ready, true);
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Root/Fight");
    // A parent whose children all refuse is not selected: the transition does not happen.
    Ai::MachineBuilder dead("Dead end");
    auto& start = dead.State("Start");
    start.Task<Probe>("start");
    start.GoTo("Closed").If([](Ai::Context&) { return true; });
    dead.State("Closed").State("Never").EnterIf([](Ai::Context&) { return false; });
    Ai::Controller& stuck = world.SpawnDirector();
    stuck.Run(dead.Build());
    world.Tick(0.1f);
    world.Tick(0.1f);
    CHECK(stuck.ActiveState() == "Start");
}

void MachineTransitionOrderTests()
{
    Ai::World world;
    Ai::MachineBuilder m("Priority");
    auto& parent = m.State("Parent");
    parent.GoTo("FromParent").If([](Ai::Context&) { return true; });
    auto& child = parent.State("Child");
    child.GoTo("FromLeaf").If([](Ai::Context&) { return true; });
    m.State("FromParent");
    auto& leaf = m.State("FromLeaf");
    leaf.GoTo("Third").If([](Ai::Context&) { return true; });
    m.State("Third");
    Ai::Controller& ai = world.SpawnDirector();
    ai.Run(m.Build());
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Parent/Child");
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "FromLeaf");           // the leaf wins over its parent
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Third");              // one transition per think, not a chain
}

void MachineCompletionTests()
{
    journal.clear();
    Ai::World world;
    Ai::MachineBuilder m("Completion");
    auto& work = m.State("Work");
    work.GoTo("Recover").OnFailed();                 // on the parent: the child's failure counts
    work.State("Try").Task<Probe>("try", 1, Ai::Status::Failed);
    m.State("Recover").Task<Probe>("recover", 1, Ai::Status::Succeeded);   // no transition: restart from the root
    Ai::Controller& ai = world.SpawnDirector();
    ai.Run(m.Build());
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Work/Try");
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Recover");
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Work/Try");           // completed without a transition: selected again
    CHECK(ai.History().back().find("selecting from the root") != std::string::npos);

    // A task finishing inside Enter completes its state; the transition happens on the next think.
    Ai::MachineBuilder quick("Quick");
    quick.State("A").Task<Probe>("a", 0).GoTo("B").OnSucceeded();
    quick.State("B");
    Ai::Controller& q = world.SpawnDirector();
    q.SetThinkInterval(10);                          // the completion wakes it anyway
    q.Run(quick.Build());
    world.Tick(0.1f);
    CHECK(q.ActiveState() == "A");
    world.Tick(0.1f);
    CHECK(q.ActiveState() == "B");
}

void MachineEventTests()
{
    Ai::World world;
    Ai::MachineBuilder m("Events");
    m.State("Wait").GoTo("Alert").OnEvent(Go);
    m.State("Alert").GoTo("Wait").OnEvent(Ai::Event{"Calm"}).If(Ai::Conditions::IsTrue(Ready));
    Ai::Controller& ai = world.SpawnDirector();
    ai.SetThinkInterval(5);                           // events do not wait for the interval
    ai.Run(m.Build());
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Wait");
    ai.SendEvent(Go);
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Alert");
    ai.SendEvent(Ai::Event{"Calm"});                  // condition false: the event is used up
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Alert");
    ai.Memory().Set(Ready, true);
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Alert");              // the old event is not replayed
    ai.SendEvent(Ai::Event{"Calm"});
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Wait");
}

void MachineDelayTests()
{
    Ai::World world;
    Ai::MachineBuilder m("Delay");
    m.State("A").GoTo("B").If([](Ai::Context&) { return true; }).After(1.0f);
    m.State("B").GoTo("A").OnEvent(Go);
    Ai::Controller& ai = world.SpawnDirector();
    ai.Run(m.Build());
    world.Tick(0.1f);
    for (int i = 0; i < 8; ++i) world.Tick(0.1f);
    CHECK(ai.ActiveState() == "A");
    world.Tick(0.1f);
    world.Tick(0.1f);
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "B");
    // Leaving the state cancels a pending delay; coming back starts a new one.
    Ai::MachineBuilder c("Cancel");
    auto& start = c.State("Start");
    start.GoTo("Late").If([](Ai::Context&) { return true; }).After(0.5f);
    start.GoTo("Other").OnEvent(Go);
    c.State("Late");
    c.State("Other").GoTo("Start").OnEvent(Ai::Event{"Back"});
    Ai::Controller& late = world.SpawnDirector();
    late.Run(c.Build());
    world.Tick(0.1f);                                 // Start
    world.Tick(0.1f);                                 // the delay begins
    late.SendEvent(Go);
    world.Tick(0.1f);                                 // Other: the delay is dropped
    late.SendEvent(Ai::Event{"Back"});
    world.Tick(0.1f);                                 // Start again
    for (int i = 0; i < 4; ++i) world.Tick(0.1f);    // the old delay would have fired by now
    CHECK(late.ActiveState() == "Start");
    for (int i = 0; i < 3; ++i) world.Tick(0.1f);
    CHECK(late.ActiveState() == "Late");
}

void MachineLinkedTests()
{
    journal.clear();
    Ai::World world;
    Ai::MachineBuilder fight("Fight");
    fight.State("Aim").Task<Probe>("aim", 1).GoTo("Shoot").OnSucceeded();
    fight.State("Shoot").Task<Probe>("shoot", 1).GoTo(Ai::Target::Finish).OnSucceeded();
    const Ai::MachinePtr fightMachine = fight.Build();

    Ai::MachineBuilder m("Soldier");
    m.State("Combat").Linked(fightMachine).GoTo("Rest").OnCompleted();
    m.State("Rest");
    Ai::Controller& a = world.SpawnDirector();
    Ai::Controller& b = world.SpawnDirector();
    const Ai::MachinePtr soldier = m.Build();
    a.Run(soldier);
    b.Run(soldier);
    world.Tick(0.1f);
    CHECK(a.ActiveState() == "Combat/Aim" && b.ActiveState() == "Combat/Aim");
    world.Tick(0.1f);
    CHECK(a.ActiveState() == "Combat/Shoot");
    world.Tick(0.1f);
    CHECK(a.ActiveState() == "Rest");                // Finish inside the linked machine completed Combat
    CHECK(a.IsRunning());
    CHECK(soldier->Find("Combat/Shoot") >= 0 && soldier->Find("Shoot") >= 0);
}

void MachineTargetsTests()
{
    Ai::World world;
    Ai::MachineBuilder m("Targets");
    auto& seq = m.State("Sequence");
    seq.State("One").GoTo(Ai::Target::Next).If([](Ai::Context&) { return true; });
    seq.State("Two").GoTo(Ai::Target::Next).If([](Ai::Context&) { return true; });
    seq.State("Three").GoTo(Ai::Target::Finish).OnEvent(Go);
    Ai::Controller& ai = world.SpawnDirector();
    ai.Run(m.Build());
    world.Tick(0.1f);
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Sequence/Two");
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Sequence/Three");
    ai.SendEvent(Go);
    world.Tick(0.1f);
    CHECK(!ai.IsRunning() && ai.ActiveState().empty());
    CHECK(ai.History().back().find("(finished)") != std::string::npos);
}

void MachineBuildErrorTests()
{
    {
        Ai::MachineBuilder m("Guard");
        m.State("Chase").GoTo("Atack").OnSucceeded();
        CHECK(Throws([&] { m.Build(); }, "Guard\": transition from \"Chase\" to unknown state \"Atack\""));
    }
    {
        Ai::MachineBuilder m("Next");
        m.State("Only").GoTo(Ai::Target::Next);
        CHECK(Throws([&] { m.Build(); }, "it is the last state"));
    }
    {
        Ai::MachineBuilder linked("L");
        linked.State("X");
        Ai::MachineBuilder m("Linked");
        auto& host = m.State("Host");
        host.Linked(linked.Build());
        host.State("Own");
        CHECK(Throws([&] { m.Build(); }, "cannot have its own child states"));
    }
    {
        Ai::MachineBuilder m("Empty");
        CHECK(Throws([&] { m.Build(); }, "no states"));
    }
    {
        Ai::MachineBuilder m("Slash");
        m.State("A/B");
        CHECK(Throws([&] { m.Build(); }, "contains '/'"));
    }
    CHECK(Throws([] { Ai::Not(nullptr); }, "empty condition"));
}

void MachineDataIsPerUnitTests()
{
    Ai::World world;
    Ai::MachineBuilder m("Count");
    m.State("Count").Task<Counter>();
    const Ai::MachinePtr machine = m.Build();
    Ai::Controller& a = world.SpawnDirector();
    Ai::Controller& b = world.SpawnDirector();
    a.Run(machine);
    world.Tick(0.1f);
    world.Tick(0.1f);
    world.Tick(0.1f);
    b.Run(machine);
    world.Tick(0.1f);
    world.Tick(0.1f);
    // One shared task object, two separate Data blocks.
    CHECK(a.Memory().Get(Ai::Key<int>{"count"}) == 4);
    CHECK(b.Memory().Get(Ai::Key<int>{"count"}) == 1);
}

void ServiceAndIntervalTests()
{
    journal.clear();
    Ai::World world;
    Ai::MachineBuilder m("Services");
    m.Service<Tally>(0.5f);
    m.State("Count").Task<Counter>();
    Ai::Controller& ai = world.SpawnDirector();
    ai.Run(m.Build());
    for (int i = 0; i < 11; ++i) world.Tick(0.1f);   // first think selects, then ten thinks of 0.1 s
    CHECK(ai.Memory().Get(Ai::Key<int>{"service"}) == 2);
    CHECK(std::abs(ai.Memory().Get(Ai::Key<float>{"serviceDt"}) - 0.5f) < 1e-3f);
    ai.Stop();
    CHECK(journal.front() == "service enter" && journal.back() == "service exit");

    Ai::Controller& slow = world.SpawnDirector();
    slow.Run(m.Build());
    world.Tick(0.1f);                                 // starts at once
    slow.SetThinkInterval(0.3f);
    const int before = slow.Memory().Get(Ai::Key<int>{"count"});
    for (int i = 0; i < 30; ++i) world.Tick(0.1f);   // three seconds
    const int thinks = slow.Memory().Get(Ai::Key<int>{"count"}) - before;
    CHECK(thinks >= 9 && thinks <= 11);
}

void ExitOnDestroyTests()
{
    journal.clear();
    {
        Ai::World world;
        Ai::MachineBuilder m("Exit");
        m.State("Busy").Task<Probe>("busy");
        Ai::Controller& ai = world.SpawnDirector();
        ai.Run(m.Build());
        world.Tick(0.1f);
        Ai::Controller& other = world.SpawnDirector();
        other.Run(m.Build());
        world.Tick(0.1f);
        world.Destroy(other);
        CHECK(std::count(journal.begin(), journal.end(), "exit busy interrupted") == 1);
    }
    CHECK(std::count(journal.begin(), journal.end(), "exit busy interrupted") == 2);   // the world closed with a running task
    CHECK(Throws([] { Ai::World world; world.SpawnDirector().Run(nullptr); }, "null state machine"));
}
