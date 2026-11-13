#pragma once
#include <Engine/AI/AiTypes.h>
#include <Engine/AI/StateMachine.h>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// State machines described in .fsm files and the blocks they are made of.
//
// A .fsm is JSON in the "ITMOEngine.Graph" format with schema "ai.fsm": the AI editor saves it, the engine
// builds the same Ai::StateMachine from it that MachineBuilder builds in C++. States, transitions and memory
// keys are written in the file; what a state does is a list of blocks: tasks, conditions and services.
// Built-in blocks are named "Ai.*". A game adds its own with ENGINE_AI_BLOCK, and the editor offers them by name.
// Format and blocks: Engine/docs/AI-FSM.md.
namespace Ai
{
class Controller;

namespace File
{
constexpr const char* FormatName = "ITMOEngine.Graph";
constexpr int FormatVersion = 1;
constexpr const char* SchemaId = "ai.fsm";
constexpr int SchemaVersion = 1;
constexpr const char* Extension = ".fsm";
constexpr const char* StateNode = "Fsm.State";    // a state; "parent" puts it inside another state
constexpr const char* FinishNode = "Fsm.Finish";  // transition target: stop the machine (or complete the linked state)
constexpr const char* NextNode = "Fsm.Next";      // transition target: the next state of the same parent
constexpr int MaxLinkDepth = 8;                   // a .fsm linking a .fsm linking…
}

enum class BlockKind { Task, Condition, Service };

/** Type of a memory variable: the index of the alternative in Blackboard::Value. */
enum class VariableType { Bool, Int, Float, Vec3, String, Object };
const char* ToString(VariableType type);

/**
 * Type of a block parameter. Key* parameters name a memory key declared in the file's "variables",
 * with the variable type they accept. Value is a literal of the type of the block's "key" parameter.
 */
enum class ParamType
{
    Bool, Int, Float, Vec3, String, Event,
    Key,        // a key of any type
    KeyBool, KeyInt, KeyFloat, KeyVec3, KeyString, KeyObject,
    KeyNumber,  // int or float
    KeyPlace,   // object or vec3: somewhere to go or to measure to
    Value
};
const char* ToString(ParamType type);

struct ParamSpec
{
    std::string name;                // as written in the file
    ParamType type = ParamType::Float;
    std::string labelRu, labelEn;
    Blackboard::Value fallback{};    // when the file leaves the parameter out
    double minimum = -std::numeric_limits<double>::infinity();
    double maximum = std::numeric_limits<double>::infinity();
    bool required = false;           // keys and events always are
};

namespace Param
{
ParamSpec Bool(std::string name, std::string labelRu, std::string labelEn, bool fallback = false);
ParamSpec Int(std::string name, std::string labelRu, std::string labelEn, int fallback = 0,
              double minimum = -std::numeric_limits<double>::infinity(), double maximum = std::numeric_limits<double>::infinity());
ParamSpec Float(std::string name, std::string labelRu, std::string labelEn, float fallback = 0,
                double minimum = -std::numeric_limits<double>::infinity(), double maximum = std::numeric_limits<double>::infinity());
ParamSpec Vec3(std::string name, std::string labelRu, std::string labelEn, glm::vec3 fallback = glm::vec3(0));
ParamSpec String(std::string name, std::string labelRu, std::string labelEn, std::string fallback = {}, bool required = false);
ParamSpec Event(std::string name, std::string labelRu, std::string labelEn);
/** type is one of the Key* types. */
ParamSpec Key(std::string name, ParamType type, std::string labelRu, std::string labelEn);
ParamSpec Value(std::string name, std::string labelRu, std::string labelEn);
}

/**
 * Parameter values of one block as written in the file, checked against its ParamSpec. Given to factories.
 * String() also returns the name of a key or an event. Factories copy what they keep: Params does not outlive the load.
 */
class Params
{
public:
    bool Bool(std::string_view name) const;
    int Int(std::string_view name) const;
    float Float(std::string_view name) const;
    glm::vec3 Vec3(std::string_view name) const;
    const std::string& String(std::string_view name) const;
    /** Type of the variable a Key* parameter names. */
    VariableType KeyType(std::string_view name) const;
    const Blackboard::Value& Get(std::string_view name) const;

    void Set(std::string name, Blackboard::Value value) { values_[std::move(name)] = std::move(value); }
    void SetKeyType(std::string name, VariableType type) { keyTypes_[std::move(name)] = type; }

private:
    std::map<std::string, Blackboard::Value, std::less<>> values_;
    std::map<std::string, VariableType, std::less<>> keyTypes_;
};

using TaskFactory = std::function<std::shared_ptr<const TaskBase>(const Params&)>;
using ConditionFactory = std::function<ConditionFn(const Params&)>;
using ServiceFactory = std::function<std::shared_ptr<const ServiceBase>(const Params&)>;

/** A block the editor offers and a .fsm may use. A factory may throw std::invalid_argument for bad values. */
struct BlockSpec
{
    std::string type;                // unique name in files: "Battle.Fire". "Ai." and "Fsm." belong to the engine.
    BlockKind kind = BlockKind::Task;
    std::string nameRu, nameEn, hintRu, hintEn;
    std::vector<ParamSpec> params;
    TaskFactory task;                // the one that matches kind
    ConditionFactory condition;
    ServiceFactory service;
};

BlockSpec TaskBlock(std::string type, std::string nameRu, std::string nameEn, std::vector<ParamSpec> params, TaskFactory factory,
                    std::string hintRu = {}, std::string hintEn = {});
BlockSpec ConditionBlock(std::string type, std::string nameRu, std::string nameEn, std::vector<ParamSpec> params, ConditionFactory factory,
                         std::string hintRu = {}, std::string hintEn = {});
BlockSpec ServiceBlock(std::string type, std::string nameRu, std::string nameEn, std::vector<ParamSpec> params, ServiceFactory factory,
                       std::string hintRu = {}, std::string hintEn = {});

/** Registers or replaces a block. Throws std::invalid_argument for an empty or reserved name, a missing factory, bad parameters. */
void RegisterBlock(BlockSpec block);
const BlockSpec* FindBlock(std::string_view type);
/** Every block, built-in ones included, sorted by type. */
std::vector<const BlockSpec*> Blocks();

/** For ENGINE_AI_BLOCK: registers while the program (or the game module) starts. */
struct BlockRegistration
{
    explicit BlockRegistration(BlockSpec block) { RegisterBlock(std::move(block)); }
};

/** node 0: the file as a whole. Messages are English; the editor translates them. */
struct MachineProblem
{
    int node = 0;
    std::string message;
};

/** A machine from a .fsm and what the file adds to it: memory values and perception settings. */
struct MachineAsset
{
    MachinePtr machine;
    std::vector<std::pair<std::string, VariableType>> variables;   // every declared memory key, in file order
    std::vector<std::pair<std::string, Blackboard::Value>> memory; // variables that have a value
    std::string targetKey, lastSeenKey;                            // perception writes here; empty: nowhere
    float thinkInterval = -1;                                      // below 0: the controller keeps its own
    /**
     * Sets the memory values and the settings, then runs the machine. values: JSON object {"key": literal} of one
     * object (AiCharacter's "machineMemory"), applied over the file's values; undeclared keys and literals of
     * another type are skipped, text that is not a JSON object is ignored.
     */
    void Run(Controller& controller, const std::string& values = {}) const;
};

struct MachineLoadResult
{
    std::shared_ptr<const MachineAsset> asset; // null when there are problems
    std::vector<MachineProblem> problems;
};

/** Text of a .fsm a state links by path: true and the text, false when the file cannot be read. */
using MachineFileReader = std::function<bool(const std::string& path, std::string& text)>;

/**
 * Builds a machine from .fsm text. Every problem is reported, each with its node id. name names the machine
 * in messages and the debugger. reader reads linked .fsm files; without it only machines registered in C++ link.
 */
MachineLoadResult LoadMachine(const std::string& json, const std::string& name, const MachineFileReader& reader = {});
/** A .fsm by its /Game path (or a path on disk), linked files read the same way. Cached until a file changes. */
MachineLoadResult LoadMachineFile(const std::string& path);
/** True for a name that ends with .fsm: AiCharacter's "stateMachine" then loads the file. */
bool IsMachineFile(std::string_view nameOrPath);
/** The format for the editor: node types, transition triggers, variable types and every block with its parameters. */
std::string MachineCatalogJson();
} // namespace Ai

/**
 * ENGINE_AI_BLOCK(Ai::TaskBlock("Battle.Fire", "Огонь", "Fire", {Ai::Param::Int("salvos", "Залпов", "Salvos", 3, 1, 10)},
 *     [](const Ai::Params& p) { return std::make_shared<Fire>(p.Int("salvos")); }))
 * in a .cpp of the game project. Not for static libraries: the linker may drop the file.
 */
#define ENGINE_AI_BLOCK(...) \
    static const ::Ai::BlockRegistration ENGINE_AI_DETAIL_JOIN(engineAiBlock_, __LINE__){__VA_ARGS__};
