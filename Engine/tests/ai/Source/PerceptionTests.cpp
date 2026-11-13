// Stage 4: sight, hearing, damage, teams, events and memory keys.
#include "TestSupport.h"

namespace
{
const Ai::Key<Ai::ObjectRef> Target{"Target"};
const Ai::Key<glm::vec3> LastSeen{"LastSeen"};

struct Recorder final : Ai::Controller
{
    using Ai::Controller::Controller;
    std::vector<Ai::Stimulus> stimuli;
    void OnPerception(const Ai::Stimulus& stimulus) override { stimuli.push_back(stimulus); }
};

// Counts events through a tiny machine: each event moves to its own state.
Ai::MachinePtr EventMachine()
{
    Ai::MachineBuilder m("Events");
    auto& any = m.State("Listen");
    any.GoTo("Spotted").OnEvent(Ai::Events::Spotted);
    any.GoTo("Lost").OnEvent(Ai::Events::Lost);
    any.GoTo("Heard").OnEvent(Ai::Events::Heard);
    any.GoTo("Damaged").OnEvent(Ai::Events::Damaged);
    any.GoTo("Alarm").OnEvent(Ai::Event{"Alarm"});
    for (const char* name : {"Spotted", "Lost", "Heard", "Damaged", "Alarm"}) m.State(name).GoTo("Listen").If([](Ai::Context&) { return true; });
    return m.Build();
}

struct Scene3
{
    Game game;
    Character* watcher = nullptr;
    Character* intruder = nullptr;
    Recorder* ai = nullptr;
    void Start(glm::vec3 watcherAt, glm::vec3 intruderAt, bool wall = false)
    {
        if (wall)
        {
            NavVolume& bounds = AddToScene<NavVolume>(game, "Bounds");
            bounds.mode = 0;
            bounds.SetScale({60, 1, 60});
            NavVolume& block = AddToScene<NavVolume>(game, "Wall", {0, 0, 5});
            block.SetScale({10, 1, 1});
        }
        watcher = &AddToScene<Character>(game, "Watcher", watcherAt);
        intruder = &AddToScene<Character>(game, "Intruder", intruderAt);
        game.InitializeGameplay();
        Ai::World& world = Ai::Get(game);
        ai = static_cast<Recorder*>(&world.Spawn(*watcher, std::make_unique<Recorder>(world)));
        ai->GetPerception().SetTeam({2});
        ai->GetPerception().EnableSight(20, 120, 2);
        ai->GetPerception().WriteTargetTo(Target);
        ai->GetPerception().WriteLastSeenTo(LastSeen);
        ai->Run(EventMachine());
        world.AddTarget(*intruder, {1});
    }
    bool History(const char* text) const
    {
        for (const auto& line : ai->History()) if (line.find(text) != std::string::npos) return true;
        return false;
    }
};
} // namespace

void SightTests()
{
    {
        Scene3 s;                                      // the watcher faces +Z: in front, 10 m
        s.Start({0, 0, 0}, {0, 0, 10});
        Step(s.game, 0.5f);
        CHECK(s.ai->GetPerception().CanSee({s.intruder->GetComponentId()}));
        CHECK(s.ai->Memory().Get(Target).id == s.intruder->GetComponentId());
        CHECK(s.History("-> Spotted"));
        CHECK(!s.ai->stimuli.empty() && s.ai->stimuli.front().kind == Ai::Stimulus::Kind::Sight);
    }
    {
        Scene3 s;                                      // behind the watcher
        s.Start({0, 0, 0}, {0, 0, -10});
        Step(s.game, 0.5f);
        CHECK(s.ai->GetPerception().Known().empty() && !s.ai->Memory().Has(Target));
    }
    {
        Scene3 s;                                      // too far
        s.Start({0, 0, 0}, {0, 0, 25});
        Step(s.game, 0.5f);
        CHECK(s.ai->GetPerception().Known().empty());
    }
    {
        Scene3 s;                                      // behind a wall
        s.Start({0, 0, 0}, {0, 0, 10}, true);
        Step(s.game, 0.5f);
        CHECK(s.ai->GetPerception().Known().empty());
    }
}

void LostAndMemoryTests()
{
    Scene3 s;
    s.Start({0, 0, 0}, {0, 0, 10});
    Step(s.game, 0.5f);
    CHECK(s.ai->GetPerception().CanSee({s.intruder->GetComponentId()}));
    s.intruder->SetPosition({3, 0, 12});
    Step(s.game, 0.3f);
    s.intruder->SetPosition({0, 0, -15});              // goes behind the watcher
    Step(s.game, 0.5f);
    CHECK(s.History("-> Lost"));
    CHECK(!s.ai->GetPerception().CanSee({s.intruder->GetComponentId()}));
    CHECK(s.ai->Memory().Has(Target));                 // remembered for two seconds
    CHECK(glm::length(s.ai->Memory().Get(LastSeen) - glm::vec3(3, 0, 12)) < 0.01f);
    Step(s.game, 2.5f);
    CHECK(!s.ai->Memory().Has(Target) && s.ai->GetPerception().Known().empty());
    CHECK(s.ai->Memory().Has(LastSeen));               // where to search stays
}

void TeamTests()
{
    {
        Scene3 s;
        s.Start({0, 0, 0}, {0, 0, 10});
        Ai::Get(s.game).AddTarget(*s.intruder, {2});   // same team as the watcher
        Step(s.game, 0.5f);
        CHECK(s.ai->GetPerception().Known().empty());
    }
    {
        Scene3 s;
        s.Start({0, 0, 0}, {0, 0, 10});
        Ai::World& world = Ai::Get(s.game);
        world.SetAttitude({1}, {2}, Ai::Attitude::Neutral);
        Step(s.game, 0.5f);
        CHECK(s.ai->GetPerception().Known().empty());
        CHECK(world.GetAttitude({2}, {1}) == Ai::Attitude::Neutral);   // symmetric
        CHECK(world.GetAttitude({0}, {1}) == Ai::Attitude::Neutral);
        CHECK(world.GetAttitude({3}, {1}) == Ai::Attitude::Hostile);
        CHECK(world.GetAttitude({3}, {3}) == Ai::Attitude::Friendly);
    }
}

void HearingDamageBroadcastTests()
{
    Scene3 s;
    s.Start({0, 0, 0}, {0, 0, -30});
    Ai::World& world = Ai::Get(s.game);
    s.ai->GetPerception().EnableHearing(15);
    world.ReportNoise({5, 0, -5}, 1.0f, {s.intruder->GetComponentId()});
    Step(s.game, 0.1f);
    CHECK(s.History("-> Heard"));
    CHECK(glm::length(s.ai->Memory().Get(LastSeen) - glm::vec3(5, 0, -5)) < 0.01f);
    const size_t before = s.ai->stimuli.size();
    world.ReportNoise({40, 0, 40}, 1.0f);              // out of range
    Step(s.game, 0.1f);
    CHECK(s.ai->stimuli.size() == before);

    float damage = 0;
    world.SetDamageHandler([&](Ai::ObjectRef victim, Ai::ObjectRef, float amount) { if (victim.id == s.watcher->GetComponentId()) damage += amount; });
    s.ai->Memory().Clear(Target);
    world.ReportDamage({s.watcher->GetComponentId()}, {s.intruder->GetComponentId()}, 12.5f);
    Step(s.game, 0.1f);
    CHECK(damage == 12.5f);
    CHECK(s.History("-> Damaged"));
    CHECK(s.ai->stimuli.back().kind == Ai::Stimulus::Kind::Damage && s.ai->stimuli.back().strength == 12.5f);
    CHECK(s.ai->Memory().Get(Target).id == s.intruder->GetComponentId());   // now it knows who hit it

    world.Broadcast(Ai::Event{"Alarm"}, {0, 0, 0}, 5);
    Step(s.game, 0.1f);
    CHECK(s.History("-> Alarm"));
}

void PerceptionBudgetTests()
{
    // Thirty watchers, two sight checks per frame: everybody still sees the target, a few frames later.
    Game game;
    std::vector<Character*> watchers;
    for (int i = 0; i < 30; ++i) watchers.push_back(&AddToScene<Character>(game, "W" + std::to_string(i), {static_cast<float>(i % 6) * 2, 0, static_cast<float>(i / 6) * 2}));
    Character& target = AddToScene<Character>(game, "Target", {5, 0, 25});
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    world.GetBudget().sightChecksPerFrame = 2;
    world.AddTarget(target, {1});
    std::vector<Ai::Controller*> ais;
    for (Character* w : watchers)
    {
        Ai::Controller& c = world.Spawn(*w);
        c.GetPerception().SetTeam({2});
        c.GetPerception().EnableSight(40, 360);
        ais.push_back(&c);
    }
    game.StepProjectSimulation(0.05f);
    size_t first = 0;
    for (auto* c : ais) first += c->GetPerception().CanSee({target.GetComponentId()});
    CHECK(first < 30);                                  // the budget split the work
    Step(game, 2.0f);
    for (auto* c : ais) CHECK(c->GetPerception().CanSee({target.GetComponentId()}));
}
