// Stage 14: the AI tutorial project (Game/AiTutorial) — its scene and .fsm files run as a designer would see them.
#include "TestSupport.h"
#include <Engine/Project/StandardTypes.h>
#include <Engine/Scene/SceneDocument.h>
#include <filesystem>

namespace
{
std::filesystem::path TutorialContent()
{
    return std::filesystem::path(__FILE__).parent_path() / ".." / ".." / ".." / ".." / "Game" / "AiTutorial" / "Content";
}

AiCharacter& Unit(Game& game, const char* name)
{
    auto* unit = dynamic_cast<AiCharacter*>(game.GetScene().FindComponentByName(name));
    if (!unit) throw std::runtime_error(std::string("no AI character ") + name);
    return *unit;
}

// Runs until the state path contains the text; false after the time is up.
bool StepUntil(Game& game, Ai::Controller& ai, const std::string& state, float seconds)
{
    for (float t = 0; t < seconds; t += 0.05f)
    {
        game.StepProjectSimulation(0.05f);
        if (ai.ActiveState().find(state) != std::string::npos) return true;
    }
    return false;
}
} // namespace

void TutorialSceneTests()
{
    const auto content = TutorialContent();
    for (const char* file : {"AI/Guard.fsm", "AI/Visitor.fsm"})
    {
        const auto loaded = Ai::LoadMachineFile((content / file).u8string());
        if (!loaded.asset)
            throw std::runtime_error(std::string(file) + ": node " + std::to_string(loaded.problems.front().node) + ": " + loaded.problems.front().message);
    }

    Game game;
    const SceneSnapshot scene = SceneDocument::Read(content / "Scenes" / "Main.scene.json");
    SceneDocument::Restore(scene, SceneProject::GetComponentTypes(), game.GetScene());
    // Outside an editor project there is no /Game: the files are named by their paths on disk.
    AiCharacter& guard = Unit(game, "Охранник");
    AiCharacter& visitor = Unit(game, "Посетитель");
    CHECK(guard.stateMachine == "/Game/AI/Guard.fsm" && visitor.stateMachine == "/Game/AI/Visitor.fsm");
    guard.stateMachine = (content / "AI/Guard.fsm").u8string();
    visitor.stateMachine = (content / "AI/Visitor.fsm").u8string();
    auto* player = dynamic_cast<Character*>(game.GetScene().FindComponentByName("Игрок"));
    CHECK(player && player->playerControlled);

    game.InitializeGameplay();
    game.StepProjectSimulation(0.05f);
    Ai::Controller* guardAi = guard.GetAi();
    Ai::Controller* visitorAi = visitor.GetAi();
    CHECK(guardAi && visitorAi && guardAi->IsRunning() && visitorAi->IsRunning());
    CHECK(Ai::Get(game).IsTarget(player->GetComponentId()) && Ai::Get(game).TeamOf(player->GetComponentId()).id == AiCharacter::PlayerTeam);
    CHECK(guardAi->ActiveState().rfind("Patrol/", 0) == 0);
    CHECK(visitorAi->ActiveState() == "SitOnBench");

    // Patrol: the guard walks to post B along the gate line.
    CHECK(StepUntil(game, *guardAi, "Patrol/GoToB", 6));
    StepUntil(game, *guardAi, "never", 1.0f);
    // The player steps in front of him: he chases and catches.
    const glm::vec3 at = guard.GetWorldPosition();
    player->SetPosition({at.x + 4, player->GetWorldPosition().y, at.z});
    CHECK(StepUntil(game, *guardAi, "Chase", 2));
    CHECK(StepUntil(game, *guardAi, "Caught", 6));
    // The player runs behind the wall: the guard searches where he saw him, looks around and goes back.
    player->SetPosition({0, player->GetWorldPosition().y, 12});
    CHECK(StepUntil(game, *guardAi, "Search", 6) || guardAi->ActiveState() == "LookAround" || guardAi->ActiveState() == "Return");
    CHECK(StepUntil(game, *guardAi, "Return", 15));
    CHECK(StepUntil(game, *guardAi, "Patrol", 20));

    // The visitor went to a bench, then to the well.
    CHECK(StepUntil(game, *visitorAi, "DrinkWater", 30));
    bool history = false;
    for (const auto& line : visitorAi->History()) history = history || line.find("SitOnBench -> DrinkWater") != std::string::npos;
    CHECK(history);
}
