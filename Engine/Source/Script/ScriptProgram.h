#pragma once

#include <Engine/Script/ScriptRuntime.h>

namespace Script
{
enum class Op : uint16_t
{
    BeginPlay, Tick, KeyPressed, KeyReleased,
    Branch, Sequence,
    Print, SetPosition, SetRotation, SetScale, Translate, Rotate, SetVariable,
    Self, FindObject, GetPosition, GetRotation, GetScale, GetVariable,
    ConstFloat, ConstBool, ConstVec3, ConstString,
    MakeVec3, BreakVec3, Vec3Add, Vec3Subtract, Vec3Scale,
    Add, Subtract, Multiply, Divide, Sin, Lerp, Clamp, Random, Time,
    Less, Greater, Equal, And, Or, Not,
    KeyDown
};

// Catalog entry with its operation (the public NodeInfo has none).
struct NodeSpec
{
    NodeInfo info;
    Op op;
};
const std::vector<NodeSpec>& Specs();
const NodeSpec* FindSpec(const std::string& type);

// A data input: the output it reads, or its default when nothing is connected.
struct InputSlot
{
    int node = -1;  // index in Program::nodes
    int output = -1; // data output index of that node
    ValueType type = ValueType::Float;
    Value fallback;
    bool selfByDefault = false; // unconnected object input: the script's own object
};

struct CompiledNode
{
    int id = 0;        // node id in the file
    Op op = Op::BeginPlay;
    std::string type;
    std::vector<InputSlot> inputs; // data inputs in pin order
    std::vector<int> next;         // exec outputs in pin order: target node index, -1 when unconnected
    Value constant;
    int variable = -1;
    int key = 0;
};

struct Variable
{
    std::string name;
    ValueType type = ValueType::Float;
    Value initial;
};

class Program
{
public:
    std::vector<CompiledNode> nodes;
    std::vector<Variable> variables;
    std::vector<int> beginPlay, tick, keyEvents; // node indices
    int FindVariable(const std::string& name) const; // -1: none
};

Value DefaultValue(ValueType type);
} // namespace Script
