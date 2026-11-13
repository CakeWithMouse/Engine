// Engine.Scene.AiMachine v1 on an edit session: catalog, check of .fsm files, the Play debugger.
#include <Engine/AI/Ai.h>
#include <Engine/Components/ComponentType.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#define AI_CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string("ai extension: ") + #condition); } while (false)

namespace
{
using Json = nlohmann::json;

// An AI character whose machine and memory are set like the editor's card sets them.
class Guard final : public AiCharacter
{
public:
    Guard()
    {
        stateMachine = "/Game/AI/Guard.fsm";
        machineMemory = R"({"Lives": 5})";
    }
};

void Write(const std::filesystem::path& file, const Json& json)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << json.dump(2);
}

Json Machine(Json nodes, Json variables = Json::array())
{
    return {{"format", "ITMOEngine.Graph"}, {"version", 1}, {"schema", "ai.fsm"}, {"schemaVersion", 1},
            {"variables", std::move(variables)}, {"nodes", std::move(nodes)}, {"connections", Json::array()}};
}
} // namespace

void AiMachineExtensionTests()
{
    const ComponentType types[] = {
        {"Test.Guard", "Guard", [] { return std::unique_ptr<GameComponent>(std::make_unique<Guard>()); }},
        {"Test.Idle", "Idle", [] { return std::unique_ptr<GameComponent>(std::make_unique<AiCharacter>()); }},
    };
    SceneEditSession session({types, 2});
    const auto api = session.GetApi();
    const auto ai = session.GetAiMachineApi();
    AI_CHECK(ai.version == 1 && ai.context && ai.describe && ai.check && ai.listUnits && ai.readStates && ai.readMemory && ai.readHistory);

    const auto root = std::filesystem::temp_directory_path() / ("EngineAiSession-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto content = root / "Content";
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove_all(path, e); } } cleanup{root};
    std::filesystem::create_directories(content);
    char error[1024]{};
    AI_CHECK(api.setContentRoot(api.context, content.u8string().c_str(), error, sizeof(error)) == EngineSceneOk);

    Write(content / "AI/Guard.fsm", Machine(
        {{{"id", 4}, {"type", "Fsm.State"}, {"x", 0}, {"y", 0}, {"properties", {{"name", "Patrol"}}}},
         {{"id", 7}, {"type", "Fsm.State"}, {"x", 0}, {"y", 100}, {"properties", {{"name", "Walk"}, {"parent", 4}, {"tasks", {{{"type", "Ai.Wait"}, {"seconds", 100}}}}}}}},
        {{{"name", "Lives"}, {"type", "int"}, {"value", 3}}, {{"name", "Spot"}, {"type", "vec3"}, {"value", {1, 2, 3}}},
         {{"name", "Label"}, {"type", "string"}, {"value", "hi"}}, {{"name", "Target"}, {"type", "object"}}}));
    Write(content / "AI/Broken.fsm", Machine({{{"id", 2}, {"type", "Fsm.State"}, {"properties", {{"name", "A"}, {"tasks", {{{"type", "Ai.Fly"}}}}}}}}));

    auto text = [](void* user, const char* value) { *static_cast<std::string*>(user) = value; };
    std::string catalog;
    AI_CHECK(ai.describe(ai.context, text, &catalog) == EngineSceneOk && Json::parse(catalog)["schema"] == "ai.fsm");

    // Check: the loader Play uses, every problem with its node.
    struct Problems { std::vector<int> nodes; std::vector<std::string> messages; };
    auto problem = [](void* user, const EngineAiProblem* item) {
        static_cast<Problems*>(user)->nodes.push_back(item->node);
        static_cast<Problems*>(user)->messages.push_back(item->message);
    };
    Problems broken, clean, missing, wrong;
    AI_CHECK(ai.check(ai.context, "/Game/AI/Broken.fsm", problem, &broken) == EngineSceneOk && broken.nodes.size() == 1 && broken.nodes[0] == 2 &&
             broken.messages[0].find("unknown block \"Ai.Fly\"") != std::string::npos);
    AI_CHECK(ai.check(ai.context, "/Game/AI/Guard.fsm", problem, &clean) == EngineSceneOk && clean.nodes.empty());
    AI_CHECK(ai.check(ai.context, "/Game/AI/Missing.fsm", problem, &missing) == EngineSceneOk && missing.messages.size() == 1 &&
             missing.messages[0] == "cannot read the file");
    AI_CHECK(ai.check(ai.context, "/Game/AI/Guard.vscript", problem, &wrong) == EngineSceneInvalidArgument);
    AI_CHECK(ai.check(ai.context, "/Game/../Outside.fsm", problem, &wrong) == EngineSceneInvalidArgument);

    EngineSceneTransform t{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
    uint64_t guard = 0, idle = 0;
    AI_CHECK(api.scene.createObject(api.scene.context, "Test.Guard", "Guard", &t, &guard) == EngineSceneOk);
    AI_CHECK(api.scene.createObject(api.scene.context, "Test.Idle", "Idle", &t, &idle) == EngineSceneOk);

    struct Units { std::vector<EngineAiUnit> items; std::vector<std::string> names, machines, active; };
    auto unit = [](void* user, const EngineAiUnit* item) {
        auto& units = *static_cast<Units*>(user);
        units.items.push_back(*item);
        units.names.push_back(item->objectName);
        units.machines.push_back(item->machine);
        units.active.push_back(item->activePath);
    };
    Units editUnits;
    AI_CHECK(ai.listUnits(ai.context, "", unit, &editUnits) == EngineSceneOk && editUnits.items.empty()); // nothing runs in Edit

    AI_CHECK(api.control(api.context, EnginePlay) == EngineSceneOk);
    for (int i = 0; i < 3; ++i) AI_CHECK(api.frame(api.context, 0.05) == EngineSceneOk);
    Units all, guards;
    AI_CHECK(ai.listUnits(ai.context, "", unit, &all) == EngineSceneOk && all.items.size() == 2);
    AI_CHECK(ai.listUnits(ai.context, "/Game/AI/Guard.fsm", unit, &guards) == EngineSceneOk && guards.items.size() == 1);
    AI_CHECK(guards.items[0].objectId == guard && guards.names[0] == "Guard" && guards.machines[0] == "/Game/AI/Guard.fsm" &&
             guards.active[0] == "Patrol/Walk" && guards.items[0].running == 1);
    for (size_t i = 0; i < all.items.size(); ++i)
        if (all.names[i] == "Idle") AI_CHECK(all.items[i].objectId == idle && all.items[i].running == 0 && all.active[i].empty());

    // The active chain with node ids of the file, the memory with readable values, the history.
    struct States { std::vector<int> nodes; std::vector<std::string> names, paths; };
    auto state = [](void* user, const EngineAiState* item) {
        static_cast<States*>(user)->nodes.push_back(item->node);
        static_cast<States*>(user)->names.push_back(item->name);
        static_cast<States*>(user)->paths.push_back(item->path);
    };
    States states;
    AI_CHECK(ai.readStates(ai.context, guard, state, &states) == EngineSceneOk);
    AI_CHECK((states.nodes == std::vector<int>{4, 7}) && (states.names == std::vector<std::string>{"Patrol", "Walk"}) &&
             (states.paths == std::vector<std::string>{"Patrol", "Patrol/Walk"}));
    std::vector<std::string> memory;
    auto value = [](void* user, const EngineAiMemoryValue* item) {
        static_cast<std::vector<std::string>*>(user)->push_back(std::string(item->key) + ":" + item->type + "=" + item->text);
    };
    AI_CHECK(ai.readMemory(ai.context, guard, value, &memory) == EngineSceneOk);
    AI_CHECK((memory == std::vector<std::string>{"Label:string=hi", "Lives:int=5", "Spot:vec3=(1, 2, 3)"}));   // the object's Lives over the file's
    std::vector<std::string> history;
    auto line = [](void* user, const char* item) { static_cast<std::vector<std::string>*>(user)->push_back(item); };
    AI_CHECK(ai.readHistory(ai.context, guard, line, &history) == EngineSceneOk && !history.empty() &&
             history.back().find("Walk") != std::string::npos);
    States none;
    AI_CHECK(ai.readStates(ai.context, 999999, state, &none) == EngineSceneNotFound);
    AI_CHECK(ai.readStates(ai.context, idle, state, &none) == EngineSceneOk && none.nodes.empty());   // no machine: an empty chain

    AI_CHECK(api.control(api.context, EngineStop) == EngineSceneOk);
    Units stopped;
    AI_CHECK(ai.listUnits(ai.context, "", unit, &stopped) == EngineSceneOk && stopped.items.empty());
    AI_CHECK(ai.readMemory(ai.context, guard, value, &memory) == EngineSceneNotFound);
}
