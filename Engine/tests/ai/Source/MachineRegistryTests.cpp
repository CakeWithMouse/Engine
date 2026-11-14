// Stage 11: machines built in C++ and picked by name, also from AiCharacter's property.
#include "TestSupport.h"
#include <algorithm>

namespace
{
int builds = 0;

Ai::MachinePtr OneState(const char* machine, const char* state)
{
    Ai::MachineBuilder m(machine);
    m.State(state).Task<Ai::Tasks::Wait>(100.0f);
    return m.Build();
}

Ai::MachinePtr BuildFromMacro() { return OneState("FromMacro", "Macro"); }

// The character runs a machine of its own: the property must not replace it.
class OwnMachine final : public AiCharacter
{
protected:
    void SetupAi(Ai::Controller& ai) override { ai.Run(OneState("Own", "Own")); }
};
} // namespace

// Registered while the test program starts, before main.
ENGINE_AI_MACHINE("Test.FromMacro", BuildFromMacro)

void MachineRegistryTests()
{
    CHECK(Ai::FindMachine("Test.Nobody") == nullptr);
    CHECK(Throws([] { Ai::GetMachine("Test.Nobody"); }, "No state machine named \"Test.Nobody\""));
    CHECK(Throws([] { Ai::RegisterMachine("", [] { return Ai::MachinePtr(); }); }, "empty name"));
    CHECK(Throws([] { Ai::RegisterMachine("Test.NoFactory", nullptr); }, "no factory"));

    // Built once, on first use; the same object for everybody.
    Ai::RegisterMachine("Test.Guard", [] { ++builds; return OneState("Guard", "Watch"); });
    CHECK(builds == 0);
    const Ai::MachinePtr first = Ai::GetMachine("Test.Guard");
    CHECK(first && first == Ai::FindMachine("Test.Guard") && builds == 1);
    const auto names = Ai::MachineNames();
    CHECK(std::find(names.begin(), names.end(), "Test.Guard") != names.end());
    CHECK(Throws([] { Ai::GetMachine("Test.Gaurd"); }, "Test.Guard"));      // the error lists what exists
    CHECK(Ai::FindMachine("Test.FromMacro") && Ai::FindMachine("Test.FromMacro")->Name() == "FromMacro");
    CHECK(Ai::FindMachine("Sentry") && Ai::FindMachine("Sentry")->Name() == "Sentry");   // the example of AI-CPP-GUIDE.md

    // A broken factory: a readable error, and the next call tries again.
    Ai::RegisterMachine("Test.Empty", [] { return Ai::MachinePtr(); });
    CHECK(Throws([] { Ai::FindMachine("Test.Empty"); }, "returned no machine"));
    Ai::RegisterMachine("Test.Broken", [] {
        Ai::MachineBuilder m("Broken");
        m.State("A").GoTo("Missing").OnSucceeded();
        return m.Build();
    });
    CHECK(Throws([] { Ai::FindMachine("Test.Broken"); }, "unknown state \"Missing\""));

    // A plain AiCharacter in the scene, no subclass: the property names the machine.
    {
        Game game;
        AiCharacter& guard = AddToScene<AiCharacter>(game, "Guard");
        guard.stateMachine = "Test.Guard";
        guard.sightRadius = 10;
        game.InitializeGameplay();
        game.StepProjectSimulation(0.05f);
        Ai::Controller* ai = guard.GetAi();
        CHECK(ai && ai->IsRunning() && ai->ActiveState() == "Watch");
        CHECK(builds == 1);                                    // still the machine built above
    }
    // SetupAi started its own machine: it stays.
    {
        Game game;
        OwnMachine& unit = AddToScene<OwnMachine>(game, "Unit");
        unit.stateMachine = "Test.Guard";
        game.InitializeGameplay();
        game.StepProjectSimulation(0.05f);
        CHECK(unit.GetAi() && unit.GetAi()->ActiveState() == "Own");
    }
    // A wrong name stops the start with a message that says who and what.
    {
        Game game;
        AiCharacter& lost = AddToScene<AiCharacter>(game, "Lost");
        lost.stateMachine = "Test.Gaurd";
        CHECK(Throws([&] { game.InitializeGameplay(); }, "AiCharacter (object"));
    }
    {
        Game game;
        AiCharacter& lost = AddToScene<AiCharacter>(game, "Lost");
        lost.stateMachine = "Test.Gaurd";
        CHECK(Throws([&] { game.InitializeGameplay(); }, "No state machine named \"Test.Gaurd\""));
    }
}
