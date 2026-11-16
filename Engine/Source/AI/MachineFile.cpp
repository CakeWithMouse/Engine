// Loader of .fsm state machines: JSON "ITMOEngine.Graph", schema "ai.fsm" -> MachineBuilder.
// Every problem is collected with its node id, so the editor can show them all at once.
#include <Engine/AI/Controller.h>
#include <Engine/AI/MachineFile.h>
#include "../Resources/VfsRead.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace Ai
{
namespace
{
using Json = nlohmann::json;
using ServicePtr = std::shared_ptr<const ServiceBase>;
constexpr int MaxConditionDepth = 8;

bool Finite(double value) { return std::isfinite(value) && std::abs(value) <= 3.4e38; }

bool ParseVariableType(const std::string& text, VariableType& type)
{
    static const std::pair<const char*, VariableType> types[] = {
        {"bool", VariableType::Bool}, {"int", VariableType::Int}, {"float", VariableType::Float},
        {"vec3", VariableType::Vec3}, {"string", VariableType::String}, {"object", VariableType::Object}};
    for (const auto& [name, value] : types)
        if (text == name) { type = value; return true; }
    return false;
}

bool ParseLiteral(const Json& json, VariableType type, Blackboard::Value& value)
{
    switch (type)
    {
    case VariableType::Bool:
        if (!json.is_boolean()) return false;
        value = json.get<bool>();
        return true;
    case VariableType::Int:
        if (!json.is_number_integer()) return false;
        value = json.get<int>();
        return true;
    case VariableType::Float:
        if (!json.is_number() || !Finite(json.get<double>())) return false;
        value = json.get<float>();
        return true;
    case VariableType::Vec3:
        if (!json.is_array() || json.size() != 3) return false;
        for (const auto& item : json)
            if (!item.is_number() || !Finite(item.get<double>())) return false;
        value = glm::vec3(json[0].get<float>(), json[1].get<float>(), json[2].get<float>());
        return true;
    case VariableType::String:
        if (!json.is_string()) return false;
        value = json.get<std::string>();
        return true;
    default:
        return false; // an object has no literal: it comes from perception or Ai.FindObject
    }
}

Blackboard::Value DefaultOf(VariableType type)
{
    switch (type)
    {
    case VariableType::Bool: return false;
    case VariableType::Int: return 0;
    case VariableType::Float: return 0.0f;
    case VariableType::Vec3: return glm::vec3(0);
    case VariableType::String: return std::string();
    default: return ObjectRef{};
    }
}

bool KeyAccepts(ParamType param, VariableType type)
{
    switch (param)
    {
    case ParamType::Key: return true;
    case ParamType::KeyBool: return type == VariableType::Bool;
    case ParamType::KeyInt: return type == VariableType::Int;
    case ParamType::KeyFloat: return type == VariableType::Float;
    case ParamType::KeyVec3: return type == VariableType::Vec3;
    case ParamType::KeyString: return type == VariableType::String;
    case ParamType::KeyObject: return type == VariableType::Object;
    case ParamType::KeyNumber: return type == VariableType::Int || type == VariableType::Float;
    case ParamType::KeyPlace: return type == VariableType::Object || type == VariableType::Vec3;
    default: return false;
    }
}

const char* KeyWants(ParamType param)
{
    switch (param)
    {
    case ParamType::KeyNumber: return "int or float";
    case ParamType::KeyPlace: return "object or vec3";
    default: return ToString(param) + 4; // "key:bool" -> "bool"
    }
}

std::string Quote(const std::string& text) { return "\"" + text + "\""; }

std::string Number(double value)
{
    std::string text = std::to_string(value);
    text.erase(text.find_last_not_of('0') + 1);
    if (!text.empty() && text.back() == '.') text.pop_back();
    return text;
}

void SetMemory(Blackboard& memory, const std::string& key, const Blackboard::Value& value)
{
    std::visit([&](const auto& typed) {
        using T = std::decay_t<decltype(typed)>;
        memory.Set(Key<T>{key.c_str()}, typed);
    }, value);
}

enum class NodeKind { State, Finish, Next };

struct Transition
{
    int link = 0;          // index in "connections", for a stable order
    int priority = 0;
    int target = 0;        // node id
    Detail::TransitionDef::Trigger trigger = Detail::TransitionDef::Trigger::Completed;
    std::string event;
    std::vector<ConditionFn> conditions;
    float delay = 0;
};

struct State
{
    int id = 0;
    std::string name;
    int parent = 0;
    double x = 0, y = 0;
    std::vector<std::shared_ptr<const TaskBase>> tasks;
    std::vector<ConditionFn> enter;
    std::vector<std::pair<ServicePtr, float>> services;
    MachinePtr linked;
    std::vector<int> children; // ids, in order
    std::vector<Transition> transitions;
};

class Loader
{
public:
    Loader(const MachineFileReader& reader, std::vector<std::string> chain) : reader_(reader), chain_(std::move(chain)) {}
    MachineLoadResult Load(const std::string& text, const std::string& name);

private:
    void Problem(int node, std::string message) { problems_.push_back({node, std::move(message)}); }
    bool ReadParams(const Json& entry, const BlockSpec& block, int node, const std::string& where, Params& params);
    std::shared_ptr<const TaskBase> ReadTask(const Json& entry, int node, const std::string& where);
    ConditionFn ReadCondition(const Json& entry, int node, const std::string& where, int depth = 0);
    ServicePtr ReadService(const Json& entry, int node, const std::string& where, float& interval);
    std::vector<ConditionFn> ReadConditions(const Json& list, int node, const std::string& where);
    void ReadServices(const Json& list, int node, const std::string& where, std::vector<std::pair<ServicePtr, float>>& services);
    MachinePtr ReadLinked(const std::string& linked, int node);
    const BlockSpec* FindOfKind(const Json& entry, BlockKind kind, int node, const std::string& where);

    const MachineFileReader& reader_;
    std::vector<std::string> chain_; // .fsm files being loaded, outermost first
    std::vector<MachineProblem> problems_;
    std::map<std::string, VariableType, std::less<>> variables_;
};

const BlockSpec* Loader::FindOfKind(const Json& entry, BlockKind kind, int node, const std::string& where)
{
    static const char* kinds[] = {"task", "condition", "service"};
    if (!entry.is_object() || !entry.contains("type") || !entry["type"].is_string())
    {
        Problem(node, where + ": a block must be an object with a \"type\"");
        return nullptr;
    }
    const std::string type = entry["type"].get<std::string>();
    const BlockSpec* block = FindBlock(type);
    if (!block)
    {
        Problem(node, where + ": unknown block " + Quote(type));
        return nullptr;
    }
    if (block->kind != kind)
    {
        Problem(node, where + ": " + Quote(type) + " is a " + kinds[int(block->kind)] + ", not a " + kinds[int(kind)]);
        return nullptr;
    }
    return block;
}

bool Loader::ReadParams(const Json& entry, const BlockSpec& block, int node, const std::string& where, Params& params)
{
    const size_t before = problems_.size();
    const std::string at = where + " " + Quote(block.type);
    for (auto it = entry.begin(); it != entry.end(); ++it)
    {
        const std::string& key = it.key();
        if (key == "type" || (key == "not" && block.kind == BlockKind::Condition) || (key == "interval" && block.kind == BlockKind::Service)) continue;
        const bool known = std::any_of(block.params.begin(), block.params.end(), [&](const ParamSpec& p) { return p.name == key; });
        if (!known) Problem(node, at + ": unknown parameter " + Quote(key));
    }
    const ParamSpec* valueParam = nullptr;
    for (const ParamSpec& param : block.params)
    {
        if (param.type == ParamType::Value) { valueParam = &param; continue; }
        const std::string what = at + ": " + Quote(param.name);
        if (!entry.contains(param.name))
        {
            if (param.required) Problem(node, what + " is not set");
            else params.Set(param.name, param.fallback);
            continue;
        }
        const Json& json = entry[param.name];
        auto range = [&](double value) {
            if (value < param.minimum || value > param.maximum)
            {
                if (std::isinf(param.maximum)) Problem(node, what + " must be " + Number(param.minimum) + " or more");
                else if (std::isinf(param.minimum)) Problem(node, what + " must be " + Number(param.maximum) + " or less");
                else Problem(node, what + " must be from " + Number(param.minimum) + " to " + Number(param.maximum));
                return false;
            }
            return true;
        };
        switch (param.type)
        {
        case ParamType::Bool:
            if (!json.is_boolean()) Problem(node, what + " must be true or false");
            else params.Set(param.name, json.get<bool>());
            break;
        case ParamType::Int:
            if (!json.is_number_integer()) Problem(node, what + " must be a whole number");
            else if (range(json.get<double>())) params.Set(param.name, json.get<int>());
            break;
        case ParamType::Float:
            if (!json.is_number() || !Finite(json.get<double>())) Problem(node, what + " must be a number");
            else if (range(json.get<double>())) params.Set(param.name, json.get<float>());
            break;
        case ParamType::Vec3:
        {
            Blackboard::Value value;
            if (!ParseLiteral(json, VariableType::Vec3, value)) Problem(node, what + " must be [x, y, z]");
            else params.Set(param.name, value);
            break;
        }
        case ParamType::String:
        case ParamType::Event:
            if (!json.is_string()) Problem(node, what + " must be text");
            else if ((param.required || param.type == ParamType::Event) && json.get<std::string>().empty()) Problem(node, what + " is not set");
            else params.Set(param.name, json.get<std::string>());
            break;
        default: // keys
        {
            const std::string name = json.is_string() ? json.get<std::string>() : std::string();
            if (name.empty() && !param.required) { params.Set(param.name, std::string()); break; }
            if (name.empty()) { Problem(node, what + " is not set: name a memory key"); break; }
            const auto variable = variables_.find(name);
            if (variable == variables_.end()) { Problem(node, what + ": no variable " + Quote(name) + "; declare it in \"variables\""); break; }
            if (!KeyAccepts(param.type, variable->second))
            {
                Problem(node, what + ": variable " + Quote(name) + " is " + ToString(variable->second) + ", the block needs " + KeyWants(param.type));
                break;
            }
            params.Set(param.name, name);
            params.SetKeyType(param.name, variable->second);
            break;
        }
        }
    }
    if (valueParam && problems_.size() == before)
    {
        // The value has the type of the "key" variable.
        const VariableType type = params.KeyType("key");
        Blackboard::Value value = DefaultOf(type);
        if (entry.contains(valueParam->name) && type != VariableType::Object && !ParseLiteral(entry[valueParam->name], type, value))
            Problem(node, at + ": " + Quote(valueParam->name) + " must be a " + ToString(type) + " like the key");
        params.Set(valueParam->name, value);
    }
    return problems_.size() == before;
}

std::shared_ptr<const TaskBase> Loader::ReadTask(const Json& entry, int node, const std::string& where)
{
    const BlockSpec* block = FindOfKind(entry, BlockKind::Task, node, where);
    Params params;
    if (!block || !ReadParams(entry, *block, node, where, params)) return nullptr;
    try
    {
        auto task = block->task(params);
        if (!task) Problem(node, where + " " + Quote(block->type) + ": the block made no task");
        return task;
    }
    catch (const std::exception& error)
    {
        Problem(node, where + " " + Quote(block->type) + ": " + error.what());
        return nullptr;
    }
}

ConditionFn Loader::ReadCondition(const Json& entry, int node, const std::string& where, int depth)
{
    bool negate = false;
    if (entry.is_object() && entry.contains("not"))
    {
        if (!entry["not"].is_boolean()) { Problem(node, where + ": \"not\" must be true or false"); return {}; }
        negate = entry["not"].get<bool>();
    }
    ConditionFn condition;
    const std::string type = entry.is_object() ? entry.value("type", std::string()) : std::string();
    if (type == "Ai.All" || type == "Ai.Any")
    {
        if (depth >= MaxConditionDepth) { Problem(node, where + ": groups are nested deeper than " + std::to_string(MaxConditionDepth)); return {}; }
        for (auto it = entry.begin(); it != entry.end(); ++it)
            if (it.key() != "type" && it.key() != "not" && it.key() != "items") Problem(node, where + " " + Quote(type) + ": unknown parameter " + Quote(it.key()));
        if (!entry.contains("items") || !entry["items"].is_array() || entry["items"].empty())
        {
            Problem(node, where + " " + Quote(type) + ": \"items\" must be a non-empty array of conditions");
            return {};
        }
        std::vector<ConditionFn> items;
        int index = 0;
        bool ok = true;
        for (const auto& item : entry["items"])
        {
            ConditionFn read = ReadCondition(item, node, where + " " + Quote(type) + " item " + std::to_string(++index), depth + 1);
            ok = ok && bool(read);
            items.push_back(std::move(read));
        }
        if (!ok) return {};
        condition = type == "Ai.All" ? Detail::AllOf(std::move(items)) : Detail::AnyOf(std::move(items));
    }
    else
    {
        const BlockSpec* block = FindOfKind(entry, BlockKind::Condition, node, where);
        Params params;
        if (!block || !ReadParams(entry, *block, node, where, params)) return {};
        try
        {
            condition = block->condition(params);
            if (!condition) { Problem(node, where + " " + Quote(block->type) + ": the block made no condition"); return {}; }
        }
        catch (const std::exception& error)
        {
            Problem(node, where + " " + Quote(block->type) + ": " + error.what());
            return {};
        }
    }
    return negate ? Not(std::move(condition)) : condition;
}

ServicePtr Loader::ReadService(const Json& entry, int node, const std::string& where, float& interval)
{
    const BlockSpec* block = FindOfKind(entry, BlockKind::Service, node, where);
    interval = 0;
    if (block && entry.contains("interval"))
    {
        const Json& value = entry["interval"];
        if (!value.is_number() || !Finite(value.get<double>()) || value.get<double>() < 0)
        {
            Problem(node, where + " " + Quote(block->type) + ": \"interval\" must be a number, 0 or more");
            return nullptr;
        }
        interval = value.get<float>();
    }
    Params params;
    if (!block || !ReadParams(entry, *block, node, where, params)) return nullptr;
    try
    {
        auto service = block->service(params);
        if (!service) Problem(node, where + " " + Quote(block->type) + ": the block made no service");
        return service;
    }
    catch (const std::exception& error)
    {
        Problem(node, where + " " + Quote(block->type) + ": " + error.what());
        return nullptr;
    }
}

std::vector<ConditionFn> Loader::ReadConditions(const Json& list, int node, const std::string& where)
{
    std::vector<ConditionFn> conditions;
    if (list.is_null()) return conditions;
    if (!list.is_array()) { Problem(node, where + " must be an array of conditions"); return conditions; }
    int index = 0;
    for (const auto& entry : list)
        if (ConditionFn condition = ReadCondition(entry, node, where + " " + std::to_string(++index))) conditions.push_back(std::move(condition));
    return conditions;
}

void Loader::ReadServices(const Json& list, int node, const std::string& where, std::vector<std::pair<ServicePtr, float>>& services)
{
    if (list.is_null()) return;
    if (!list.is_array()) { Problem(node, where + " must be an array of services"); return; }
    int index = 0;
    for (const auto& entry : list)
    {
        float interval = 0;
        if (ServicePtr service = ReadService(entry, node, where + " " + std::to_string(++index), interval)) services.emplace_back(std::move(service), interval);
    }
}

MachinePtr Loader::ReadLinked(const std::string& linked, int node)
{
    if (!IsMachineFile(linked))
    {
        try
        {
            if (MachinePtr machine = FindMachine(linked)) return machine;
            Problem(node, "no state machine named " + Quote(linked) + " is registered in C++");
        }
        catch (const std::exception& error)
        {
            Problem(node, "linked " + Quote(linked) + ": " + error.what());
        }
        return nullptr;
    }
    if (!reader_) { Problem(node, "linked " + Quote(linked) + ": linked files cannot be read here"); return nullptr; }
    if (std::find(chain_.begin(), chain_.end(), linked) != chain_.end())
    {
        std::string loop;
        for (const auto& path : chain_) loop += path + " -> ";
        Problem(node, "linked machines form a loop: " + loop + linked);
        return nullptr;
    }
    if (static_cast<int>(chain_.size()) >= File::MaxLinkDepth)
    {
        Problem(node, "linked machines are nested deeper than " + std::to_string(File::MaxLinkDepth));
        return nullptr;
    }
    std::string text;
    if (!reader_(linked, text)) { Problem(node, "linked " + Quote(linked) + ": cannot read the file"); return nullptr; }
    std::vector<std::string> chain = chain_;
    chain.push_back(linked);
    std::string stem = std::filesystem::u8path(linked).stem().u8string();
    MachineLoadResult result = Loader(reader_, std::move(chain)).Load(text, stem);
    if (!result.asset)
    {
        const MachineProblem& first = result.problems.front();
        std::string message = "linked " + Quote(linked) + " has problems: " + (first.node ? "node " + std::to_string(first.node) + ": " : std::string()) + first.message;
        if (result.problems.size() > 1) message += " (and " + std::to_string(result.problems.size() - 1) + " more)";
        Problem(node, message);
        return nullptr;
    }
    return result.asset->machine;
}

MachineLoadResult Loader::Load(const std::string& text, const std::string& name)
{
    MachineLoadResult result;
    Json root;
    try
    {
        root = Json::parse(text);
    }
    catch (const Json::exception& error)
    {
        Problem(0, std::string("invalid JSON: ") + error.what());
        result.problems = std::move(problems_);
        return result;
    }
    auto finish = [&]() -> MachineLoadResult& { result.problems = std::move(problems_); if (!result.problems.empty()) result.asset.reset(); return result; };
    if (!root.is_object()) { Problem(0, "the root must be a JSON object"); return finish(); }
    if (root.value("format", std::string()) != File::FormatName)
    {
        Problem(0, std::string("not a graph file: \"format\" must be \"") + File::FormatName + "\"");
        return finish();
    }
    const int version = root.contains("version") && root["version"].is_number_integer() ? root["version"].get<int>() : 0;
    if (version < 1 || version > File::FormatVersion) Problem(0, "unsupported graph format version " + std::to_string(version) + "; update the engine");
    if (root.value("schema", std::string()) != File::SchemaId)
        Problem(0, std::string("not a state machine: \"schema\" must be \"") + File::SchemaId + "\"");
    const int schemaVersion = root.contains("schemaVersion") && root["schemaVersion"].is_number_integer() ? root["schemaVersion"].get<int>() : 0;
    if (schemaVersion < 1 || schemaVersion > File::SchemaVersion)
        Problem(0, "unsupported state machine version " + std::to_string(schemaVersion) + "; update the engine");
    if (!problems_.empty()) return finish();

    try
    {
    auto asset = std::make_shared<MachineAsset>();

    // Memory keys.
    if (root.contains("variables") && !root["variables"].is_array()) Problem(0, "\"variables\" must be an array");
    for (const auto& item : root.value("variables", Json::array()))
    {
        const std::string variable = item.is_object() ? item.value("name", std::string()) : std::string();
        const std::string type = item.is_object() ? item.value("type", std::string()) : std::string();
        VariableType parsed = VariableType::Bool;
        if (variable.empty()) { Problem(0, "a variable without a name"); continue; }
        if (variables_.count(variable)) { Problem(0, "variable " + Quote(variable) + " is declared twice"); continue; }
        if (!ParseVariableType(type, parsed))
        {
            Problem(0, "variable " + Quote(variable) + " has unknown type " + Quote(type) + "; use bool, int, float, vec3, string or object");
            continue;
        }
        variables_.emplace(variable, parsed);
        asset->variables.emplace_back(variable, parsed);
        if (!item.contains("value")) continue;
        Blackboard::Value value;
        if (parsed == VariableType::Object) Problem(0, "variable " + Quote(variable) + ": an object has no starting value");
        else if (!ParseLiteral(item["value"], parsed, value)) Problem(0, "variable " + Quote(variable) + ": the value is not a " + ToString(parsed));
        else asset->memory.emplace_back(variable, std::move(value));
    }

    // Settings of the controller that runs the machine.
    std::vector<std::pair<ServicePtr, float>> globalServices;
    if (root.contains("settings"))
    {
        const Json& settings = root["settings"];
        if (!settings.is_object()) Problem(0, "\"settings\" must be an object");
        else
        {
            for (auto it = settings.begin(); it != settings.end(); ++it)
            {
                const std::string& key = it.key();
                if (key == "targetKey" || key == "lastSeenKey")
                {
                    const VariableType wanted = key == "targetKey" ? VariableType::Object : VariableType::Vec3;
                    const std::string variable = it->is_string() ? it->get<std::string>() : std::string();
                    const auto found = variables_.find(variable);
                    if (variable.empty()) continue;
                    if (found == variables_.end()) Problem(0, "settings: " + Quote(key) + " names no variable " + Quote(variable));
                    else if (found->second != wanted)
                        Problem(0, "settings: " + Quote(key) + " needs a variable of type " + ToString(wanted) + ", " + Quote(variable) + " is " + ToString(found->second));
                    else (key == "targetKey" ? asset->targetKey : asset->lastSeenKey) = variable;
                }
                else if (key == "thinkInterval")
                {
                    if (!it->is_number() || !Finite(it->get<double>()) || it->get<double>() < 0) Problem(0, "settings: \"thinkInterval\" must be a number, 0 or more");
                    else asset->thinkInterval = it->get<float>();
                }
                else if (key == "services") ReadServices(*it, 0, "settings: service", globalServices);
                else Problem(0, "settings: unknown setting " + Quote(key));
            }
        }
    }

    // Nodes.
    if (!root.contains("nodes") || !root["nodes"].is_array()) Problem(0, "\"nodes\" must be an array");
    std::unordered_map<int, NodeKind> kinds;
    std::unordered_map<int, State> states;
    std::vector<int> order;         // state ids as written
    std::set<int> failed;           // nodes reported and skipped
    std::map<std::string, int> byName;
    for (const auto& item : root.value("nodes", Json::array()))
    {
        const int id = item.is_object() && item.contains("id") && item["id"].is_number_integer() ? item["id"].get<int>() : 0;
        if (id <= 0) { Problem(0, "a node without a positive integer id"); continue; }
        if (kinds.count(id) || failed.count(id)) { Problem(id, "node id " + std::to_string(id) + " is used twice"); continue; }
        const std::string type = item.value("type", std::string());
        if (type == File::FinishNode) { kinds.emplace(id, NodeKind::Finish); continue; }
        if (type == File::NextNode) { kinds.emplace(id, NodeKind::Next); continue; }
        if (type != File::StateNode) { Problem(id, "unknown node type " + Quote(type)); failed.insert(id); continue; }

        State state;
        state.id = id;
        state.x = item.contains("x") && item["x"].is_number() ? item["x"].get<double>() : 0.0;
        state.y = item.contains("y") && item["y"].is_number() ? item["y"].get<double>() : 0.0;
        const Json properties = item.contains("properties") && item["properties"].is_object() ? item["properties"] : Json::object();
        state.name = properties.contains("name") && properties["name"].is_string() ? properties["name"].get<std::string>() : std::string();
        if (state.name.empty()) Problem(id, "name the state");
        else if (state.name.find('/') != std::string::npos) Problem(id, "a state name cannot contain '/'");
        else if (!byName.emplace(state.name, id).second) Problem(id, "two states are named " + Quote(state.name));
        if (properties.contains("parent"))
        {
            const Json& parent = properties["parent"];
            if (!parent.is_number_integer() || parent.get<int>() < 0) Problem(id, "\"parent\" must be the id of a state node");
            else state.parent = parent.get<int>();
        }
        for (const char* list : {"tasks", "services"})
            if (properties.contains(list) && !properties[list].is_array()) Problem(id, Quote(list) + " must be an array");
        int index = 0;
        for (const auto& entry : properties.value("tasks", Json::array()))
            if (auto task = ReadTask(entry, id, "task " + std::to_string(++index))) state.tasks.push_back(std::move(task));
        state.enter = ReadConditions(properties.value("enterIf", Json()), id, "enter condition");
        ReadServices(properties.value("services", Json()), id, "service", state.services);
        if (properties.contains("linked"))
        {
            const Json& linked = properties["linked"];
            if (!linked.is_string()) Problem(id, "\"linked\" must be a machine name or a .fsm path");
            else if (!linked.get<std::string>().empty()) state.linked = ReadLinked(linked.get<std::string>(), id);
        }
        kinds.emplace(id, NodeKind::State);
        order.push_back(id);
        states.emplace(id, std::move(state));
    }
    if (states.empty() && problems_.empty()) Problem(0, "the machine has no states");

    // Nesting: parents exist, are states and form no loop.
    for (const int id : order)
    {
        State& state = states.at(id);
        if (!state.parent) continue;
        const auto kind = kinds.find(state.parent);
        if (kind == kinds.end()) { if (!failed.count(state.parent)) Problem(id, "parent " + std::to_string(state.parent) + " is not a node"); state.parent = 0; continue; }
        if (kind->second != NodeKind::State) { Problem(id, "the parent must be a state"); state.parent = 0; continue; }
    }
    for (const int id : order)
    {
        State& state = states.at(id);
        int walk = state.parent, steps = 0;
        while (walk && walk != id && ++steps <= static_cast<int>(states.size())) walk = states.at(walk).parent;
        if (walk == id) { Problem(id, "states are nested inside each other in a loop"); state.parent = 0; }
    }
    std::vector<int> roots;
    for (const int id : order) (states.at(id).parent ? states.at(states.at(id).parent).children : roots).push_back(id);
    // Children are ordered as drawn: top to bottom, then left to right.
    auto drawn = [&](int a, int b) {
        const State& sa = states.at(a);
        const State& sb = states.at(b);
        if (sa.y != sb.y) return sa.y < sb.y;
        if (sa.x != sb.x) return sa.x < sb.x;
        return a < b;
    };
    std::sort(roots.begin(), roots.end(), drawn);
    for (auto& [id, state] : states)
    {
        std::sort(state.children.begin(), state.children.end(), drawn);
        if (state.linked && !state.children.empty()) Problem(id, "a linked state cannot have its own child states");
    }

    // Transitions.
    if (root.contains("connections") && !root["connections"].is_array()) Problem(0, "\"connections\" must be an array");
    int linkIndex = 0;
    for (const auto& link : root.value("connections", Json::array()))
    {
        ++linkIndex;
        if (!link.is_object()) { Problem(0, "a link must be an object"); continue; }
        const int fromId = link.contains("fromNode") && link["fromNode"].is_number_integer() ? link["fromNode"].get<int>() : 0;
        const int toId = link.contains("toNode") && link["toNode"].is_number_integer() ? link["toNode"].get<int>() : 0;
        const auto from = kinds.find(fromId), to = kinds.find(toId);
        if (from == kinds.end() || to == kinds.end())
        {
            const int missing = from == kinds.end() ? fromId : toId;
            if (!failed.count(missing)) Problem(0, "a link to a missing node " + std::to_string(missing));
            continue;
        }
        if (from->second != NodeKind::State) { Problem(fromId, "a transition can start only at a state"); continue; }
        if (link.value("fromPin", std::string("out")) != "out") Problem(fromId, "a state has only the output \"out\"");
        if (link.value("toPin", std::string("in")) != "in") Problem(toId, "the only input is \"in\"");
        State& source = states.at(fromId);
        const State* target = to->second == NodeKind::State ? &states.at(toId) : nullptr;
        const std::string where = "transition to " + (target ? Quote(target->name) : to->second == NodeKind::Finish ? std::string("Finish") : std::string("Next"));
        Transition transition;
        transition.link = linkIndex;
        transition.target = toId;
        const Json properties = link.contains("properties") && link["properties"].is_object() ? link["properties"] : Json::object();
        const std::string trigger = properties.contains("trigger") && properties["trigger"].is_string() ? properties["trigger"].get<std::string>() : std::string("completed");
        using Trigger = Detail::TransitionDef::Trigger;
        if (trigger == "completed") transition.trigger = Trigger::Completed;
        else if (trigger == "succeeded") transition.trigger = Trigger::Succeeded;
        else if (trigger == "failed") transition.trigger = Trigger::Failed;
        else if (trigger == "tick") transition.trigger = Trigger::Tick;
        else if (trigger == "event") transition.trigger = Trigger::Event;
        else Problem(fromId, where + ": unknown trigger " + Quote(trigger) + "; use completed, succeeded, failed, event or tick");
        if (transition.trigger == Trigger::Event)
        {
            transition.event = properties.contains("event") && properties["event"].is_string() ? properties["event"].get<std::string>() : std::string();
            if (transition.event.empty()) Problem(fromId, where + ": name the event");
        }
        transition.conditions = ReadConditions(properties.value("conditions", Json()), fromId, where + ": condition");
        if (properties.contains("delay"))
        {
            const Json& delay = properties["delay"];
            if (!delay.is_number() || !Finite(delay.get<double>()) || delay.get<double>() < 0) Problem(fromId, where + ": \"delay\" must be a number, 0 or more");
            else transition.delay = delay.get<float>();
        }
        if (properties.contains("priority"))
        {
            const Json& priority = properties["priority"];
            if (!priority.is_number_integer()) Problem(fromId, where + ": \"priority\" must be a whole number");
            else transition.priority = priority.get<int>();
        }
        if (to->second == NodeKind::Next)
        {
            const auto& siblings = source.parent ? states.at(source.parent).children : roots;
            if (!siblings.empty() && siblings.back() == fromId) Problem(fromId, where + ": " + Quote(source.name) + " is the last state of its parent");
        }
        source.transitions.push_back(std::move(transition));
    }
    if (!problems_.empty()) return finish();

    // Build: the same checks as a machine written in C++.
    MachineBuilder builder(name.empty() ? std::string("Machine") : name);
    std::unordered_map<int, StateBuilder*> built;
    auto add = [&](auto&& self, int id, StateBuilder* parent) -> void {
        State& state = states.at(id);
        StateBuilder& b = parent ? parent->State(state.name) : builder.State(state.name);
        built[id] = &b;
        for (auto& task : state.tasks) b.AddTask(task);
        for (auto& condition : state.enter) b.EnterIf(condition);
        for (auto& [service, interval] : state.services) b.AddService(service, interval);
        if (state.linked) b.Linked(state.linked);
        for (const int child : state.children) self(self, child, &b);
    };
    for (const int id : roots) add(add, id, nullptr);
    for (auto& [id, state] : states)
    {
        std::stable_sort(state.transitions.begin(), state.transitions.end(), [](const Transition& a, const Transition& b) {
            return a.priority != b.priority ? a.priority < b.priority : a.link < b.link;
        });
        for (const Transition& transition : state.transitions)
        {
            const NodeKind kind = kinds.at(transition.target);
            TransitionBuilder& t = kind == NodeKind::State ? built.at(id)->GoTo(states.at(transition.target).name)
                                                           : built.at(id)->GoTo(kind == NodeKind::Finish ? Target::Finish : Target::Next);
            using Trigger = Detail::TransitionDef::Trigger;
            switch (transition.trigger)
            {
            case Trigger::Succeeded: t.OnSucceeded(); break;
            case Trigger::Failed: t.OnFailed(); break;
            case Trigger::Completed: t.OnCompleted(); break;
            case Trigger::Event: t.OnEvent(Event{transition.event.c_str()}); break;
            default: break;
            }
            for (const auto& condition : transition.conditions) t.If(condition);
            if (transition.delay > 0) t.After(transition.delay);
        }
    }
    for (auto& [service, interval] : globalServices) builder.AddService(service, interval);
    try
    {
        asset->machine = builder.Build();
    }
    catch (const std::exception& error)
    {
        Problem(0, error.what());
        return finish();
    }
    // Paths of the file's own states, for the editor's debugger.
    for (const auto& [id, state] : states)
    {
        std::string path = state.name;
        for (int parent = state.parent; parent; parent = states.at(parent).parent) path = states.at(parent).name + "/" + path;
        asset->stateNodes.emplace(std::move(path), id);
    }
    result.asset = std::move(asset);
    }
    catch (const Json::exception& error)
    {
        // A field of the wrong JSON type deep inside the file.
        Problem(0, std::string("invalid state machine file: ") + error.what());
    }
    return finish();
}

std::string Physical(const std::string& path)
{
    std::string physical = VfsRead::Active().Resolve(path);
    return physical.empty() ? path : physical;
}

bool ReadFile(const std::string& physical, std::string& text, std::filesystem::file_time_type& time)
{
    const auto file = std::filesystem::u8path(physical);
    std::error_code error;
    time = std::filesystem::last_write_time(file, error);
    if (error) return false;
    std::ifstream input(file, std::ios::binary);
    if (!input) return false;
    text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (text.compare(0, 3, "\xef\xbb\xbf") == 0) text.erase(0, 3); // UTF-8 BOM
    return true;
}

struct CachedMachine
{
    std::vector<std::pair<std::string, std::filesystem::file_time_type>> files; // every file read, the machine's own first
    MachineLoadResult result;
};
} // namespace

bool IsMachineFile(std::string_view nameOrPath)
{
    const std::string_view extension = File::Extension;
    if (nameOrPath.size() < extension.size()) return false;
    const std::string_view tail = nameOrPath.substr(nameOrPath.size() - extension.size());
    return std::equal(tail.begin(), tail.end(), extension.begin(), [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; });
}

MachineLoadResult LoadMachine(const std::string& json, const std::string& name, const MachineFileReader& reader)
{
    return Loader(reader, {}).Load(json, name);
}

MachineLoadResult LoadMachineFile(const std::string& path)
{
    static std::map<std::string, CachedMachine> cache;
    const std::string physical = Physical(path);
    const auto found = cache.find(physical);
    if (found != cache.end())
    {
        bool fresh = true;
        for (const auto& [file, time] : found->second.files)
        {
            std::error_code error;
            const auto now = std::filesystem::last_write_time(std::filesystem::u8path(file), error);
            if (error || now != time) { fresh = false; break; }
        }
        if (fresh) return found->second.result;
    }
    CachedMachine entry;
    std::string text;
    std::filesystem::file_time_type time;
    if (!ReadFile(physical, text, time))
    {
        cache.erase(physical);
        return {nullptr, {{0, "cannot read the file"}}};
    }
    entry.files.emplace_back(physical, time);
    const MachineFileReader reader = [&entry](const std::string& linked, std::string& linkedText) {
        const std::string file = Physical(linked);
        std::filesystem::file_time_type linkedTime;
        if (!ReadFile(file, linkedText, linkedTime)) return false;
        entry.files.emplace_back(file, linkedTime);
        return true;
    };
    entry.result = Loader(reader, {path}).Load(text, std::filesystem::u8path(path).stem().u8string());
    return (cache[physical] = std::move(entry)).result;
}

void MachineAsset::Run(Controller& controller, const std::string& values) const
{
    if (!machine) throw std::invalid_argument("MachineAsset::Run: no machine");
    for (const auto& [key, value] : memory) SetMemory(controller.Memory(), key, value);
    if (!values.empty())
    {
        const Json overrides = Json::parse(values, nullptr, false);
        if (overrides.is_object())
            for (const auto& [key, type] : variables)
            {
                Blackboard::Value value;
                if (overrides.contains(key) && ParseLiteral(overrides[key], type, value)) SetMemory(controller.Memory(), key, value);
            }
    }
    if (!targetKey.empty()) controller.GetPerception().WriteTargetTo(Key<ObjectRef>{targetKey.c_str()});
    if (!lastSeenKey.empty()) controller.GetPerception().WriteLastSeenTo(Key<glm::vec3>{lastSeenKey.c_str()});
    if (thinkInterval >= 0) controller.SetThinkInterval(thinkInterval);
    controller.Run(machine);
}
} // namespace Ai
