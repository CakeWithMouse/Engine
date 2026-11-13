// Stage 5: the real SaveGame captures and restores AI state; references are remapped to new objects.
#include "TestSupport.h"
#include <Engine/Components/ComponentType.h>
#include <Engine/SaveGame/SaveGame.h>
#include <filesystem>

namespace
{
const Ai::Key<Ai::ObjectRef> Friend{"Friend"};
const Ai::Key<int> Counter{"Counter"};
const Ai::Key<glm::vec3> Mark{"Mark"};

class Keeper final : public Ai::Controller
{
public:
    using Ai::Controller::Controller;
    int visits = 0;
    void SerializeSaveGame(SaveArchive& archive) override
    {
        Ai::Controller::SerializeSaveGame(archive);
        archive.Field("visits", visits);
    }
};

Ai::MachinePtr PatrolMachine()
{
    static const Ai::MachinePtr machine = [] {
        Ai::MachineBuilder m("Patrol");
        auto& patrol = m.State("Patrol");
        patrol.State("Walk").Task<Ai::Tasks::FollowRoute>().GoTo("Rest").OnSucceeded();
        patrol.State("Rest").Task<Ai::Tasks::Wait>(3.0f).GoTo("Walk").OnCompleted();
        return m.Build();
    }();
    return machine;
}

class Saver final : public AiCharacter
{
protected:
    std::unique_ptr<Ai::Controller> CreateController(Ai::World& world) override { return std::make_unique<Keeper>(world); }
    void SetupAi(Ai::Controller& ai) override
    {
        ai.SetRoute({{10, 0, 0}, {10, 0, 10}, {0, 0, 10}});
        ai.Run(PatrolMachine());
    }
};

const ComponentType types[] = {
    {"Test.Saver", "Saver", EngineComponentDetail::Create<Saver>},
    {"Test.Character", "Character", EngineComponentDetail::Create<Character>},
};

template<class T> T& AddTyped(Game& game, const std::string& name, const char* type, glm::vec3 position)
{
    std::unique_ptr<GameComponent> object = std::make_unique<T>();
    T& typed = static_cast<T&>(*object);
    typed.SetPosition(position);
    if (game.GetScene().AddComponent(name, object, type).result != SceneResult::Ok) throw std::runtime_error("AddComponent failed");
    return typed;
}
} // namespace

void SaveAndLoadTests()
{
    GameSession session;
    std::string error;
    CHECK(session.Initialize(error));
    Game game;
    game.AttachSession(session);
    game.Saves().SetCatalog({types, std::size(types)});
    game.Saves().Configure("ai-save-tests", std::filesystem::temp_directory_path() / "ai-save-tests");
    // A loaded scene numbers its objects from 1. Shift the original IDs so the remap really matters.
    for (int i = 0; i < 3; ++i)
    {
        Character& spacer = AddTyped<Character>(game, "Spacer", "Test.Character", {50, 0, 50});
        CHECK(game.GetScene().RemoveComponent(spacer.GetComponentId()) == SceneResult::Ok);
    }
    Saver& saver = AddTyped<Saver>(game, "Saver", "Test.Saver", {0, 0, 0});
    saver.SetMoveSpeed(5);
    Character& buddy = AddTyped<Character>(game, "Buddy", "Test.Character", {-3, 0, 2});
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);

    // A director named "script" in its second state.
    Ai::MachineBuilder script("Script");
    script.State("A").GoTo("B").If([](Ai::Context&) { return true; });
    script.State("B").GoTo("C").OnEvent(Ai::Event{"Next"});
    script.State("C");
    Ai::Controller& director = world.SpawnDirector("script");
    director.Run(script.Build());

    // Walk to point 0 (2 s), rest 3 s, then on the way to point 1.
    Step(game, 6.0f);
    auto* keeper = dynamic_cast<Keeper*>(saver.GetAi());
    CHECK(keeper && keeper->ActiveState() == "Patrol/Walk" && keeper->RouteIndex() == 1);
    CHECK(keeper->GetMoveStatus() == Ai::MoveStatus::Moving);
    CHECK(director.ActiveState() == "B");
    keeper->Memory().Set(Friend, Ai::ObjectRef{buddy.GetComponentId()});
    keeper->Memory().Set(Counter, 7);
    keeper->Memory().Set(Mark, glm::vec3(1, 2, 3));
    keeper->visits = 3;
    const ComponentId oldBuddy = buddy.GetComponentId();
    const glm::vec3 savedAt = saver.GetWorldPosition();

    const std::string snapshot = game.Saves().Capture();

    // Change everything, then load.
    keeper->Memory().Set(Counter, 99);
    keeper->visits = 50;
    director.SendEvent(Ai::Event{"Next"});
    Step(game, 4.0f);
    CHECK(director.ActiveState() == "C" && keeper->ActiveState() == "Patrol/Rest");
    game.Saves().Restore(snapshot);
    CHECK(director.ActiveState() == "B");                          // directors take their state at once
    CHECK(glm::length(game.GetScene().FindComponentByName("Saver")->GetWorldPosition() - savedAt) < 0.01f);
    game.StepProjectSimulation(0.05f);                              // restored objects get BeginPlay

    auto* loaded = dynamic_cast<Saver*>(game.GetScene().FindComponentByName("Saver"));
    auto* loadedBuddy = game.GetScene().FindComponentByName("Buddy");
    CHECK(loaded && loadedBuddy);
    CHECK(loadedBuddy->GetComponentId() != oldBuddy);              // new objects, new IDs
    auto* restored = dynamic_cast<Keeper*>(loaded->GetAi());
    CHECK(restored && restored != keeper);
    CHECK(restored->ActiveState() == "Patrol/Walk");
    CHECK(restored->Memory().Get(Counter) == 7);
    CHECK(restored->Memory().Get(Mark) == glm::vec3(1, 2, 3));
    CHECK(restored->Memory().Get(Friend).id == loadedBuddy->GetComponentId());   // the reference was remapped
    CHECK(restored->visits == 3);                                  // a field of the game's controller
    CHECK(restored->RouteIndex() == 1);
    CHECK(world.Controllers().size() == 2);                        // the old unit's controller is gone

    // It keeps walking to point 1.
    Step(game, 3.0f);
    CHECK(glm::length(FlatOf(loaded->GetWorldPosition()) - glm::vec2(10, 10)) < 0.6f);
}
