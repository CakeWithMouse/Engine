#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <glm/glm.hpp>

class GameComponent;

/**
 * Visual scripts: graphs of events and actions saved as .vscript (JSON, "ITMOEngine.Graph" format,
 * schema "code"). The engine interprets them, so projects without C++ run them too.
 *
 * A script is attached to one object. It runs only in Play and in the game: BeginPlay once after the
 * component's BeginPlay, then every frame key events and Tick after the component's Tick. A runtime
 * error disables that object's script until the next Play; the game keeps running.
 * See Engine/docs/VISUAL-SCRIPTING.md.
 */
namespace Script
{
constexpr const char* FormatName = "ITMOEngine.Graph";
constexpr int FormatVersion = 1;
constexpr const char* SchemaId = "code";
constexpr int SchemaVersion = 1;
constexpr int StepLimit = 10000;  // nodes run or evaluated per event before "probably an endless loop"
constexpr int MaxSequenceOutputs = 8;
constexpr int MaxEventDepth = 16; // Call event inside Call event…: deeper is "events call each other too deeply"

// Variable: the pin takes the type of the variable named by the node's "variable" property.
enum class ValueType : uint8_t { Exec, Bool, Float, Vec3, String, Object, Any, Variable };
const char* TypeName(ValueType type);

struct ObjectRef
{
    uint64_t id = 0; // ComponentId; 0 means "no object"
    bool operator==(const ObjectRef& other) const { return id == other.id; }
};
using Value = std::variant<std::monostate, bool, float, glm::vec3, std::string, ObjectRef>;
std::string ToText(const Value& value);

struct PinInfo
{
    std::string name;
    bool input = false;
    ValueType type = ValueType::Float;
};

// Node properties the editor shows: key (a key name), variable (a variable name), outputs (Sequence),
// value (the type of the constant).
struct PropertyInfo
{
    std::string name;
    std::string kind; // "key", "variable", "outputs", "bool", "float", "vec3", "string"
};

struct NodeInfo
{
    std::string type;
    std::string group;
    std::string nameRu, nameEn, hintRu, hintEn;
    std::vector<PinInfo> pins; // Sequence also gets then0..then(N-1) from its "outputs" property
    std::vector<PropertyInfo> properties;
    bool event = false;
};

const std::vector<NodeInfo>& Catalog();
const NodeInfo* FindNode(const std::string& type);
// Pins of a node with these properties (Sequence outputs follow "outputs").
std::vector<PinInfo> NodePins(const NodeInfo& node, int sequenceOutputs);
// Key names accepted by the "key" property (W, Space, Left, D1, F1, LeftButton…).
const std::vector<std::string>& KeyNames();
int KeyCode(const std::string& name); // 0: unknown
// The catalog for the editor: schema, node types, pins, properties and key names as JSON.
std::string CatalogJson();

// node 0: the file as a whole.
struct Problem
{
    int node = 0;
    std::string message;
};

class Program;
struct CompileResult
{
    std::shared_ptr<const Program> program; // null when there are problems
    std::vector<Problem> problems;
};
// Builds a program from .vscript text. Every problem is reported, each with its node id.
CompileResult Compile(const std::string& json);

// One running script of one object.
class Instance
{
public:
    // source: the /Game path, used in messages.
    Instance(std::shared_ptr<const Program> program, GameComponent& self, std::string source);
    ~Instance();
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    void BeginPlay();
    void Tick(float deltaTime);
    // Custom events named name (Action.CallEvent of this or another object); false: none ran.
    bool Trigger(const std::string& name);

    bool Failed() const { return failed_; }
    const Problem& Error() const { return error_; }
    const std::string& Source() const { return source_; }
    // Text of "Print" nodes, newest last; the last 100 lines are kept.
    const std::vector<std::string>& Printed() const { return printed_; }
    Value GetVariable(const std::string& name) const;

    // Debug trace for the editor (Engine.Scene.GraphTrace): nodes that ran or were evaluated at least once.
    // framesAgo 0 — during the latest frame (BeginPlay counts as the first one).
    struct NodeTrace
    {
        int node = 0; // node id in the file
        uint32_t framesAgo = 0;
        uint32_t count = 0;
    };
    std::vector<NodeTrace> Trace() const;
    // The last value every data output gave to a reader since Play started.
    struct OutputValue
    {
        int node = 0;      // node id in the file
        std::string pin;   // output name
        std::string text;  // ToText of the value
    };
    std::vector<OutputValue> Values() const;

private:
    struct Event;
    void Run(int node, const Event& event);
    void Execute(int node, const Event& event, int depth);
    Value Evaluate(int node, int output, const Event& event, int depth);
    Value Input(int node, int input, const Event& event, int depth);
    GameComponent* Resolve(const ObjectRef& object, int node) const;
    bool KeyDown(int key) const;
    void Step(int node, bool traced = true);
    void Fail(const Problem& problem);
    void Print(const std::string& text);

    std::shared_ptr<const Program> program_;
    GameComponent& self_;
    std::string source_;
    std::vector<Value> variables_;
    std::vector<bool> keyWasDown_; // per key event node
    uint64_t frame_ = 0;              // Tick calls since BeginPlay
    std::vector<uint64_t> ranAt_;     // per program node: frame_ + 1 of the last run, 0 — never
    std::vector<uint32_t> runs_;
    std::vector<std::vector<Value>> outputs_; // per program node, per data output; monostate — never read
    std::vector<float> nodeNumber_;  // per node: Every timer, For index, FlipFlop side (1 — A)
    std::vector<uint8_t> nodeFlag_;  // per node: DoOnce done, Delay waiting, FlipFlop next side
    struct Pending
    {
        int node; // Delay node index
        float due;
    };
    std::vector<Pending> delays_;
    std::unordered_map<int, uint64_t> spawned_; // Spawn node index -> the object it created last
    void Continue(int node, const Event& event); // runs a chain that does not start at an event
    float elapsed_ = 0.0f;
    int steps_ = 0;
    bool failed_ = false;
    bool started_ = false;
    Problem error_;
    std::vector<std::string> printed_;
    std::mt19937 random_{std::random_device{}()};
};
} // namespace Script
