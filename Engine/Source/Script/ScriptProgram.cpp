#include "ScriptProgram.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace Script
{
namespace
{
using Json = nlohmann::json;

bool ParseValueType(const std::string& text, ValueType& type)
{
    static const std::unordered_map<std::string, ValueType> types{
        {"bool", ValueType::Bool}, {"float", ValueType::Float}, {"vec3", ValueType::Vec3}, {"string", ValueType::String}};
    const auto found = types.find(text);
    if (found == types.end()) return false;
    type = found->second;
    return true;
}

bool Finite(double value) { return std::isfinite(value) && std::abs(value) <= 3.4e38; }

// A literal of the given type from JSON: a property value, a variable default or an input default.
bool ParseLiteral(const Json& json, ValueType type, Value& value)
{
    switch (type)
    {
    case ValueType::Bool:
        if (!json.is_boolean()) return false;
        value = json.get<bool>();
        return true;
    case ValueType::Float:
        if (!json.is_number() || !Finite(json.get<double>())) return false;
        value = json.get<float>();
        return true;
    case ValueType::Vec3:
        if (!json.is_array() || json.size() != 3) return false;
        for (const auto& item : json)
            if (!item.is_number() || !Finite(item.get<double>())) return false;
        value = glm::vec3(json[0].get<float>(), json[1].get<float>(), json[2].get<float>());
        return true;
    case ValueType::String:
        if (!json.is_string()) return false;
        value = json.get<std::string>();
        return true;
    case ValueType::Any: // Print: whatever literal is written
        return ParseLiteral(json, ValueType::Bool, value) || ParseLiteral(json, ValueType::Float, value) ||
               ParseLiteral(json, ValueType::Vec3, value) || ParseLiteral(json, ValueType::String, value);
    default:
        return false;
    }
}

int OutputIndex(const std::vector<PinInfo>& pins, const std::string& name, bool exec)
{
    int index = 0;
    for (const auto& pin : pins)
    {
        if (pin.input || (pin.type == ValueType::Exec) != exec) continue;
        if (pin.name == name) return index;
        ++index;
    }
    return -1;
}

int InputIndex(const std::vector<PinInfo>& pins, const std::string& name)
{
    int index = 0;
    for (const auto& pin : pins)
    {
        if (!pin.input || pin.type == ValueType::Exec) continue;
        if (pin.name == name) return index;
        ++index;
    }
    return -1;
}

const PinInfo* FindPin(const std::vector<PinInfo>& pins, const std::string& name, bool input)
{
    for (const auto& pin : pins)
        if (pin.name == name && pin.input == input) return &pin;
    return nullptr;
}
} // namespace

void ApplyVariableValues(const Program& program, const std::string& json, std::vector<Value>& values)
{
    if (json.empty()) return;
    const Json root = Json::parse(json, nullptr, false);
    if (!root.is_object()) return;
    for (auto it = root.begin(); it != root.end(); ++it)
    {
        const int index = program.FindVariable(it.key());
        if (index < 0 || size_t(index) >= values.size()) continue;
        Value value;
        if (ParseLiteral(it.value(), program.variables[index].type, value)) values[index] = value;
    }
}

int Program::FindVariable(const std::string& name) const
{
    for (size_t i = 0; i < variables.size(); ++i)
        if (variables[i].name == name) return int(i);
    return -1;
}

CompileResult Compile(const std::string& text)
{
    CompileResult result;
    auto problem = [&](int node, std::string message) { result.problems.push_back({node, std::move(message)}); };

    Json root;
    try
    {
        root = Json::parse(text);
    }
    catch (const Json::exception& error)
    {
        problem(0, std::string("invalid JSON: ") + error.what());
        return result;
    }
    if (!root.is_object())
    {
        problem(0, "the root must be a JSON object");
        return result;
    }
    if (root.value("format", std::string()) != FormatName)
    {
        problem(0, std::string("not a visual script: \"format\" must be \"") + FormatName + "\"");
        return result;
    }
    const int version = root.contains("version") && root["version"].is_number_integer() ? root["version"].get<int>() : 0;
    if (version < 1 || version > FormatVersion)
        problem(0, "unsupported graph format version " + std::to_string(version) + "; update the engine");
    if (root.value("schema", std::string()) != SchemaId)
        problem(0, std::string("not a code graph: \"schema\" must be \"") + SchemaId + "\"");
    const int schemaVersion = root.contains("schemaVersion") && root["schemaVersion"].is_number_integer() ? root["schemaVersion"].get<int>() : 0;
    if (schemaVersion < 1 || schemaVersion > SchemaVersion)
        problem(0, "unsupported script version " + std::to_string(schemaVersion) + "; update the engine");
    if (!result.problems.empty()) return result;

    try
    {
    auto program = std::make_shared<Program>();

    // Variables.
    if (root.contains("variables") && !root["variables"].is_array()) problem(0, "\"variables\" must be an array");
    for (const auto& item : root.value("variables", Json::array()))
    {
        Variable variable;
        variable.name = item.is_object() ? item.value("name", std::string()) : std::string();
        const std::string type = item.is_object() ? item.value("type", std::string()) : std::string();
        if (variable.name.empty()) { problem(0, "a variable without a name"); continue; }
        if (program->FindVariable(variable.name) >= 0) { problem(0, "variable \"" + variable.name + "\" is declared twice"); continue; }
        if (!ParseValueType(type, variable.type)) { problem(0, "variable \"" + variable.name + "\" has unknown type \"" + type + "\""); continue; }
        variable.initial = DefaultValue(variable.type);
        if (item.contains("value") && !ParseLiteral(item["value"], variable.type, variable.initial))
            problem(0, "variable \"" + variable.name + "\": the value is not a " + TypeName(variable.type));
        program->variables.push_back(std::move(variable));
    }

    // Nodes.
    if (!root.contains("nodes") || !root["nodes"].is_array()) problem(0, "\"nodes\" must be an array");
    std::unordered_map<int, int> indexOf;      // file id -> index
    std::vector<std::vector<PinInfo>> pinsOf;   // per node index
    std::unordered_set<int> failedIds;          // nodes reported and skipped
    for (const auto& item : root.value("nodes", Json::array()))
    {
        const int id = item.is_object() && item.contains("id") && item["id"].is_number_integer() ? item["id"].get<int>() : 0;
        if (id <= 0) { problem(0, "a node without a positive integer id"); continue; }
        if (indexOf.count(id) || failedIds.count(id)) { problem(id, "node id " + std::to_string(id) + " is used twice"); continue; }
        const std::string type = item.value("type", std::string());
        const NodeSpec* spec = FindSpec(type);
        if (!spec) { problem(id, "unknown node type \"" + type + "\""); failedIds.insert(id); continue; }
        const Json properties = item.contains("properties") && item["properties"].is_object() ? item["properties"] : Json::object();

        CompiledNode node;
        node.id = id;
        node.op = spec->op;
        node.type = type;

        int outputs = 2;
        if (spec->op == Op::Sequence && properties.contains("outputs"))
        {
            const Json& count = properties["outputs"];
            if (!count.is_number_integer() || count.get<int>() < 1 || count.get<int>() > MaxSequenceOutputs)
                problem(id, "\"outputs\" must be a whole number from 1 to " + std::to_string(MaxSequenceOutputs));
            else
                outputs = count.get<int>();
        }
        std::vector<PinInfo> pins = NodePins(spec->info, outputs);

        for (const auto& property : spec->info.properties)
        {
            if (property.kind == "key")
            {
                const std::string name = properties.value("key", std::string());
                node.key = KeyCode(name);
                if (!node.key) problem(id, name.empty() ? std::string("choose a key") : "unknown key \"" + name + "\"");
            }
            else if (property.kind == "variable")
            {
                const std::string name = properties.value("variable", std::string());
                node.variable = program->FindVariable(name);
                if (node.variable < 0) problem(id, name.empty() ? std::string("choose a variable") : "no variable \"" + name + "\"");
            }
            else if (property.name == "seconds")
            {
                const Json& seconds = properties.value("seconds", Json(1.0));
                if (!seconds.is_number() || !(seconds.get<double>() > 0.0)) problem(id, "\"seconds\" must be a number greater than 0");
                else node.constant = float(seconds.get<double>());
            }
            else if (property.name == "memory")
            {
                const Json& name = properties.value("memory", Json(std::string()));
                node.text = name.is_string() ? name.get<std::string>() : std::string();
                if (node.text.empty()) problem(id, "name the AI memory key");
            }
            else if (property.name == "event")
            {
                const Json& name = properties.value("event", Json(std::string()));
                node.text = name.is_string() ? name.get<std::string>() : std::string();
                if (node.text.empty()) problem(id, "name the event");
            }
            else if (property.name == "value")
            {
                ValueType type = ValueType::Float;
                ParseValueType(property.kind, type);
                node.constant = DefaultValue(type);
                if (properties.contains("value") && !ParseLiteral(properties["value"], type, node.constant))
                    problem(id, std::string("the value is not a ") + TypeName(type));
            }
        }

        // Variable pins take the type of the chosen variable.
        const ValueType variableType = node.variable >= 0 ? program->variables[node.variable].type : ValueType::Float;
        for (auto& pin : pins)
            if (pin.type == ValueType::Variable) pin.type = variableType;

        const Json defaults = properties.contains("inputs") && properties["inputs"].is_object() ? properties["inputs"] : Json::object();
        for (const auto& pin : pins)
        {
            if (pin.type == ValueType::Exec)
            {
                if (!pin.input) node.next.push_back(-1);
                continue;
            }
            if (!pin.input)
            {
                node.outputNames.push_back(pin.name);
                continue;
            }
            InputSlot slot;
            slot.type = pin.type;
            slot.fallback = DefaultValue(pin.type == ValueType::Any ? ValueType::String : pin.type);
            slot.selfByDefault = pin.type == ValueType::Object;
            if (defaults.contains(pin.name) && !ParseLiteral(defaults[pin.name], pin.type, slot.fallback))
                problem(id, "default of input " + pin.name + " is not a " + TypeName(pin.type));
            node.inputs.push_back(std::move(slot));
        }

        indexOf.emplace(id, int(program->nodes.size()));
        pinsOf.push_back(std::move(pins));
        program->nodes.push_back(std::move(node));
    }

    // Links.
    if (root.contains("connections") && !root["connections"].is_array()) problem(0, "\"connections\" must be an array");
    for (const auto& link : root.value("connections", Json::array()))
    {
        if (!link.is_object()) { problem(0, "a link must be an object"); continue; }
        const int fromId = link.value("fromNode", 0), toId = link.value("toNode", 0);
        const std::string fromPin = link.value("fromPin", std::string()), toPin = link.value("toPin", std::string());
        const auto from = indexOf.find(fromId), to = indexOf.find(toId);
        if (from == indexOf.end() || to == indexOf.end())
        {
            // A link of a node that failed to load is already reported on that node.
            const int missing = from == indexOf.end() ? fromId : toId;
            if (!failedIds.count(missing)) problem(0, "a link to a missing node " + std::to_string(missing));
            continue;
        }
        const PinInfo* output = FindPin(pinsOf[from->second], fromPin, false);
        const PinInfo* input = FindPin(pinsOf[to->second], toPin, true);
        if (!output) { problem(fromId, "no output \"" + fromPin + "\""); continue; }
        if (!input) { problem(toId, "no input \"" + toPin + "\""); continue; }
        const bool execOut = output->type == ValueType::Exec, execIn = input->type == ValueType::Exec;
        if (execOut != execIn)
        {
            problem(toId, "input " + toPin + (execIn ? " takes the execution flow, not a value" : " takes a value, not the execution flow"));
            continue;
        }
        if (execOut)
        {
            int& next = program->nodes[from->second].next[OutputIndex(pinsOf[from->second], fromPin, true)];
            if (next >= 0) { problem(fromId, "output " + fromPin + " has more than one link"); continue; }
            next = to->second;
            continue;
        }
        if (input->type != ValueType::Any && input->type != output->type)
        {
            problem(toId, std::string("input ") + toPin + " takes " + TypeName(input->type) + ", got " + TypeName(output->type));
            continue;
        }
        InputSlot& slot = program->nodes[to->second].inputs[InputIndex(pinsOf[to->second], toPin)];
        if (slot.node >= 0) { problem(toId, "input " + toPin + " has more than one link"); continue; }
        slot.node = from->second;
        slot.output = OutputIndex(pinsOf[from->second], fromPin, false);
    }

    // Values must not depend on themselves.
    std::vector<int> state(program->nodes.size(), 0); // 0 new, 1 on the stack, 2 done
    std::function<bool(int)> visit = [&](int index) {
        if (state[index] == 1) return false;
        if (state[index] == 2) return true;
        state[index] = 1;
        for (const auto& slot : program->nodes[index].inputs)
            if (slot.node >= 0 && !visit(slot.node)) return false;
        state[index] = 2;
        return true;
    };
    std::unordered_set<int> reported;
    for (size_t i = 0; i < program->nodes.size(); ++i)
        if (state[i] == 0 && !visit(int(i)))
        {
            for (size_t j = 0; j < program->nodes.size(); ++j)
                if (state[j] == 1 && reported.insert(int(j)).second) problem(program->nodes[j].id, "the value depends on itself (a data cycle)");
            std::fill(state.begin(), state.end(), 2);
            break;
        }

    for (size_t i = 0; i < program->nodes.size(); ++i)
    {
        const Op op = program->nodes[i].op;
        if (op == Op::BeginPlay) program->beginPlay.push_back(int(i));
        else if (op == Op::Tick) program->tick.push_back(int(i));
        else if (op == Op::KeyPressed || op == Op::KeyReleased) program->keyEvents.push_back(int(i));
        else if (op == Op::Every) program->every.push_back(int(i));
        else if (op == Op::CustomEvent) program->customEvents.push_back(int(i));
    }

    if (result.problems.empty()) result.program = std::move(program);
    }
    catch (const Json::exception& error)
    {
        // A field of the wrong JSON type deep inside the graph.
        problem(0, std::string("invalid graph: ") + error.what());
        result.program.reset();
    }
    return result;
}
} // namespace Script
