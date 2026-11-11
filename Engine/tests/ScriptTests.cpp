// Visual scripts: catalog, compilation with per-node problems, the interpreter and the Game lifecycle.
#include <Engine/Components/GameComponents.h>
#include <Engine/InputDevice/Keys.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneDocument.h>
#include "SceneTestAccess.h"
#include <Engine/Script/ScriptRuntime.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

#define SCRIPT_CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string("script: ") + #condition); } while (false)

namespace
{
using Json = nlohmann::json;

// A .vscript built in code.
struct Graph
{
    Json nodes = Json::array(), links = Json::array(), variables = Json::array();
    int nextId = 1;

    int Node(const std::string& type, Json properties = Json::object())
    {
        nodes.push_back({{"id", nextId}, {"type", type}, {"title", ""}, {"x", 0}, {"y", 0}, {"properties", properties}});
        return nextId++;
    }
    void Link(int from, const std::string& fromPin, int to, const std::string& toPin)
    {
        links.push_back({{"fromNode", from}, {"fromPin", fromPin}, {"toNode", to}, {"toPin", toPin}});
    }
    void Variable(const std::string& name, const std::string& type, Json value)
    {
        variables.push_back({{"name", name}, {"type", type}, {"value", value}});
    }
    std::string Text() const
    {
        return Json{{"format", Script::FormatName}, {"version", 1}, {"schema", Script::SchemaId}, {"schemaVersion", 1},
                    {"variables", variables}, {"nodes", nodes}, {"connections", links}}.dump(2);
    }
};

bool HasProblem(const Script::CompileResult& result, int node, const std::string& text)
{
    return std::any_of(result.problems.begin(), result.problems.end(), [&](const Script::Problem& problem) {
        return problem.node == node && problem.message.find(text) != std::string::npos;
    });
}

std::shared_ptr<const Script::Program> Build(const Graph& graph)
{
    auto result = Script::Compile(graph.Text());
    if (!result.program)
    {
        std::string text = "graph does not compile:";
        for (const auto& problem : result.problems) text += " [" + std::to_string(problem.node) + "] " + problem.message;
        throw std::runtime_error(text);
    }
    return result.program;
}

// An object in a scene without GPU: ids and name lookup work like in Play.
struct Fixture
{
    Scene scene;
    GameComponent* Add(const std::string& name)
    {
        std::unique_ptr<GameComponent> component = std::make_unique<GameComponent>();
        GameComponent* raw = component.get();
        SCRIPT_CHECK(scene.AddComponent(name, component).result == SceneResult::Ok);
        return raw;
    }
};

void CatalogChecks()
{
    std::set<std::string> types;
    for (const auto& node : Script::Catalog())
    {
        SCRIPT_CHECK(types.insert(node.type).second);
        SCRIPT_CHECK(!node.nameRu.empty() && !node.nameEn.empty() && !node.group.empty());
        SCRIPT_CHECK(Script::FindNode(node.type) == &node || Script::FindNode(node.type)->type == node.type);
    }
    for (const char* required : {"Event.BeginPlay", "Event.Tick", "Event.KeyPressed", "Flow.Branch", "Flow.Sequence", "Action.Print",
                                 "Action.Translate", "Variable.Set", "Object.Find", "Math.Add", "Input.KeyDown"})
        SCRIPT_CHECK(types.count(required));
    SCRIPT_CHECK(Script::FindNode("Teleport") == nullptr);
    SCRIPT_CHECK(Script::KeyCode("W") == int(Keys::W) && Script::KeyCode("Space") == int(Keys::Space) && Script::KeyCode("Nope") == 0);
    SCRIPT_CHECK(Script::NodePins(*Script::FindNode("Flow.Sequence"), 3).size() == 4);

    const Json catalog = Json::parse(Script::CatalogJson());
    SCRIPT_CHECK(catalog["schema"] == Script::SchemaId && catalog["nodes"].size() == Script::Catalog().size());
    SCRIPT_CHECK(std::find(catalog["keys"].begin(), catalog["keys"].end(), "Space") != catalog["keys"].end());
    const auto tick = std::find_if(catalog["nodes"].begin(), catalog["nodes"].end(), [](const Json& n) { return n["type"] == "Event.Tick"; });
    SCRIPT_CHECK(tick != catalog["nodes"].end() && (*tick)["event"] == true && (*tick)["pins"][1]["type"] == "float");
}

void CompileProblemChecks()
{
    SCRIPT_CHECK(HasProblem(Script::Compile("{oops"), 0, "invalid JSON"));
    SCRIPT_CHECK(HasProblem(Script::Compile(R"({"version":1,"nodes":[]})"), 0, "not a visual script"));
    SCRIPT_CHECK(HasProblem(Script::Compile(R"({"format":"ITMOEngine.Graph","version":2,"schema":"code","schemaVersion":1,"nodes":[]})"), 0, "version 2"));
    SCRIPT_CHECK(HasProblem(Script::Compile(R"({"format":"ITMOEngine.Graph","version":1,"schema":"ai.bt","schemaVersion":1,"nodes":[]})"), 0, "not a code graph"));
    SCRIPT_CHECK(HasProblem(Script::Compile(R"({"format":"ITMOEngine.Graph","version":1,"schema":"code","schemaVersion":1,"nodes":[{"id":"x"}]})"), 0, ""));

    Graph graph;
    graph.Variable("Speed", "float", 2);
    graph.Variable("Speed", "float", 3);
    graph.Variable("Bad", "matrix", 0);
    const int unknown = graph.Node("Teleport");
    const int key = graph.Node("Event.KeyPressed", {{"key", "Hyper"}});
    const int variable = graph.Node("Variable.Get", {{"variable", "Missing"}});
    const int number = graph.Node("Const.Float", {{"value", 1}});
    const int text = graph.Node("Const.String", {{"value", "hi"}});
    const int add = graph.Node("Math.Add");
    const int print = graph.Node("Action.Print");
    const int other = graph.Node("Action.Print");
    const int begin = graph.Node("Event.BeginPlay");
    const int sequence = graph.Node("Flow.Sequence", {{"outputs", 9}});
    const int translate = graph.Node("Action.Translate", {{"inputs", {{"Offset", "far"}}}});
    const int a = graph.Node("Math.Add"), b = graph.Node("Math.Add");
    graph.Link(text, "Value", add, "A");          // string into float
    graph.Link(number, "Value", add, "B");
    graph.Link(number, "Value", add, "B");        // second link into one input
    graph.Link(begin, "then", print, "exec");
    graph.Link(begin, "then", other, "exec");     // exec output twice
    graph.Link(number, "Value", print, "exec");   // value into the flow
    graph.Link(unknown, "Out", print, "Text");    // link of an unknown node: reported on that node only
    graph.Link(a, "Out", b, "A");
    graph.Link(b, "Out", a, "A");                 // data cycle
    graph.Link(begin, "nope", print, "exec");
    const auto result = Script::Compile(graph.Text());
    SCRIPT_CHECK(!result.program);
    SCRIPT_CHECK(HasProblem(result, 0, "declared twice") && HasProblem(result, 0, "unknown type \"matrix\""));
    SCRIPT_CHECK(HasProblem(result, unknown, "unknown node type"));
    SCRIPT_CHECK(HasProblem(result, key, "unknown key"));
    SCRIPT_CHECK(HasProblem(result, variable, "no variable"));
    SCRIPT_CHECK(HasProblem(result, add, "takes float, got string"));
    SCRIPT_CHECK(HasProblem(result, add, "more than one link"));
    SCRIPT_CHECK(HasProblem(result, begin, "output then has more than one link"));
    SCRIPT_CHECK(HasProblem(result, print, "execution flow"));
    SCRIPT_CHECK(HasProblem(result, sequence, "outputs"));
    SCRIPT_CHECK(HasProblem(result, translate, "default of input Offset"));
    SCRIPT_CHECK(HasProblem(result, a, "data cycle") && HasProblem(result, b, "data cycle"));
    SCRIPT_CHECK(HasProblem(result, begin, "no output \"nope\""));
    SCRIPT_CHECK(!HasProblem(result, 0, "missing node"));

    Graph empty;
    SCRIPT_CHECK(Script::Compile(empty.Text()).program != nullptr); // a graph without events is valid and does nothing
}

void InterpreterChecks()
{
    // BeginPlay, Sequence order, constants and defaults of unconnected inputs.
    {
        Fixture fixture;
        GameComponent* cube = fixture.Add("Cube");
        Graph graph;
        const int begin = graph.Node("Event.BeginPlay");
        const int sequence = graph.Node("Flow.Sequence", {{"outputs", 3}});
        int prints[3];
        for (int i = 0; i < 3; ++i) prints[i] = graph.Node("Action.Print", {{"inputs", {{"Text", std::string(1, char('a' + i))}}}});
        const int move = graph.Node("Action.Translate", {{"inputs", {{"Offset", {1, 0, 0}}}}});
        graph.Link(begin, "then", sequence, "exec");
        graph.Link(sequence, "then0", prints[0], "exec");
        graph.Link(sequence, "then1", prints[1], "exec");
        graph.Link(sequence, "then2", prints[2], "exec");
        graph.Link(prints[2], "then", move, "exec");
        Script::Instance script(Build(graph), *cube, "/Game/Scripts/Order.vscript");
        script.Tick(1.0f);
        SCRIPT_CHECK(script.Printed().empty()); // nothing before BeginPlay
        script.BeginPlay();
        script.BeginPlay(); // once only
        SCRIPT_CHECK((script.Printed() == std::vector<std::string>{"a", "b", "c"}));
        SCRIPT_CHECK(cube->GetLocalPosition() == glm::vec3(1, 0, 0));
    }

    // Tick with DeltaTime, variables, comparison and Branch.
    {
        Fixture fixture;
        GameComponent* cube = fixture.Add("Cube");
        Graph graph;
        graph.Variable("Count", "float", 0);
        graph.Variable("Label", "string", "big");
        const int tick = graph.Node("Event.Tick");
        const int get = graph.Node("Variable.Get", {{"variable", "Count"}});
        const int add = graph.Node("Math.Add", {{"inputs", {{"B", 1}}}});
        const int set = graph.Node("Variable.Set", {{"variable", "Count"}});
        const int again = graph.Node("Variable.Get", {{"variable", "Count"}});
        const int greater = graph.Node("Logic.Greater", {{"inputs", {{"B", 2}}}});
        const int branch = graph.Node("Flow.Branch");
        const int label = graph.Node("Variable.Get", {{"variable", "Label"}});
        const int print = graph.Node("Action.Print");
        const int scale = graph.Node("Vec3.Scale");
        const int speed = graph.Node("Const.Vec3", {{"value", {0, 0, 2}}});
        const int move = graph.Node("Action.Translate");
        graph.Link(tick, "then", set, "exec");
        graph.Link(get, "Value", add, "A");
        graph.Link(add, "Out", set, "Value");
        graph.Link(set, "then", branch, "exec");
        graph.Link(again, "Value", greater, "A");
        graph.Link(greater, "Out", branch, "Condition");
        graph.Link(branch, "True", print, "exec");
        graph.Link(label, "Value", print, "Text");
        graph.Link(branch, "False", move, "exec");
        graph.Link(speed, "Value", scale, "Vector");
        graph.Link(tick, "DeltaTime", scale, "Factor");
        graph.Link(scale, "Out", move, "Offset");
        Script::Instance script(Build(graph), *cube, "/Game/Scripts/Count.vscript");
        script.BeginPlay();
        for (int i = 0; i < 4; ++i) script.Tick(0.25f);
        SCRIPT_CHECK(std::get<float>(script.GetVariable("Count")) == 4.0f);
        SCRIPT_CHECK(script.Printed().size() == 2 && script.Printed()[0] == "big"); // counts 3 and 4
        SCRIPT_CHECK(std::abs(cube->GetLocalPosition().z - 1.0f) < 1e-5f);     // two ticks of 0.25 s at 2 units/s
        SCRIPT_CHECK(std::holds_alternative<std::monostate>(script.GetVariable("Nope")));
    }

    // Other objects by name, rotation, scale, time.
    {
        Fixture fixture;
        GameComponent* cube = fixture.Add("Cube");
        GameComponent* lamp = fixture.Add("Lamp");
        Graph graph;
        const int begin = graph.Node("Event.BeginPlay");
        const int name = graph.Node("Const.String", {{"value", "Lamp"}});
        const int find = graph.Node("Object.Find");
        const int place = graph.Node("Action.SetPosition", {{"inputs", {{"Position", {5, 6, 7}}}}});
        const int rotate = graph.Node("Action.Rotate", {{"inputs", {{"Degrees", {0, 90, 0}}}}});
        const int scale = graph.Node("Action.SetScale", {{"inputs", {{"Scale", {2, 2, 2}}}}});
        graph.Link(begin, "then", place, "exec");
        graph.Link(name, "Value", find, "Name");
        graph.Link(find, "Object", place, "Target");
        graph.Link(place, "then", rotate, "exec");
        graph.Link(rotate, "then", scale, "exec");
        graph.Link(find, "Object", scale, "Target");
        Script::Instance script(Build(graph), *cube, "/Game/Scripts/Lamp.vscript");
        script.BeginPlay();
        SCRIPT_CHECK(!script.Failed());
        SCRIPT_CHECK(lamp->GetLocalPosition() == glm::vec3(5, 6, 7) && cube->GetLocalPosition() == glm::vec3(0));
        SCRIPT_CHECK(cube->GetRotation() == glm::vec3(0, 90, 0) && lamp->GetScale() == glm::vec3(2));
    }

    // A runtime error stops this object's script and names the node; later frames do nothing.
    {
        Fixture fixture;
        GameComponent* cube = fixture.Add("Cube");
        Graph graph;
        const int tick = graph.Node("Event.Tick");
        const int move = graph.Node("Action.Translate", {{"inputs", {{"Offset", {1, 0, 0}}}}});
        const int print = graph.Node("Action.Print");
        const int divide = graph.Node("Math.Divide", {{"inputs", {{"A", 1}}}});
        graph.Link(tick, "then", move, "exec");
        graph.Link(move, "then", print, "exec");
        graph.Link(divide, "Out", print, "Text");
        Script::Instance script(Build(graph), *cube, "/Game/Scripts/Divide.vscript");
        script.BeginPlay();
        script.Tick(0.1f);
        SCRIPT_CHECK(script.Failed() && script.Error().node == divide && script.Error().message.find("division by zero") != std::string::npos);
        script.Tick(0.1f);
        SCRIPT_CHECK(cube->GetLocalPosition() == glm::vec3(1, 0, 0));
    }

    // A missing object is an error, not a silent no-op.
    {
        Fixture fixture;
        GameComponent* cube = fixture.Add("Cube");
        Graph graph;
        const int begin = graph.Node("Event.BeginPlay");
        const int find = graph.Node("Object.Find", {{"inputs", {{"Name", "Nobody"}}}});
        const int place = graph.Node("Action.SetPosition");
        graph.Link(begin, "then", place, "exec");
        graph.Link(find, "Object", place, "Target");
        Script::Instance script(Build(graph), *cube, "/Game/Scripts/Missing.vscript");
        script.BeginPlay();
        SCRIPT_CHECK(script.Failed() && script.Error().node == place && script.Error().message.find("no object") != std::string::npos);
    }

    // An endless exec loop hits the step limit instead of freezing the game.
    {
        Fixture fixture;
        GameComponent* cube = fixture.Add("Cube");
        Graph graph;
        const int begin = graph.Node("Event.BeginPlay");
        const int first = graph.Node("Action.Rotate");
        const int second = graph.Node("Action.Rotate");
        graph.Link(begin, "then", first, "exec");
        graph.Link(first, "then", second, "exec");
        graph.Link(second, "then", first, "exec");
        Script::Instance script(Build(graph), *cube, "/Game/Scripts/Loop.vscript");
        script.BeginPlay();
        SCRIPT_CHECK(script.Failed() && script.Error().message.find("endless loop") != std::string::npos);
    }

    // Random stays in range; Equal has a tolerance.
    {
        Fixture fixture;
        GameComponent* cube = fixture.Add("Cube");
        Graph graph;
        graph.Variable("Roll", "float", -1);
        graph.Variable("Same", "bool", false);
        const int tick = graph.Node("Event.Tick");
        const int random = graph.Node("Math.Random", {{"inputs", {{"Min", 2}, {"Max", 3}}}});
        const int setRoll = graph.Node("Variable.Set", {{"variable", "Roll"}});
        const int equal = graph.Node("Logic.Equal", {{"inputs", {{"A", 0.3}, {"B", 0.3000001}}}});
        const int setSame = graph.Node("Variable.Set", {{"variable", "Same"}});
        graph.Link(tick, "then", setRoll, "exec");
        graph.Link(random, "Out", setRoll, "Value");
        graph.Link(setRoll, "then", setSame, "exec");
        graph.Link(equal, "Out", setSame, "Value");
        Script::Instance script(Build(graph), *cube, "/Game/Scripts/Random.vscript");
        script.BeginPlay();
        for (int i = 0; i < 20; ++i)
        {
            script.Tick(0.01f);
            const float roll = std::get<float>(script.GetVariable("Roll"));
            SCRIPT_CHECK(roll >= 2.0f && roll <= 3.0f);
        }
        SCRIPT_CHECK(std::get<bool>(script.GetVariable("Same")));
    }
}

void WriteText(const std::filesystem::path& file, const std::string& text)
{
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output << text;
    SCRIPT_CHECK(output.good());
}

void GameLifecycleChecks()
{
    const auto folder = std::filesystem::temp_directory_path() / ("EngineScriptTests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(folder);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove_all(path, e); } } cleanup{folder};

    // Moves on Tick, prints on Space press and release.
    Graph mover;
    const int tick = mover.Node("Event.Tick");
    const int scale = mover.Node("Vec3.Scale");
    const int speed = mover.Node("Const.Vec3", {{"value", {0, 0, 1}}});
    const int move = mover.Node("Action.Translate");
    const int pressed = mover.Node("Event.KeyPressed", {{"key", "Space"}});
    const int released = mover.Node("Event.KeyReleased", {{"key", "Space"}});
    const int down = mover.Node("Action.Print", {{"inputs", {{"Text", "down"}}}});
    const int up = mover.Node("Action.Print", {{"inputs", {{"Text", "up"}}}});
    mover.Link(tick, "then", move, "exec");
    mover.Link(speed, "Value", scale, "Vector");
    mover.Link(tick, "DeltaTime", scale, "Factor");
    mover.Link(scale, "Out", move, "Offset");
    mover.Link(pressed, "then", down, "exec");
    mover.Link(released, "then", up, "exec");
    const auto moverFile = folder / "Mover.vscript";
    WriteText(moverFile, "\xef\xbb\xbf" + mover.Text()); // a BOM is accepted

    Game game;
    std::unique_ptr<GameComponent> owned = std::make_unique<GameComponent>();
    GameComponent* cube = owned.get();
    cube->SetScriptAsset(moverFile.generic_u8string());
    SCRIPT_CHECK(cube->GetScriptAsset() == moverFile.generic_u8string());
    SCRIPT_CHECK(game.GetScene().AddComponent("Cube", owned).result == SceneResult::Ok);
    SCRIPT_CHECK(cube->GetScriptInstance() == nullptr); // Edit: nothing runs
    game.InitializeGameplay();
    const Script::Instance* script = cube->GetScriptInstance();
    SCRIPT_CHECK(script && !script->Failed());
    game.StepProjectSimulation(0.5f);
    SCRIPT_CHECK(std::abs(cube->GetLocalPosition().z - 0.5f) < 1e-5f);
    game.GetInput().SetKey(Keys::Space, true);
    game.StepProjectSimulation(0.1f);
    game.StepProjectSimulation(0.1f); // held: no second press
    game.GetInput().SetKey(Keys::Space, false);
    game.StepProjectSimulation(0.1f);
    SCRIPT_CHECK((script->Printed() == std::vector<std::string>{"down", "up"}));

    // A broken script is logged and skipped; the game keeps running.
    const auto brokenFile = folder / "Broken.vscript";
    WriteText(brokenFile, R"({"format":"ITMOEngine.Graph","version":1,"schema":"code","schemaVersion":1,"nodes":[{"id":1,"type":"Teleport"}]})");
    std::unique_ptr<GameComponent> brokenOwned = std::make_unique<GameComponent>();
    GameComponent* broken = brokenOwned.get();
    broken->SetScriptAsset(brokenFile.generic_u8string());
    SCRIPT_CHECK(game.GetScene().AddComponent("Broken", brokenOwned).result == SceneResult::Ok);
    std::unique_ptr<GameComponent> missingOwned = std::make_unique<GameComponent>();
    GameComponent* missing = missingOwned.get();
    missing->SetScriptAsset((folder / "Nope.vscript").generic_u8string());
    SCRIPT_CHECK(game.GetScene().AddComponent("Missing", missingOwned).result == SceneResult::Ok);
    game.StepProjectSimulation(0.1f);
    SCRIPT_CHECK(broken->GetScriptInstance() == nullptr && missing->GetScriptInstance() == nullptr);
    SCRIPT_CHECK(cube->GetLocalPosition().z > 0.79f);

    // A changed file is compiled again for objects that start later.
    Graph printer;
    const int begin = printer.Node("Event.BeginPlay");
    const int hello = printer.Node("Action.Print", {{"inputs", {{"Text", "v2"}}}});
    printer.Link(begin, "then", hello, "exec");
    WriteText(moverFile, printer.Text());
    std::filesystem::last_write_time(moverFile, std::filesystem::last_write_time(moverFile) + std::chrono::seconds(5));
    std::unique_ptr<GameComponent> laterOwned = std::make_unique<GameComponent>();
    GameComponent* later = laterOwned.get();
    later->SetScriptAsset(moverFile.generic_u8string());
    SCRIPT_CHECK(game.GetScene().AddComponent("Later", laterOwned).result == SceneResult::Ok);
    game.StepProjectSimulation(0.1f);
    SCRIPT_CHECK(later->GetScriptInstance() && (later->GetScriptInstance()->Printed() == std::vector<std::string>{"v2"}));
    SCRIPT_CHECK(cube->GetScriptInstance() == script); // running instances keep their program until the next Play
}
// 1000 objects whose script rotates them every frame: the interpreter cost of one frame.
void PerformanceChecks()
{
    Fixture fixture;
    Graph graph;
    const int tick = graph.Node("Event.Tick");
    const int degrees = graph.Node("Vec3.Scale");
    const int speed = graph.Node("Const.Vec3", {{"value", {0, 90, 0}}});
    const int rotate = graph.Node("Action.Rotate");
    graph.Link(tick, "then", rotate, "exec");
    graph.Link(speed, "Value", degrees, "Vector");
    graph.Link(tick, "DeltaTime", degrees, "Factor");
    graph.Link(degrees, "Out", rotate, "Degrees");
    const auto program = Build(graph);
    std::vector<std::unique_ptr<Script::Instance>> scripts;
    for (int i = 0; i < 1000; ++i)
    {
        scripts.push_back(std::make_unique<Script::Instance>(program, *fixture.Add("Cube" + std::to_string(i)), "/Game/Scripts/Spin.vscript"));
        scripts.back()->BeginPlay();
    }
    constexpr int Frames = 60;
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < Frames; ++frame)
        for (auto& script : scripts) script->Tick(1.0f / 60.0f);
    const double perFrame = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / Frames;
    std::printf("visual scripts: 1000 objects, one Tick each: %.3f ms per frame\n", perFrame);
    SCRIPT_CHECK(std::abs(fixture.scene.FindComponentByName("Cube0")->GetRotation().y - 90.0f) < 0.01f);
    SCRIPT_CHECK(perFrame < 50.0); // generous: Debug builds and shared CI machines
}
// "assets.script" in scene files: written only when assigned, restored, validated.
void SceneDocumentChecks()
{
    const ComponentType box{"Test.Box", "Box", [] { return std::make_unique<GameComponent>(); }};
    Scene scene("Scripts");
    std::unique_ptr<GameComponent> plain = std::make_unique<GameComponent>(), scripted = std::make_unique<GameComponent>();
    GameComponent* scriptedRaw = scripted.get();
    SCRIPT_CHECK(scene.AddComponent("Plain", plain, box.typeName).result == SceneResult::Ok);
    const auto added = scene.AddComponent("Scripted", scripted, box.typeName);
    SCRIPT_CHECK(added.result == SceneResult::Ok);
    const uint64_t revision = SceneTestAccess::Revision(scene);
    SCRIPT_CHECK(scene.SetScriptAsset(added.id, "/Game/Scripts/Spin.vscript") == SceneResult::Ok);
    SCRIPT_CHECK(SceneTestAccess::Revision(scene) > revision && scriptedRaw->GetScriptAsset() == "/Game/Scripts/Spin.vscript");
    SCRIPT_CHECK(scene.SetScriptAsset(999999, "") == SceneResult::NotFound);

    const std::string text = SceneDocument::Serialize(SceneTestAccess::Snapshot(scene));
    const Json json = Json::parse(text);
    for (const auto& object : json["objects"])
    {
        if (object["name"] == "Plain") SCRIPT_CHECK(!object.contains("assets")); // old builds keep reading such objects
        if (object["name"] == "Scripted")
            SCRIPT_CHECK(object["assets"]["script"] == "/Game/Scripts/Spin.vscript" && object["assets"]["model"] == "" && !object["assets"].contains("material"));
    }
    Scene restored("Restored");
    SceneDocument::Restore(SceneDocument::Parse(text), {&box, 1}, restored);
    SCRIPT_CHECK(restored.FindComponentByName("Scripted")->GetScriptAsset() == "/Game/Scripts/Spin.vscript");
    SCRIPT_CHECK(restored.FindComponentByName("Plain")->GetScriptAsset().empty());

    auto rejects = [&](const std::string& assets) {
        Json document = json;
        for (auto& object : document["objects"])
            if (object["name"] == "Scripted") object["assets"] = Json::parse(assets);
        try { SceneDocument::Parse(document.dump()); return false; }
        catch (const SceneDocumentError&) { return true; }
    };
    SCRIPT_CHECK(rejects(R"({"model":"","texture":"","script":5})"));
    SCRIPT_CHECK(rejects(R"({"model":"","texture":"","script":"/Game/../Outside.vscript"})"));
    SCRIPT_CHECK(rejects(R"({"model":"","texture":"","sound":"/Game/a.wav"})")); // the format stays strict
    SCRIPT_CHECK(rejects(R"({"texture":"","script":"/Game/Scripts/Spin.vscript"})"));
    SCRIPT_CHECK(!rejects(R"({"model":"","texture":"","material":"/Game/M.mat","script":"/Game/Scripts/Spin.vscript"})"));
}

// Engine.Scene.Script v1 on an edit session: catalog, assignment, checks, Play problems.
void ExtensionChecks()
{
    const ComponentType box{"Test.Box", "Box", [] { return std::make_unique<GameComponent>(); }};
    SceneEditSession session({&box, 1});
    const auto api = session.GetApi();
    const auto script = session.GetScriptApi();
    SCRIPT_CHECK(script.version == 1 && script.context && script.describe && script.get && script.set && script.check && script.readProblems);

    const auto root = std::filesystem::temp_directory_path() / ("EngineScriptSession-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto content = root / "Content";
    std::filesystem::create_directories(content / "Scripts");
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove_all(path, e); } } cleanup{root};
    char error[1024]{};
    SCRIPT_CHECK(api.setContentRoot(api.context, content.u8string().c_str(), error, sizeof(error)) == EngineSceneOk);

    auto text = [](void* user, const char* value) { *static_cast<std::string*>(user) = value; };
    std::string catalog;
    SCRIPT_CHECK(script.describe(script.context, text, &catalog) == EngineSceneOk && Json::parse(catalog)["schema"] == Script::SchemaId);

    Graph good;
    const int begin = good.Node("Event.BeginPlay");
    const int print = good.Node("Action.Print", {{"inputs", {{"Text", "started"}}}});
    good.Link(begin, "then", print, "exec");
    WriteText(content / "Scripts/Good.vscript", good.Text());
    Graph divide;
    const int start = divide.Node("Event.BeginPlay");
    const int out = divide.Node("Action.Print");
    const int zero = divide.Node("Math.Divide", {{"inputs", {{"A", 1}}}});
    divide.Link(start, "then", out, "exec");
    divide.Link(zero, "Out", out, "Text");
    WriteText(content / "Scripts/Divide.vscript", divide.Text());
    WriteText(content / "Scripts/Broken.vscript", R"({"format":"ITMOEngine.Graph","version":1,"schema":"code","schemaVersion":1,"nodes":[{"id":7,"type":"Teleport"}]})");

    EngineSceneTransform t{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
    uint64_t first = 0, second = 0, third = 0;
    SCRIPT_CHECK(api.scene.createObject(api.scene.context, box.typeName, "First", &t, &first) == EngineSceneOk);
    SCRIPT_CHECK(api.scene.createObject(api.scene.context, box.typeName, "Second", &t, &second) == EngineSceneOk);
    SCRIPT_CHECK(api.scene.createObject(api.scene.context, box.typeName, "Third", &t, &third) == EngineSceneOk);

    SCRIPT_CHECK(script.set(script.context, first, "/Game/Scripts/Good.png", error, sizeof(error)) == EngineSceneInvalidArgument && std::string(error).find(".vscript") != std::string::npos);
    SCRIPT_CHECK(script.set(script.context, first, "/Game/Scripts/Missing.vscript", error, sizeof(error)) == EngineSceneInvalidArgument);
    SCRIPT_CHECK(script.set(script.context, 999999, "", error, sizeof(error)) == EngineSceneNotFound);
    SCRIPT_CHECK(script.set(script.context, first, "/Game/Scripts/Good.vscript", error, sizeof(error)) == EngineSceneOk);
    SCRIPT_CHECK(script.set(script.context, second, "/Game/Scripts/Divide.vscript", error, sizeof(error)) == EngineSceneOk);
    SCRIPT_CHECK(script.set(script.context, third, "/Game/Scripts/Broken.vscript", error, sizeof(error)) == EngineSceneOk);
    std::string assigned;
    SCRIPT_CHECK(script.get(script.context, first, text, &assigned) == EngineSceneOk && assigned == "/Game/Scripts/Good.vscript");

    struct Problems { std::vector<EngineScriptProblem> items; std::vector<std::string> names, messages; };
    auto collect = [](void* user, const EngineScriptProblem* problem) {
        auto& problems = *static_cast<Problems*>(user);
        problems.items.push_back(*problem);
        problems.names.push_back(problem->objectName);
        problems.messages.push_back(problem->message);
    };
    Problems checked;
    SCRIPT_CHECK(script.check(script.context, "/Game/Scripts/Broken.vscript", collect, &checked) == EngineSceneOk);
    SCRIPT_CHECK(checked.items.size() == 1 && checked.items[0].node == 7 && checked.items[0].objectId == 0 && checked.messages[0].find("unknown node type") != std::string::npos);
    Problems clean;
    SCRIPT_CHECK(script.check(script.context, "/Game/Scripts/Good.vscript", collect, &clean) == EngineSceneOk && clean.items.empty());
    Problems missing;
    SCRIPT_CHECK(script.check(script.context, "/Game/Scripts/Nope.vscript", collect, &missing) == EngineSceneOk && missing.items.size() == 1);

    const auto saved = content / "Scenes/Main.scene.json";
    std::filesystem::create_directories(saved.parent_path());
    SCRIPT_CHECK(api.saveScene(api.context, saved.u8string().c_str(), error, sizeof(error)) == EngineSceneOk);
    std::ifstream file(saved);
    const Json document = Json::parse(file);
    SCRIPT_CHECK(std::count_if(document["objects"].begin(), document["objects"].end(), [](const Json& o) { return o.contains("assets") && o["assets"].contains("script"); }) == 3);

    Problems none;
    SCRIPT_CHECK(script.readProblems(script.context, collect, &none) == EngineSceneOk && none.items.empty()); // Edit: nothing runs
    SCRIPT_CHECK(api.control(api.context, EnginePlay) == EngineSceneOk);
    SCRIPT_CHECK(script.set(script.context, first, "", error, sizeof(error)) == EngineSceneReadOnly);
    Problems play;
    SCRIPT_CHECK(script.readProblems(script.context, collect, &play) == EngineSceneOk && play.items.size() == 2);
    for (size_t i = 0; i < play.items.size(); ++i)
    {
        if (play.names[i] == "Second")
            SCRIPT_CHECK(play.items[i].runtime == 1 && play.items[i].objectId == second && play.items[i].node == zero && play.messages[i].find("division by zero") != std::string::npos);
        else
            SCRIPT_CHECK(play.names[i] == "Third" && play.items[i].runtime == 0 && play.items[i].objectId == third && play.items[i].node == 7);
    }
    SCRIPT_CHECK(api.control(api.context, EngineStop) == EngineSceneOk);
    SCRIPT_CHECK(script.set(script.context, first, "", error, sizeof(error)) == EngineSceneOk);
    SCRIPT_CHECK(script.get(script.context, first, text, &assigned) == EngineSceneOk && assigned.empty());
}
// The shipped example scripts compile with the current catalog.
void ExampleScriptChecks()
{
    std::filesystem::path folder;
    for (auto dir = std::filesystem::current_path(); !dir.empty(); dir = dir.parent_path())
    {
        if (std::filesystem::is_directory(dir / "Game/BasicExamples/Content/Scripts")) { folder = dir / "Game/BasicExamples/Content/Scripts"; break; }
        if (dir == dir.parent_path()) break;
    }
    SCRIPT_CHECK(!folder.empty()); // run from the repository
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".vscript") continue;
        std::ifstream input(entry.path(), std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const auto result = Script::Compile(text);
        if (!result.program)
            throw std::runtime_error("example " + entry.path().filename().u8string() + ": " + result.problems.front().message);
        ++count;
    }
    SCRIPT_CHECK(count >= 3);
}
} // namespace

void ScriptTests()
{
    ExampleScriptChecks();
    CatalogChecks();
    CompileProblemChecks();
    InterpreterChecks();
    GameLifecycleChecks();
    PerformanceChecks();
    SceneDocumentChecks();
    ExtensionChecks();
}
