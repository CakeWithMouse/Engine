#include "ScriptProgram.h"

#include <Engine/Components/GameComponents.h>
#include <Engine/InputDevice/Keys.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Scene/Scene.h>

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
    keyWasDown_.assign(program_->keyEvents.size(), false);
    ranAt_.assign(program_->nodes.size(), 0);
    runs_.assign(program_->nodes.size(), 0);
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
    for (const int node : program_->tick)
    {
        Run(node, event);
        if (failed_) return;
    }
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
    if (slot.node >= 0) return Evaluate(slot.node, slot.output, event, depth + 1);
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
    const bool isEvent = node.op == Op::BeginPlay || node.op == Op::Tick || node.op == Op::KeyPressed || node.op == Op::KeyReleased;
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
    default:
        Raise(id, "internal: node " + node.type + " has no value");
    }
}
} // namespace Script
