// Stage 13: machines from .fsm files of the AI editor, built-in and game blocks.
#include "TestSupport.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
using Json = nlohmann::json;

// A game block: counts its runs in memory and finishes after "ticks" thinks.
struct BumpData { int ticks = 0; };
class Bump final : public Ai::Task<BumpData>
{
public:
    Bump(std::string key, int ticks) : key_(std::move(key)), ticks_(ticks) {}
    Ai::Status Enter(Ai::Context& ctx, BumpData& data) const override
    {
        data.ticks = 0;
        ctx.Memory().Set(Ai::Key<int>{key_.c_str()}, ctx.Memory().Get(Ai::Key<int>{key_.c_str()}) + 1);
        return ticks_ == 0 ? Ai::Status::Succeeded : Ai::Status::Running;
    }
    Ai::Status Tick(Ai::Context&, BumpData& data, float) const override { return ++data.ticks >= ticks_ ? Ai::Status::Succeeded : Ai::Status::Running; }
private:
    std::string key_;
    int ticks_;
};

class Stamp final : public Ai::Service<>
{
public:
    explicit Stamp(std::string key) : key_(std::move(key)) {}
    void Tick(Ai::Context& ctx, Ai::NoData&, float) const override { ctx.Memory().Set(Ai::Key<float>{key_.c_str()}, ctx.Memory().Get(Ai::Key<float>{key_.c_str()}) + 1); }
private:
    std::string key_;
};
} // namespace

ENGINE_AI_BLOCK(Ai::TaskBlock("Test.Bump", "Счёт", "Bump",
    {Ai::Param::Key("key", Ai::ParamType::KeyInt, "Счётчик", "Counter"), Ai::Param::Int("ticks", "Тиков", "Ticks", 1, 0, 100)},
    [](const Ai::Params& p) { return std::make_shared<Bump>(p.String("key"), p.Int("ticks")); }))
ENGINE_AI_BLOCK(Ai::ServiceBlock("Test.Stamp", "Отметка", "Stamp", {Ai::Param::Key("key", Ai::ParamType::KeyFloat, "Ключ", "Key")},
    [](const Ai::Params& p) { return std::make_shared<Stamp>(p.String("key")); }))

namespace
{
Json Machine()
{
    return {{"format", "ITMOEngine.Graph"}, {"version", 1}, {"schema", "ai.fsm"}, {"schemaVersion", 1},
            {"variables", Json::array()}, {"nodes", Json::array()}, {"connections", Json::array()}};
}

Json StateNode(int id, const std::string& name, double y, Json properties = Json::object(), double x = 0)
{
    properties["name"] = name;
    return {{"id", id}, {"type", "Fsm.State"}, {"title", name}, {"x", x}, {"y", y}, {"properties", std::move(properties)}};
}

Json LinkOf(int from, int to, Json properties = Json::object())
{
    Json link{{"fromNode", from}, {"fromPin", "out"}, {"toNode", to}, {"toPin", "in"}};
    if (!properties.empty()) link["properties"] = std::move(properties);
    return link;
}

Json Variable(const std::string& name, const std::string& type) { return {{"name", name}, {"type", type}}; }
Json Variable(const std::string& name, const std::string& type, Json value) { return {{"name", name}, {"type", type}, {"value", std::move(value)}}; }

Ai::MachineLoadResult Load(const Json& file, const Ai::MachineFileReader& reader = {}) { return Ai::LoadMachine(file.dump(), "Test", reader); }

std::shared_ptr<const Ai::MachineAsset> Built(const Json& file, const Ai::MachineFileReader& reader = {})
{
    Ai::MachineLoadResult result = Load(file, reader);
    if (!result.asset)
    {
        std::string all;
        for (const auto& problem : result.problems) all += "\n  node " + std::to_string(problem.node) + ": " + problem.message;
        throw std::runtime_error("the machine did not load:" + all);
    }
    return result.asset;
}

bool HasProblem(const Ai::MachineLoadResult& result, int node, const std::string& fragment)
{
    return std::any_of(result.problems.begin(), result.problems.end(),
                       [&](const Ai::MachineProblem& p) { return p.node == node && p.message.find(fragment) != std::string::npos; });
}

int Count(const Ai::Controller& ai, const char* key) { return ai.Memory().Get(Ai::Key<int>{key}); }

void CatalogChecks()
{
    CHECK(Ai::FindBlock("Ai.Wait") && Ai::FindBlock("Ai.Wait")->kind == Ai::BlockKind::Task);
    CHECK(Ai::FindBlock("Ai.CanSee") && Ai::FindBlock("Ai.CanSee")->kind == Ai::BlockKind::Condition);
    CHECK(Ai::FindBlock("Test.Bump") && Ai::FindBlock("Test.Stamp")->kind == Ai::BlockKind::Service);   // registered before main
    const auto blocks = Ai::Blocks();
    CHECK(std::is_sorted(blocks.begin(), blocks.end(), [](auto* a, auto* b) { return a->type < b->type; }));
    for (const auto* block : blocks) CHECK(!block->nameRu.empty() && !block->nameEn.empty());

    // The catalog for the editor: node types, triggers and every block with typed parameters.
    const Json catalog = Json::parse(Ai::MachineCatalogJson());
    CHECK(catalog["schema"] == "ai.fsm" && catalog["nodes"].size() == 3 && catalog["triggers"].size() == 5);
    bool moveTo = false, bump = false;
    for (const auto& block : catalog["blocks"])
    {
        if (block["type"] == "Ai.MoveTo")
        {
            moveTo = block["engine"] == true && block["kind"] == "task" && block["params"][0]["type"] == "key:place" &&
                     block["params"][0]["required"] == true && block["params"][1]["default"] == 1.0 && block["params"][1]["min"] == 0.0;
        }
        if (block["type"] == "Test.Bump") bump = block["engine"] == false && block["params"][1]["max"] == 100.0;
    }
    CHECK(moveTo && bump);

    // Registration errors are readable.
    auto factory = [](const Ai::Params&) { return std::make_shared<Ai::Tasks::Wait>(1.0f); };
    CHECK(Throws([&] { Ai::RegisterBlock(Ai::TaskBlock("Ai.Mine", "М", "M", {}, factory)); }, "belong to the engine"));
    CHECK(Throws([&] { Ai::RegisterBlock(Ai::TaskBlock("", "М", "M", {}, factory)); }, "empty block name"));
    CHECK(Throws([&] { Ai::RegisterBlock(Ai::ConditionBlock("Test.NoFactory", "М", "M", {}, nullptr)); }, "no factory"));
    CHECK(Throws([&] { Ai::RegisterBlock(Ai::TaskBlock("Test.Twice", "М", "M", {Ai::Param::Float("a", "", ""), Ai::Param::Float("a", "", "")}, factory)); }, "two parameters"));
    CHECK(Throws([&] { Ai::RegisterBlock(Ai::TaskBlock("Test.Reserved", "М", "M", {Ai::Param::Float("not", "", "")}, factory)); }, "reserved"));
    CHECK(Throws([&] { Ai::Param::Key("k", Ai::ParamType::Float, "", ""); }, "Key types"));
    CHECK(Ai::IsMachineFile("/Game/AI/Guard.fsm") && Ai::IsMachineFile("guard.FSM") && !Ai::IsMachineFile("Guard") && !Ai::IsMachineFile("fsm"));
}

// Patrol: Walk (Bump, 2 thinks) -> Rest (Wait) on success; Rest -> Walk after 0.3 s; children ordered by y.
void RunChecks()
{
    Json file = Machine();
    file["variables"] = {Variable("walks", "int", 0), Variable("speed", "float", 2.5), Variable("home", "vec3", {1, 2, 3}), Variable("target", "object")};
    file["settings"] = {{"targetKey", "target"}, {"thinkInterval", 0.0}};
    file["nodes"] = {
        StateNode(1, "Patrol", 0),
        StateNode(3, "Rest", 200, {{"parent", 1}, {"tasks", {{{"type", "Ai.Wait"}, {"seconds", 100}}}}}),
        StateNode(2, "Walk", 100, {{"parent", 1}, {"tasks", {{{"type", "Test.Bump"}, {"key", "walks"}, {"ticks", 2}}}}}),
    };
    file["connections"] = {LinkOf(2, 3, {{"trigger", "succeeded"}}), LinkOf(3, 2, {{"trigger", "tick"}, {"conditions", {{{"type", "Ai.TimeInState"}, {"seconds", 0.3}}}}})};
    const auto asset = Built(file);
    CHECK(asset->machine->Name() == "Test" && asset->machine->Find("Patrol/Walk") >= 0);
    CHECK(asset->memory.size() == 3 && asset->variables.size() == 4 && asset->variables[3].second == Ai::VariableType::Object);
    CHECK(asset->targetKey == "target" && asset->thinkInterval == 0);

    Ai::World world;
    Ai::Controller& ai = world.SpawnDirector("patrol");
    asset->Run(ai, "not json");                                                // ignored
    CHECK(ai.Memory().Get(Ai::Key<float>{"speed"}) == 2.5f && ai.Memory().Get(Ai::Key<glm::vec3>{"home"}) == glm::vec3(1, 2, 3));
    CHECK(!ai.Memory().Has(Ai::Key<Ai::ObjectRef>{"target"}));                 // an object has no starting value
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Patrol/Walk" && Count(ai, "walks") == 1);      // Walk is drawn above Rest
    world.Tick(0.1f);
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Patrol/Rest");
    for (int i = 0; i < 4; ++i) world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Patrol/Walk" && Count(ai, "walks") == 2);

    // Moving Rest above Walk changes which child starts.
    file["nodes"][1]["y"] = 50;
    Ai::Controller& other = world.SpawnDirector("rest first");
    Built(file)->Run(other, R"({"speed": 4, "home": [0, 0, 7], "walks": 1.5})");   // an int key takes no fraction
    CHECK(other.Memory().Get(Ai::Key<float>{"speed"}) == 4.0f && other.Memory().Get(Ai::Key<glm::vec3>{"home"}) == glm::vec3(0, 0, 7));
    CHECK(Count(other, "walks") == 0);
    world.Tick(0.1f);
    CHECK(other.ActiveState() == "Patrol/Rest");
}

// Next and Finish targets, events, Ai.SetValue / Ai.ClearKey, "not", Ai.Any, priorities, delays, services.
void TargetsAndConditionsChecks()
{
    Json file = Machine();
    file["variables"] = {Variable("done", "bool"), Variable("count", "int"), Variable("stamps", "float"), Variable("label", "string", "a")};
    file["nodes"] = {
        StateNode(1, "First", 0, {{"tasks", {{{"type", "Ai.SetValue"}, {"key", "count"}, {"value", 5}}, {{"type", "Ai.ClearKey"}, {"key", "label"}},
                                            {{"type", "Ai.SendEvent"}, {"event", "Go"}}}},
                                  {"services", {{{"type", "Test.Stamp"}, {"key", "stamps"}, {"interval", 0}}}}}),
        StateNode(2, "Second", 100, {{"tasks", {{{"type", "Ai.Wait"}, {"seconds", 100}}}}}),   // a state completes with its first finished task
        StateNode(3, "Third", 200, {{"tasks", {{{"type", "Ai.SetValue"}, {"key", "done"}, {"value", true}}}}}),
        {{"id", 8}, {"type", "Fsm.Next"}},
        {{"id", 9}, {"type", "Fsm.Finish"}},
    };
    Json any = {{"type", "Ai.Any"}, {"items", {{{"type", "Ai.IsTrue"}, {"key", "done"}}, {{"type", "Ai.Compare"}, {"key", "count"}, {"op", ">="}, {"value", 5}}}}};
    Json absent = {{"type", "Ai.HasKey"}, {"key", "label"}, {"not", true}};
    file["connections"] = {
        LinkOf(1, 3, {{"trigger", "event"}, {"event", "Go"}, {"priority", 2}}),   // also fires on Go, but priority 1 is checked first
        LinkOf(1, 8, {{"trigger", "event"}, {"event", "Go"}, {"priority", 1}, {"conditions", {any}}}),
        LinkOf(2, 8, {{"trigger", "tick"}, {"conditions", {absent}}, {"delay", 0.25}}),
        LinkOf(3, 9),
    };
    Ai::World world;
    Ai::Controller& ai = world.SpawnDirector("targets");
    Built(file)->Run(ai);
    CHECK(ai.Memory().Get(Ai::Key<std::string>{"label"}) == "a");
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "First" && Count(ai, "count") == 5 && !ai.Memory().Has(Ai::Key<std::string>{"label"}));
    world.Tick(0.1f);                                      // the event arrives: Any holds, Next wins by priority
    CHECK(ai.ActiveState() == "Second");
    CHECK(ai.Memory().Get(Ai::Key<float>{"stamps"}) >= 1);
    world.Tick(0.1f);
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "Second");                   // the delay is still running
    for (int i = 0; i < 3; ++i) world.Tick(0.1f);
    CHECK(!ai.IsRunning() && ai.Memory().Get(Ai::Key<bool>{"done"}));   // Third ran, then Finish
}

// Linked machines: by C++ name and by .fsm path through the reader; loops are refused.
void LinkedChecks()
{
    Ai::RegisterMachine("Test.Inner", [] {
        Ai::MachineBuilder m("Inner");
        m.State("Deep").Task<Ai::Tasks::Wait>(100.0f);
        return m.Build();
    });
    Json inner = Machine();
    inner["nodes"] = {StateNode(1, "Leaf", 0, {{"tasks", {{{"type", "Ai.Wait"}, {"seconds", 0.15}}}}}), {{"id", 2}, {"type", "Fsm.Finish"}}};
    inner["connections"] = Json::array({LinkOf(1, 2)});
    Json outer = Machine();
    outer["variables"] = Json::array({Variable("after", "int")});
    outer["nodes"] = {StateNode(1, "ByName", 0, {{"linked", "Test.Inner"}}), StateNode(2, "ByFile", 100, {{"linked", "/Game/AI/Inner.fsm"}}),
                      StateNode(3, "After", 200, {{"tasks", {{{"type", "Test.Bump"}, {"key", "after"}, {"ticks", 100}}}}})};
    outer["connections"] = Json::array({LinkOf(2, 3)});
    std::vector<std::string> read;
    Ai::MachineFileReader reader = [&](const std::string& path, std::string& text) {
        read.push_back(path);
        if (path != "/Game/AI/Inner.fsm") return false;
        text = inner.dump();
        return true;
    };
    const auto asset = Built(outer, reader);
    CHECK(read == std::vector<std::string>{"/Game/AI/Inner.fsm"});
    CHECK(asset->machine->Find("ByName/Deep") >= 0 && asset->machine->Find("ByFile/Leaf") >= 0);

    // Finish inside the linked file completes the state that links it.
    outer["nodes"][0]["properties"]["linked"] = "";
    outer["nodes"][0]["properties"]["enterIf"] = Json::array({{{"type", "Ai.Chance"}, {"probability", 0}}});
    Ai::World world;
    Ai::Controller& ai = world.SpawnDirector("linked");
    Built(outer, reader)->Run(ai);
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "ByFile/Leaf");
    world.Tick(0.1f);
    world.Tick(0.1f);
    CHECK(ai.ActiveState() == "After" && Count(ai, "after") == 1);

    // Problems of links.
    outer["nodes"][0]["properties"]["linked"] = "Test.Nobody";
    outer["nodes"][1]["properties"]["linked"] = "/Game/AI/Missing.fsm";
    auto result = Load(outer, reader);
    CHECK(!result.asset && HasProblem(result, 1, "no state machine named \"Test.Nobody\"") && HasProblem(result, 2, "cannot read the file"));
    CHECK(HasProblem(Load(outer), 2, "cannot be read here"));
    Json self = Machine();
    self["nodes"] = Json::array({StateNode(1, "Again", 0, {{"linked", "/Game/AI/Self.fsm"}})});
    Ai::MachineFileReader loop = [&](const std::string&, std::string& text) { text = self.dump(); return true; };
    result = Load(self, loop);
    CHECK(HasProblem(result, 1, "linked \"/Game/AI/Self.fsm\" has problems") && HasProblem(result, 1, "loop"));
    Json broken = Machine();
    broken["nodes"] = {StateNode(1, "Kid", 0, {{"parent", 2}}), StateNode(2, "Host", 100, {{"linked", "Test.Inner"}})};
    CHECK(HasProblem(Load(broken), 2, "linked state cannot have its own child states"));
}

// One file with many mistakes: every one is reported, at its node.
void ProblemChecks()
{
    Json file = Machine();
    file["variables"] = {Variable("flag", "bool"), Variable("where", "vec3"), Variable("who", "object", 1), Variable("flag", "int"),
                         Variable("odd", "matrix"), Variable("n", "int", "five")};
    file["settings"] = {{"targetKey", "where"}, {"thinkInterval", -1}, {"colour", 1}};
    file["nodes"] = {
        StateNode(1, "A", 0, {{"tasks", {{{"type", "Ai.Fly"}}, {{"type", "Ai.Wait"}, {"seconds", -1}, {"secs", 1}},
                                         {{"type", "Ai.MoveTo"}}, {{"type", "Ai.MoveTo"}, {"target", "flag"}}, {{"type", "Ai.CanSee"}, {"target", "who"}},
                                         {{"type", "Ai.SetValue"}, {"key", "flag"}, {"value", 3}}, {{"type", "Ai.WaitRandom"}, {"minimum", 3}, {"maximum", 1}}}}}),
        StateNode(2, "A", 100, {{"enterIf", {{{"type", "Ai.IsTrue"}, {"key", "nothing"}}, {{"type", "Ai.Any"}, {"items", Json::array()}},
                                            {{"type", "Ai.Compare"}, {"key", "where"}}}}}),
        StateNode(3, "", 200),
        StateNode(4, "Loop1", 300, {{"parent", 5}}),
        StateNode(5, "Loop2", 400, {{"parent", 4}}),
        StateNode(6, "Last", 500, {{"parent", 99}}),
        {{"id", 7}, {"type", "Fsm.Teleport"}},
        {{"id", 8}, {"type", "Fsm.Next"}},
        {{"id", 0}, {"type", "Fsm.State"}},
    };
    file["connections"] = {LinkOf(6, 8), LinkOf(1, 2, {{"trigger", "sometimes"}}), LinkOf(1, 2, {{"trigger", "event"}}), LinkOf(1, 2, {{"delay", -2}}),
                           LinkOf(8, 1), LinkOf(1, 42)};
    const auto result = Load(file);
    CHECK(!result.asset);
    CHECK(HasProblem(result, 0, "variable \"who\": an object has no starting value"));
    CHECK(HasProblem(result, 0, "variable \"flag\" is declared twice"));
    CHECK(HasProblem(result, 0, "unknown type \"matrix\""));
    CHECK(HasProblem(result, 0, "variable \"n\": the value is not a int"));
    CHECK(HasProblem(result, 0, "\"targetKey\" needs a variable of type object"));
    CHECK(HasProblem(result, 0, "\"thinkInterval\" must be a number, 0 or more"));
    CHECK(HasProblem(result, 0, "unknown setting \"colour\""));
    CHECK(HasProblem(result, 1, "task 1: unknown block \"Ai.Fly\""));
    CHECK(HasProblem(result, 1, "task 2 \"Ai.Wait\": \"seconds\" must be 0 or more"));
    CHECK(HasProblem(result, 1, "task 2 \"Ai.Wait\": unknown parameter \"secs\""));
    CHECK(HasProblem(result, 1, "task 3 \"Ai.MoveTo\": \"target\" is not set"));
    CHECK(HasProblem(result, 1, "variable \"flag\" is bool, the block needs object or vec3"));
    CHECK(HasProblem(result, 1, "task 5: \"Ai.CanSee\" is a condition, not a task"));
    CHECK(HasProblem(result, 1, "\"value\" must be a bool like the key"));
    CHECK(HasProblem(result, 1, "\"minimum\" is greater than \"maximum\""));
    CHECK(HasProblem(result, 2, "two states are named \"A\""));
    CHECK(HasProblem(result, 2, "no variable \"nothing\""));
    CHECK(HasProblem(result, 2, "\"items\" must be a non-empty array"));
    CHECK(HasProblem(result, 2, "the block needs int or float"));
    CHECK(HasProblem(result, 3, "name the state"));
    CHECK(HasProblem(result, 4, "loop") || HasProblem(result, 5, "loop"));
    CHECK(HasProblem(result, 6, "parent 99 is not a node"));
    CHECK(HasProblem(result, 7, "unknown node type \"Fsm.Teleport\""));
    CHECK(HasProblem(result, 0, "a node without a positive integer id"));
    CHECK(HasProblem(result, 1, "unknown trigger \"sometimes\""));
    CHECK(HasProblem(result, 1, "name the event"));
    CHECK(HasProblem(result, 1, "\"delay\" must be a number"));
    CHECK(HasProblem(result, 8, "a transition can start only at a state"));
    CHECK(HasProblem(result, 0, "a link to a missing node 42"));

    // "Next" from the last child.
    Json last = Machine();
    last["nodes"] = {StateNode(1, "Only", 0), {{"id", 2}, {"type", "Fsm.Next"}}};
    last["connections"] = Json::array({LinkOf(1, 2)});
    CHECK(HasProblem(Load(last), 1, "\"Only\" is the last state of its parent"));

    // The file as a whole.
    CHECK(HasProblem(Ai::LoadMachine("{", "x"), 0, "invalid JSON"));
    Json code = Machine();
    code["schema"] = "code";
    CHECK(HasProblem(Load(code), 0, "\"schema\" must be \"ai.fsm\""));
    Json future = Machine();
    future["schemaVersion"] = 2;
    CHECK(HasProblem(Load(future), 0, "update the engine"));
    CHECK(HasProblem(Load(Machine()), 0, "no states"));
    Json typed = Machine();
    typed["nodes"] = Json::array({StateNode(1, "A", 0, {{"tasks", {{{"type", "Ai.Wait"}, {"seconds", "long"}}}}})});
    CHECK(HasProblem(Load(typed), 1, "\"seconds\" must be a number"));
}

// A plain AiCharacter whose "stateMachine" is a .fsm path; the cache follows changes of the file.
void SceneChecks()
{
    const auto folder = std::filesystem::temp_directory_path() / "ai-machine-file-tests";
    std::filesystem::create_directories(folder);
    const auto path = folder / "Guard.fsm";
    auto write = [&](const Json& file) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "\xef\xbb\xbf" << file.dump(2);                      // a BOM is accepted
    };
    Json file = Machine();
    file["variables"] = {Variable("player", "object"), Variable("seen", "vec3"), Variable("visits", "int")};
    file["settings"] = {{"targetKey", "player"}, {"lastSeenKey", "seen"}, {"thinkInterval", 0.1}};
    file["nodes"] = Json::array({StateNode(1, "Watch", 0, {{"tasks", {{{"type", "Test.Bump"}, {"key", "visits"}, {"ticks", 100}}}}})});
    write(file);
    const std::string name = path.u8string();
    CHECK(Ai::LoadMachineFile(name).asset && Ai::LoadMachineFile(name).asset == Ai::LoadMachineFile(name).asset);   // cached

    {
        Game game;
        AiCharacter& guard = AddToScene<AiCharacter>(game, "Guard");
        guard.stateMachine = name;
        guard.sightRadius = 10;
        // Values of this object over the file's: an unknown key and a literal of another type are skipped.
        guard.machineMemory = R"({"visits": 10, "nobody": 1, "seen": "here"})";
        game.InitializeGameplay();
        game.StepProjectSimulation(0.05f);
        Ai::Controller* ai = guard.GetAi();
        CHECK(ai && ai->ActiveState() == "Watch" && ai->ThinkInterval() == 0.1f && Count(*ai, "visits") == 11);
        CHECK(!ai->Memory().Has(Ai::Key<int>{"nobody"}) && !ai->Memory().Has(Ai::Key<std::string>{"seen"}));
    }

    // A broken file: the start stops with the problems in the message.
    file["nodes"][0]["properties"]["tasks"][0]["key"] = "visitz";
    write(file);
    std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + std::chrono::seconds(2));
    CHECK(HasProblem(Ai::LoadMachineFile(name), 1, "no variable \"visitz\""));
    {
        Game game;
        AiCharacter& guard = AddToScene<AiCharacter>(game, "Guard");
        guard.stateMachine = name;
        CHECK(Throws([&] { game.InitializeGameplay(); }, "node 1: task 1 \"Test.Bump\""));
    }
    CHECK(HasProblem(Ai::LoadMachineFile((folder / "Missing.fsm").u8string()), 0, "cannot read the file"));
    std::filesystem::remove_all(folder);
}

// The example of Engine/docs/AI-FSM.md loads as written.
void GuideExampleChecks()
{
    const auto guide = std::filesystem::path(__FILE__).parent_path() / ".." / ".." / ".." / "docs" / "AI-FSM.md";
    std::ifstream input(guide, std::ios::binary);
    CHECK(input.good());
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const size_t begin = text.find("```json\n") != std::string::npos ? text.find("```json\n") + 8 : text.find("```json\r\n") + 9;
    const size_t end = text.find("```", begin);
    CHECK(begin != std::string::npos && end != std::string::npos);
    const auto asset = Built(Json::parse(text.substr(begin, end - begin)));
    CHECK(asset->machine->Find("Chase") >= 0 && asset->targetKey == "Player" && asset->lastSeenKey == "LastSeen" && asset->memory.size() == 1);
}
} // namespace

void MachineFileTests()
{
    GuideExampleChecks();
    CatalogChecks();
    RunChecks();
    TargetsAndConditionsChecks();
    LinkedChecks();
    ProblemChecks();
    SceneChecks();
}
