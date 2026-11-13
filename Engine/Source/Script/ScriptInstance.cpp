#include "ScriptProgram.h"

#include <Engine/Components/GameComponents.h>
#include <Engine/InputDevice/Keys.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Scene/Scene.h>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace Script
{
namespace
{
// Thrown inside a run; the event entry turns it into Instance::Error and disables the script.
struct ScriptFailure
{
    Problem problem;
};

thread_local int eventDepth = 0; // nested Call event across all scripts of this thread

[[noreturn]] void Raise(int node, std::string message)
{
    throw ScriptFailure{{node, std::move(message)}};
}

template <class T>
T As(const Value& value, int node)
{
    if (const T* typed = std::get_if<T>(&value)) return *typed;
    Raise(node, "internal: unexpected value type");
}
} // namespace

struct Instance::Event
{
    float deltaTime = 0.0f;
};

Instance::Instance(std::shared_ptr<const Program> program, GameComponent& self, std::string source)
    : program_(std::move(program)), self_(self), source_(std::move(source))
{
    for (const auto& variable : program_->variables) variables_.push_back(variable.initial);
    ApplyVariableValues(*program_, self_.GetScriptVariables(), variables_);
    keyWasDown_.assign(program_->keyEvents.size(), false);
    ranAt_.assign(program_->nodes.size(), 0);
    runs_.assign(program_->nodes.size(), 0);
    nodeNumber_.assign(program_->nodes.size(), 0.0f);
    nodeFlag_.assign(program_->nodes.size(), 0);
    outputs_.resize(program_->nodes.size());
    for (size_t i = 0; i < program_->nodes.size(); ++i) outputs_[i].resize(program_->nodes[i].outputNames.size());
}

std::vector<Instance::OutputValue> Instance::Values() const
{
    std::vector<OutputValue> values;
    for (size_t i = 0; i < outputs_.size(); ++i)
        for (size_t o = 0; o < outputs_[i].size(); ++o)
            if (!std::holds_alternative<std::monostate>(outputs_[i][o]))
                values.push_back({program_->nodes[i].id, program_->nodes[i].outputNames[o], ToText(outputs_[i][o])});
    return values;
}

std::vector<Instance::NodeTrace> Instance::Trace() const
{
    std::vector<NodeTrace> trace;
    for (size_t i = 0; i < ranAt_.size(); ++i)
        if (ranAt_[i]) trace.push_back({program_->nodes[i].id, static_cast<uint32_t>(frame_ + 1 - ranAt_[i]), runs_[i]});
    return trace;
}

Instance::~Instance() = default;

Value Instance::GetVariable(const std::string& name) const
{
    const int index = program_->FindVariable(name);
    return index >= 0 ? variables_[index] : Value{};
}

void Instance::Fail(const Problem& problem)
{
    failed_ = true;
    error_ = problem;
    std::string type;
    for (const auto& node : program_->nodes)
        if (node.id == problem.node) type = " (" + node.type + ")";
    std::cout << "[Script] " << source_ << ": node " << problem.node << type << ": " << problem.message
              << "; the script of this object is stopped until the next Play" << std::endl;
}

void Instance::Print(const std::string& text)
{
    std::cout << "[Script] " << source_ << ": " << text << std::endl;
    printed_.push_back(text);
    if (printed_.size() > 100) printed_.erase(printed_.begin());
}

void Instance::BeginPlay()
{
    if (started_ || failed_) return;
    started_ = true;
    const Event event;
    for (const int node : program_->beginPlay)
    {
        Run(node, event);
        if (failed_) return;
    }
}

void Instance::Tick(float deltaTime)
{
    if (!started_) return;
    ++frame_; // also after a failure: the trace shows how long ago the nodes ran
    if (failed_) return;
    elapsed_ += deltaTime;
    Event event;
    event.deltaTime = deltaTime;
    // Delays that are due continue first, in the order they started.
    if (!delays_.empty())
    {
        std::vector<Pending> due;
        for (auto it = delays_.begin(); it != delays_.end();)
            if (it->due <= elapsed_) { due.push_back(*it); it = delays_.erase(it); }
            else ++it;
        for (const auto& pending : due)
        {
            nodeFlag_[pending.node] = 0;
            Continue(program_->nodes[pending.node].next[0], event);
            if (failed_) return;
        }
    }
    // Key edges first: what was pressed this frame is handled before Tick.
    for (size_t i = 0; i < program_->keyEvents.size(); ++i)
    {
        const CompiledNode& node = program_->nodes[program_->keyEvents[i]];
        const bool down = KeyDown(node.key);
        const bool fire = node.op == Op::KeyPressed ? down && !keyWasDown_[i] : !down && keyWasDown_[i];
        keyWasDown_[i] = down;
        if (fire) Run(program_->keyEvents[i], event);
        if (failed_) return;
    }
    for (const int node : program_->every)
    {
        // At most once per frame: a long frame does not fire a burst.
        float& timer = nodeNumber_[node];
        timer += deltaTime;
        const float period = std::get<float>(program_->nodes[node].constant);
        if (timer < period) continue;
        timer = std::fmod(timer, period);
        Run(node, event);
        if (failed_) return;
    }
    for (const int node : program_->tick)
    {
        Run(node, event);
        if (failed_) return;
    }
}

void Instance::Continue(int node, const Event& event)
{
    if (node < 0) return;
    Run(node, event);
}

bool Instance::Trigger(const std::string& name)
{
    if (!started_ || failed_) return false;
    const Event event;
    const int steps = steps_; // a nested call does not reset the caller's step count
    bool ran = false;
    for (const int node : program_->customEvents)
    {
        if (program_->nodes[node].text != name) continue;
        ran = true;
        Run(node, event);
        if (failed_) break;
    }
    steps_ = steps;
    return ran;
}

bool Instance::KeyDown(int key) const
{
    Game* game = self_.GetGame();
    return game && key && game->GetInput().IsPhysicalKeyDown(static_cast<Keys>(key));
}

void Instance::Run(int node, const Event& event)
{
    steps_ = 0;
    try
    {
        Execute(node, event, 0);
    }
    catch (const ScriptFailure& failure)
    {
        Fail(failure.problem);
    }
    catch (const std::exception& error)
    {
        Fail({program_->nodes[node].id, std::string("unexpected error: ") + error.what()});
    }
}

void Instance::Step(int node, bool traced)
{
    if (traced)
    {
        ranAt_[node] = frame_ + 1;
        if (runs_[node] != UINT32_MAX) ++runs_[node];
    }
    if (++steps_ > StepLimit)
        Raise(program_->nodes[node].id, "more than " + std::to_string(StepLimit) + " steps in one event: probably an endless loop");
}

GameComponent* Instance::Resolve(const ObjectRef& object, int node) const
{
    // Self first: an object outside a scene has id 0.
    if (object.id == self_.GetComponentId()) return &self_;
    if (!object.id) Raise(program_->nodes[node].id, "no object (the Target input is empty or the object was not found)");
    Scene* scene = self_.GetScene();
    GameComponent* found = scene ? scene->FindComponent(object.id) : nullptr;
    if (!found) Raise(program_->nodes[node].id, "object #" + std::to_string(object.id) + " is no longer in the scene");
    return found;
}

Value Instance::Input(int node, int input, const Event& event, int depth)
{
    const InputSlot& slot = program_->nodes[node].inputs[input];
    if (slot.node >= 0)
    {
        Value value = Evaluate(slot.node, slot.output, event, depth + 1);
        auto& outputs = outputs_[slot.node];
        if (slot.output >= 0 && size_t(slot.output) < outputs.size()) outputs[slot.output] = value;
        return value;
    }
    if (slot.selfByDefault) return ObjectRef{self_.GetComponentId()};
    return slot.fallback;
}

void Instance::Execute(int index, const Event& event, int depth)
{
    while (index >= 0)
    {
        Step(index);
        const CompiledNode& node = program_->nodes[index];
        const int id = node.id;
        auto in = [&](int input) { return Input(index, input, event, depth); };
        auto vec = [&](int input) { return As<glm::vec3>(in(input), id); };
        switch (node.op)
        {
        case Op::Branch:
            index = node.next[As<bool>(in(0), id) ? 0 : 1];
            continue;
        case Op::Sequence:
            for (size_t i = 0; i + 1 < node.next.size(); ++i) Execute(node.next[i], event, depth + 1);
            index = node.next.back();
            continue;
        case Op::Print:
            Print(ToText(in(0)));
            break;
        case Op::SetPosition:
            Resolve(As<ObjectRef>(in(0), id), index)->SetPosition(vec(1));
            break;
        case Op::SetRotation:
            Resolve(As<ObjectRef>(in(0), id), index)->SetRotation(vec(1));
            break;
        case Op::SetScale:
            Resolve(As<ObjectRef>(in(0), id), index)->SetScale(vec(1));
            break;
        case Op::Translate:
        {
            GameComponent* target = Resolve(As<ObjectRef>(in(0), id), index);
            target->SetPosition(target->GetLocalPosition() + vec(1));
            break;
        }
        case Op::Rotate:
        {
            GameComponent* target = Resolve(As<ObjectRef>(in(0), id), index);
            target->SetRotation(target->GetRotation() + vec(1));
            break;
        }
        case Op::SetVariable:
            variables_[node.variable] = in(0);
            break;
        case Op::Delay:
            if (!nodeFlag_[index])
            {
                nodeFlag_[index] = 1;
                delays_.push_back({index, elapsed_ + std::max(0.0f, As<float>(in(0), id))});
            }
            return; // the chain goes on in a later frame
        case Op::DoOnce:
            if (nodeFlag_[index]) return;
            nodeFlag_[index] = 1;
            break;
        case Op::FlipFlop:
        {
            const bool a = nodeFlag_[index] == 0;
            nodeFlag_[index] = a ? 1 : 0;
            nodeNumber_[index] = a ? 1.0f : 0.0f;
            index = node.next[a ? 0 : 1];
            continue;
        }
        case Op::For:
        {
            const float first = std::floor(As<float>(in(0), id)), last = std::floor(As<float>(in(1), id));
            for (float value = first; value <= last; value += 1.0f)
            {
                Step(index, false); // an empty body still counts towards the step limit
                nodeNumber_[index] = value;
                Execute(node.next[0], event, depth + 1);
            }
            index = node.next[1];
            continue;
        }
        case Op::CallEvent:
        {
            GameComponent* target = Resolve(As<ObjectRef>(in(0), id), index);
            if (eventDepth >= MaxEventDepth)
                Raise(id, "events call each other too deeply (more than " + std::to_string(MaxEventDepth) + ")");
            if (Instance* script = target->GetScriptInstance())
            {
                ++eventDepth;
                try { script->Trigger(node.text); }
                catch (...) { --eventDepth; throw; }
                --eventDepth;
            }
            break;
        }
        case Op::Spawn:
        {
            GameComponent* source = Resolve(As<ObjectRef>(in(0), id), index);
            Game* game = self_.GetGame();
            std::string error;
            const uint64_t created = game ? game->SpawnCopy(*source, vec(1), error) : 0;
            if (!created) Raise(id, error.empty() ? std::string("cannot spawn a copy") : "cannot spawn a copy: " + error);
            nodeNumber_[index] = 0;
            spawned_[index] = created;
            break;
        }
        case Op::Destroy:
        {
            GameComponent* target = Resolve(As<ObjectRef>(in(0), id), index);
            if (Scene* scene = target->GetScene()) scene->RemoveComponent(target->GetComponentId());
            break;
        }
        default: // events start a chain; they have one exec output
            break;
        }
        index = node.next.empty() ? -1 : node.next[0];
    }
}

Value Instance::Evaluate(int index, int output, const Event& event, int depth)
{
    const CompiledNode& node = program_->nodes[index];
    // Reading an event's value (DeltaTime) is not a run of the event: the trace counts its runs only.
    const bool isEvent = node.op == Op::BeginPlay || node.op == Op::Tick || node.op == Op::KeyPressed || node.op == Op::KeyReleased ||
                         node.op == Op::Every || node.op == Op::CustomEvent;
    Step(index, !isEvent);
    const int id = node.id;
    auto in = [&](int input) { return Input(index, input, event, depth); };
    auto num = [&](int input) { return As<float>(in(input), id); };
    auto vec = [&](int input) { return As<glm::vec3>(in(input), id); };
    auto flag = [&](int input) { return As<bool>(in(input), id); };
    switch (node.op)
    {
    case Op::Tick: return event.deltaTime;
    case Op::Self: return ObjectRef{self_.GetComponentId()};
    case Op::FindObject:
    {
        Scene* scene = self_.GetScene();
        GameComponent* found = scene ? scene->FindComponentByName(As<std::string>(in(0), id)) : nullptr;
        return ObjectRef{found ? found->GetComponentId() : 0};
    }
    case Op::GetPosition: return Resolve(As<ObjectRef>(in(0), id), index)->GetLocalPosition();
    case Op::GetRotation: return Resolve(As<ObjectRef>(in(0), id), index)->GetRotation();
    case Op::GetScale: return Resolve(As<ObjectRef>(in(0), id), index)->GetScale();
    case Op::GetVariable: return variables_[node.variable];
    case Op::ConstFloat:
    case Op::ConstBool:
    case Op::ConstVec3:
    case Op::ConstString: return node.constant;
    case Op::MakeVec3: return glm::vec3(num(0), num(1), num(2));
    case Op::BreakVec3: return vec(0)[output];
    case Op::Vec3Add: return vec(0) + vec(1);
    case Op::Vec3Subtract: return vec(0) - vec(1);
    case Op::Vec3Scale: return vec(0) * num(1);
    case Op::Add: return num(0) + num(1);
    case Op::Subtract: return num(0) - num(1);
    case Op::Multiply: return num(0) * num(1);
    case Op::Divide:
    {
        const float a = num(0), b = num(1);
        if (b == 0.0f) Raise(id, "division by zero");
        return a / b;
    }
    case Op::Sin: return std::sin(num(0));
    case Op::Lerp: { const float a = num(0), b = num(1); return a + (b - a) * num(2); }
    case Op::Clamp:
    {
        const float value = num(0), low = num(1), high = num(2);
        return value < low ? low : value > high ? high : value;
    }
    case Op::Random:
    {
        const float low = num(0), high = num(1);
        if (!(low <= high)) Raise(id, "Min is greater than Max");
        return low == high ? low : std::uniform_real_distribution<float>(low, high)(random_);
    }
    case Op::Time: return elapsed_;
    case Op::Less: return num(0) < num(1);
    case Op::Greater: return num(0) > num(1);
    case Op::Equal: return std::abs(num(0) - num(1)) <= 1e-6f;
    case Op::And: return flag(0) && flag(1);
    case Op::Or: return flag(0) || flag(1);
    case Op::Not: return !flag(0);
    case Op::KeyDown: return KeyDown(node.key);
    case Op::FlipFlop: return nodeNumber_[index] != 0.0f;
    case Op::For: return nodeNumber_[index];
    case Op::Spawn: return ObjectRef{spawned_.count(index) ? spawned_.at(index) : 0};
    case Op::MousePosition:
    {
        Game* game = self_.GetGame();
        const auto cursor = game ? game->GetInput().GetCursorPosition() : Input::CursorPosition{};
        if (output == 0) return cursor.valid ? cursor.x : 0.0f;
        if (output == 1) return cursor.valid ? cursor.y : 0.0f;
        return cursor.valid;
    }
    case Op::Distance: return glm::length(vec(0) - vec(1));
    case Op::Vec3Length: return glm::length(vec(0));
    case Op::Vec3Normalize:
    {
        const glm::vec3 v = vec(0);
        const float length = glm::length(v);
        return length > 0.0f ? v / length : glm::vec3(0.0f);
    }
    case Op::Vec3Lerp: { const glm::vec3 a = vec(0), b = vec(1); return a + (b - a) * num(2); }
    case Op::Abs: return std::abs(num(0));
    case Op::Cos: return std::cos(num(0));
    case Op::Min: return std::min(num(0), num(1));
    case Op::Max: return std::max(num(0), num(1));
    default:
        Raise(id, "internal: node " + node.type + " has no value");
    }
}
} // namespace Script
