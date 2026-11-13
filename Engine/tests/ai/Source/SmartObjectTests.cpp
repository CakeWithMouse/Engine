// Stage 12: smart objects. A slot is claimed when the task starts, so two units never head for
// one seat; it is given back when the use ends, is interrupted or the unit disappears.
#include "TestSupport.h"
#include <Engine/AI/SmartObject.h>
#include <algorithm>
#include <functional>

namespace
{
const Ai::Event Wake{"Test.Wake"};

// Sit on the nearest free "Seat" for two seconds. No seat: NoSeat. Woken up: Awake.
Ai::MachinePtr RestMachine()
{
    Ai::MachineBuilder m("Rest");
    auto& sit = m.State("Sit");
    sit.Task<Ai::Tasks::UseSmartObject>("Seat", 2.0f, 30.0f);
    sit.GoTo("Done").OnSucceeded();
    sit.GoTo("NoSeat").OnFailed();
    sit.GoTo("Awake").OnEvent(Wake);
    m.State("Done");
    m.State("NoSeat");
    m.State("Awake");
    return m.Build();
}

class Visitor final : public AiCharacter
{
protected:
    void SetupAi(Ai::Controller& ai) override { ai.Run(RestMachine()); }
};

Ai::SmartObject& Seat(Game& game, const std::string& name, glm::vec3 at, int slots = 1, const char* tag = "Seat")
{
    Ai::SmartObject& seat = AddToScene<Ai::SmartObject>(game, name, at);
    seat.tag = tag;
    seat.slots = slots;
    return seat;
}

Visitor& Unit(Game& game, const std::string& name, glm::vec3 at)
{
    Visitor& unit = AddToScene<Visitor>(game, name, at);
    unit.SetMoveSpeed(4);
    return unit;
}

void RunUntil(Game& game, float seconds, const std::function<bool()>& done)
{
    for (float t = 0; t < seconds && !done(); t += 0.05f) game.StepProjectSimulation(0.05f);
}
} // namespace

void SmartObjectTests()
{
    // Two seats, three visitors: two sit, one is left without a seat, the seats are free again after.
    {
        Game game;
        Ai::SmartObject& closeSeat = Seat(game, "SeatNear", {8, 0, 0});
        Ai::SmartObject& farSeat = Seat(game, "SeatFar", {16, 0, 0});
        Seat(game, "Well", {-4, 0, 0}, 4, "Water");                 // another tag is never taken
        Ai::SmartObject& tooFar = Seat(game, "SeatTooFar", {60, 0, 0}); // beyond the 30 m search
        Visitor& a = Unit(game, "A", {0, 0, 0});
        Visitor& b = Unit(game, "B", {0, 0, 2});
        Visitor& c = Unit(game, "C", {0, 0, -2});
        game.InitializeGameplay();
        Ai::World& world = Ai::Get(game);
        CHECK(world.SmartObjects().size() == 4);
        CHECK(world.FindSmartObject("Seat", glm::vec3(0), 30) == &closeSeat);
        CHECK(world.FindSmartObject("Seat", glm::vec3(0), 5) == nullptr);
        game.StepProjectSimulation(0.05f);
        // Claimed at the start, while still walking: nobody else is sent to the same seat.
        CHECK(closeSeat.FreeSlots() == 0 && farSeat.FreeSlots() == 0 && tooFar.FreeSlots() == 1);
        // The failed start turns into a transition on the next think.
        Step(game, 0.2f);
        int seated = 0, without = 0;
        for (Visitor* v : {&a, &b, &c})
        {
            seated += closeSeat.IsUsedBy(v->GetAi()->Serial()) || farSeat.IsUsedBy(v->GetAi()->Serial());
            without += v->GetAi()->ActiveState() == "NoSeat";
        }
        CHECK(seated == 2 && without == 1);
        CHECK(world.FindSmartObject("Seat", glm::vec3(0), 30) == nullptr);   // both taken
        RunUntil(game, 20, [&] { return a.GetAi()->ActiveState() != "Sit" && b.GetAi()->ActiveState() != "Sit" && c.GetAi()->ActiveState() != "Sit"; });
        int done = 0;
        for (Visitor* v : {&a, &b, &c}) done += v->GetAi()->ActiveState() == "Done";
        CHECK(done == 2);
        CHECK(closeSeat.FreeSlots() == 1 && farSeat.FreeSlots() == 1);
        // They really walked there: each of the two stands at a seat.
        for (Visitor* v : {&a, &b, &c})
        {
            if (v->GetAi()->ActiveState() != "Done") continue;
            const glm::vec2 p = FlatOf(v->GetWorldPosition());
            CHECK(std::min(glm::length(p - glm::vec2(8, 0)), glm::length(p - glm::vec2(16, 0))) < 1.5f);
        }
    }
    // Interrupted on the way: the seat is given back at once and the next visitor may take it.
    {
        Game game;
        Ai::SmartObject& seat = Seat(game, "Seat", {12, 0, 0});
        Visitor& first = Unit(game, "First", {0, 0, 0});
        game.InitializeGameplay();
        game.StepProjectSimulation(0.05f);
        CHECK(seat.IsUsedBy(first.GetAi()->Serial()));
        first.GetAi()->SendEvent(Wake);
        Step(game, 0.2f);
        CHECK(first.GetAi()->ActiveState() == "Awake");
        CHECK(seat.FreeSlots() == 1);
        CHECK(first.GetAi()->GetMoveStatus() != Ai::MoveStatus::Moving);     // it stopped walking there
    }
    // The visitor disappears: its slot is not kept forever.
    {
        Game game;
        Ai::SmartObject& seat = Seat(game, "Seat", {12, 0, 0});
        Visitor& gone = Unit(game, "Gone", {0, 0, 0});
        game.InitializeGameplay();
        game.StepProjectSimulation(0.05f);
        CHECK(seat.FreeSlots() == 0);
        CHECK(game.GetScene().RemoveComponent(gone.GetComponentId()) != SceneResult::NotFound);
        Step(game, 0.2f);
        CHECK(seat.FreeSlots() == 1);
    }
    // The seat disappears while the visitor walks to it: the use fails, nothing hangs.
    {
        Game game;
        Ai::SmartObject& seat = Seat(game, "Seat", {12, 0, 0});
        Visitor& unit = Unit(game, "Unit", {0, 0, 0});
        game.InitializeGameplay();
        game.StepProjectSimulation(0.05f);
        CHECK(game.GetScene().RemoveComponent(seat.GetComponentId()) != SceneResult::NotFound);
        Step(game, 0.3f);
        CHECK(unit.GetAi()->ActiveState() == "NoSeat");
        CHECK(Ai::Get(game).SmartObjects().empty());
    }
    // Several slots: one bench seats three.
    {
        Game game;
        Ai::SmartObject& bench = Seat(game, "Bench", {10, 0, 0}, 3);
        Visitor& a = Unit(game, "A", {0, 0, 0});
        Visitor& b = Unit(game, "B", {0, 0, 3});
        Visitor& c = Unit(game, "C", {0, 0, -3});
        game.InitializeGameplay();
        game.StepProjectSimulation(0.05f);
        CHECK(bench.FreeSlots() == 0);
        for (Visitor* v : {&a, &b, &c}) CHECK(bench.IsUsedBy(v->GetAi()->Serial()));
        CHECK(!bench.Claim(9999) && bench.Claim(a.GetAi()->Serial()));        // full; a second claim by the same unit is fine
    }
}
