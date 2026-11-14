// Stage 15: visual scripts and .fsm machines talk to each other (nodes "Ai.*" in scripts, blocks Ai.CallScriptEvent
// and Ai.ScriptVariable in machines).
#include "TestSupport.h"
#include <Engine/Script/ScriptRuntime.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace
{
using Json = nlohmann::json;

struct Graph
{
    Json nodes = Json::array(), links = Json::array();
    int Node(const std::string& type, Json properties = Json::object())
    {
        const int id = static_cast<int>(nodes.size()) + 1;
        nodes.push_back({{"id", id}, {"type", type}, {"properties", std::move(properties)}});
        return id;
    }
    void Link(int from, const char* fromPin, int to, const char* toPin)
    {
        links.push_back({{"fromNode", from}, {"fromPin", fromPin}, {"toNode", to}, {"toPin", toPin}});
    }
};

void Write(const std::filesystem::path& file, const Json& json) { std::ofstream(file, std::ios::binary) << json.dump(2); }

Json State(int id, const char* name, double y, Json tasks)
{
    return {{"id", id}, {"type", "Fsm.State"}, {"x", 0}, {"y", y}, {"properties", {{"name", name}, {"tasks", std::move(tasks)}}}};
}
} // namespace

void ScriptLinkTests()
{
    const auto folder = std::filesystem::temp_directory_path() / "ai-script-link-tests";
    std::filesystem::create_directories(folder);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove_all(path, e); } } cleanup{folder};

    // The guard's script: every 0.2 s it raises the alarm and writes Lives; "OnAlarm" (called by the machine) sets Ready;
    // every frame it reads Lives and whether the machine is in "Ready".
    Graph script;
    const int every = script.Node("Event.Every", {{"seconds", 0.2}});
    const int send = script.Node("Ai.SendEvent", {{"event", "Alarm"}});
    const int set = script.Node("Ai.SetMemory", {{"memory", "Lives"}, {"inputs", {{"Value", 2}}}});
    script.Link(every, "then", send, "exec");
    script.Link(send, "then", set, "exec");
    const int onAlarm = script.Node("Event.Custom", {{"event", "OnAlarm"}});
    const int ready = script.Node("Variable.Set", {{"variable", "Ready"}, {"inputs", {{"Value", true}}}});
    script.Link(onAlarm, "then", ready, "exec");
    const int tick = script.Node("Event.Tick");
    const int seen = script.Node("Variable.Set", {{"variable", "Seen"}});
    const int lives = script.Node("Ai.GetNumber", {{"memory", "Lives"}});
    const int inReady = script.Node("Variable.Set", {{"variable", "InReady"}});
    const int inState = script.Node("Ai.InState", {{"inputs", {{"State", "Ready"}}}});
    const int stateName = script.Node("Variable.Set", {{"variable", "State"}});
    const int state = script.Node("Ai.State");
    script.Link(tick, "then", seen, "exec");
    script.Link(lives, "Value", seen, "Value");
    script.Link(seen, "then", inReady, "exec");
    script.Link(inState, "Value", inReady, "Value");
    script.Link(inReady, "then", stateName, "exec");
    script.Link(state, "State", stateName, "Value");
    Write(folder / "Guard.vscript", {{"format", "ITMOEngine.Graph"}, {"version", 1}, {"schema", "code"}, {"schemaVersion", 1},
        {"variables", {{{"name", "Ready"}, {"type", "bool"}, {"value", false}}, {{"name", "Seen"}, {"type", "float"}, {"value", 0}},
                       {{"name", "InReady"}, {"type", "bool"}, {"value", false}}, {{"name", "State"}, {"type", "string"}, {"value", ""}}}},
        {"nodes", script.nodes}, {"connections", script.links}});
    const auto compiled = Script::Compile([&] { std::ifstream in(folder / "Guard.vscript"); return std::string(std::istreambuf_iterator<char>(in), {}); }());
    if (!compiled.program) throw std::runtime_error("script: node " + std::to_string(compiled.problems.front().node) + ": " + compiled.problems.front().message);

    // The machine: an "Alarm" event wakes it, it calls the script's OnAlarm, then waits for the script's Ready.
    Write(folder / "Guard.fsm", {{"format", "ITMOEngine.Graph"}, {"version", 1}, {"schema", "ai.fsm"}, {"schemaVersion", 1},
        {"variables", {{{"name", "Lives"}, {"type", "int"}, {"value", 5}}}},
        {"nodes", {State(1, "Idle", 0, {{{"type", "Ai.Wait"}, {"seconds", 100}}}),
                   State(2, "Alarmed", 100, {{{"type", "Ai.CallScriptEvent"}, {"event", "OnAlarm"}}}),
                   State(3, "Calm", 200, {{{"type", "Ai.Wait"}, {"seconds", 100}}}),
                   State(4, "Ready", 300, {{{"type", "Ai.Wait"}, {"seconds", 100}}})}},
        {"connections", {{{"fromNode", 1}, {"fromPin", "out"}, {"toNode", 2}, {"toPin", "in"}, {"properties", {{"trigger", "event"}, {"event", "Alarm"}}}},
                         {{"fromNode", 2}, {"fromPin", "out"}, {"toNode", 3}, {"toPin", "in"}, {"properties", {{"trigger", "succeeded"}}}},
                         {{"fromNode", 3}, {"fromPin", "out"}, {"toNode", 4}, {"toPin", "in"},
                          {"properties", {{"trigger", "tick"}, {"conditions", {{{"type", "Ai.ScriptVariable"}, {"variable", "Ready"}, {"op", "=="}, {"value", 1}}}}}}}}}});

    Game game;
    AiCharacter& guard = AddToScene<AiCharacter>(game, "Guard");
    guard.stateMachine = (folder / "Guard.fsm").u8string();
    guard.SetScriptAsset((folder / "Guard.vscript").u8string());
    game.InitializeGameplay();
    Step(game, 1.0f);
    Ai::Controller* ai = guard.GetAi();
    const Script::Instance* instance = guard.GetScriptInstance();
    CHECK(ai && instance);
    if (instance->Failed()) throw std::runtime_error("script failed: " + instance->Error().message);
    CHECK(ai->ActiveState() == "Ready");                                          // event -> script event -> script variable
    CHECK(std::get<bool>(instance->GetVariable("Ready")));
    CHECK(ai->Memory().Get(Ai::Key<int>{"Lives"}) == 2 && !ai->Memory().Has(Ai::Key<float>{"Lives"}));   // a whole-number key stays int
    CHECK(std::get<float>(instance->GetVariable("Seen")) == 2.0f);               // read back through "AI memory: number"
    CHECK(std::get<bool>(instance->GetVariable("InReady")) && std::get<std::string>(instance->GetVariable("State")) == "Ready");
    bool called = false;
    for (const auto& line : ai->History()) called = called || line.find("Alarmed -> Calm") != std::string::npos;
    CHECK(called);

    // A script that talks to an object without AI stops with a readable error; a machine whose script has no such event fails the task.
    Graph wrong;
    const int begin = wrong.Node("Event.BeginPlay");
    const int tell = wrong.Node("Ai.SendEvent", {{"event", "Alarm"}});
    wrong.Link(begin, "then", tell, "exec");
    Write(folder / "Box.vscript", {{"format", "ITMOEngine.Graph"}, {"version", 1}, {"schema", "code"}, {"schemaVersion", 1},
                                   {"variables", Json::array()}, {"nodes", wrong.nodes}, {"connections", wrong.links}});
    Json lonely = {{"format", "ITMOEngine.Graph"}, {"version", 1}, {"schema", "ai.fsm"}, {"schemaVersion", 1}, {"variables", Json::array()},
                   {"nodes", {State(1, "Call", 0, {{{"type", "Ai.CallScriptEvent"}, {"event", "Missing"}}}), State(2, "Failed", 100, {{{"type", "Ai.Wait"}, {"seconds", 100}}}),
                              State(3, "Done", 200, {{{"type", "Ai.Wait"}, {"seconds", 100}}})}},
                   {"connections", {{{"fromNode", 1}, {"fromPin", "out"}, {"toNode", 2}, {"toPin", "in"}, {"properties", {{"trigger", "failed"}}}},
                                    {{"fromNode", 1}, {"fromPin", "out"}, {"toNode", 3}, {"toPin", "in"}, {"properties", {{"trigger", "succeeded"}}}}}}};
    Write(folder / "Lonely.fsm", lonely);
    Game other;
    GameComponent& box = AddToScene<GameComponent>(other, "Box");
    box.SetScriptAsset((folder / "Box.vscript").u8string());
    AiCharacter& lonelyUnit = AddToScene<AiCharacter>(other, "Lonely");
    lonelyUnit.stateMachine = (folder / "Lonely.fsm").u8string();
    other.InitializeGameplay();
    Step(other, 0.3f);
    CHECK(box.GetScriptInstance() && box.GetScriptInstance()->Failed() &&
          box.GetScriptInstance()->Error().message == "the object is not an AI character");
    CHECK(lonelyUnit.GetAi() && lonelyUnit.GetAi()->ActiveState() == "Failed");   // no script at all: the task fails
}
