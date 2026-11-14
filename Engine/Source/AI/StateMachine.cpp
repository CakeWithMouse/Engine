#include <Engine/AI/StateMachine.h>
#include <algorithm>
#include <map>
#include <stdexcept>

namespace Ai
{
namespace
{
std::string CleanTypeName(const char* raw)
{
    std::string name = raw ? raw : "";
    for (const char* prefix : {"class ", "struct "})
        for (size_t at; (at = name.find(prefix)) != std::string::npos;) name.erase(at, std::char_traits<char>::length(prefix));
    return name;
}

size_t Allocate(size_t& offset, size_t size, size_t align)
{
    if (align == 0) align = 1;
    offset = (offset + align - 1) / align * align;
    const size_t at = offset;
    offset += size ? size : 1;
    return at;
}

void RequireCondition(const ConditionFn& condition, const std::string& where)
{
    if (!condition) throw std::invalid_argument(where + ": empty condition");
}
} // namespace

std::string TaskBase::Name() const { return CleanTypeName(typeid(*this).name()); }
std::string ServiceBase::Name() const { return CleanTypeName(typeid(*this).name()); }

ConditionFn Not(ConditionFn condition)
{
    RequireCondition(condition, "Ai::Not");
    return [condition = std::move(condition)](Context& ctx) { return !condition(ctx); };
}

namespace Detail
{
ConditionFn AllOf(std::vector<ConditionFn> conditions)
{
    for (const auto& condition : conditions) RequireCondition(condition, "Ai::All");
    return [conditions = std::move(conditions)](Context& ctx) {
        for (const auto& condition : conditions) if (!condition(ctx)) return false;
        return true;
    };
}
ConditionFn AnyOf(std::vector<ConditionFn> conditions)
{
    for (const auto& condition : conditions) RequireCondition(condition, "Ai::Any");
    return [conditions = std::move(conditions)](Context& ctx) {
        for (const auto& condition : conditions) if (condition(ctx)) return true;
        return false;
    };
}
} // namespace Detail

int StateMachine::Find(std::string_view nameOrPath) const
{
    const bool path = nameOrPath.find('/') != std::string_view::npos;
    int found = -1;
    for (size_t i = 0; i < states_.size(); ++i)
    {
        const auto& state = states_[i];
        if (path ? state.path == nameOrPath : (!state.linked && state.name == nameOrPath))
        {
            if (found >= 0) return -1;
            found = static_cast<int>(i);
        }
    }
    if (found < 0 && !path) // a unique name inside a linked machine still works
        for (size_t i = 0; i < states_.size(); ++i)
            if (states_[i].name == nameOrPath) { if (found >= 0) return -1; found = static_cast<int>(i); }
    return found;
}

// Transition builder
TransitionBuilder& TransitionBuilder::OnSucceeded() { def_.trigger = Detail::TransitionDef::Trigger::Succeeded; return *this; }
TransitionBuilder& TransitionBuilder::OnFailed() { def_.trigger = Detail::TransitionDef::Trigger::Failed; return *this; }
TransitionBuilder& TransitionBuilder::OnCompleted() { def_.trigger = Detail::TransitionDef::Trigger::Completed; return *this; }
TransitionBuilder& TransitionBuilder::OnEvent(Event event)
{
    if (!event.name || !*event.name) throw std::invalid_argument("OnEvent: the event has no name");
    def_.trigger = Detail::TransitionDef::Trigger::Event;
    def_.event = event.name;
    return *this;
}
TransitionBuilder& TransitionBuilder::If(ConditionFn condition)
{
    RequireCondition(condition, "If");
    def_.conditions.push_back(std::move(condition));
    return *this;
}
TransitionBuilder& TransitionBuilder::After(float seconds)
{
    if (!(seconds >= 0)) throw std::invalid_argument("After: the delay must be zero or more");
    def_.delay = seconds;
    return *this;
}

// State builder
StateBuilder::StateBuilder(MachineBuilder& owner, std::string name, StateBuilder* parent)
    : owner_(&owner), parent_(parent), name_(std::move(name)) {}
StateBuilder& StateBuilder::State(std::string name) { return owner_->Create(std::move(name), this); }
StateBuilder& StateBuilder::AddTask(std::shared_ptr<const TaskBase> task)
{
    if (!task) throw std::invalid_argument(name_ + ": null task");
    tasks_.push_back(std::move(task));
    return *this;
}
StateBuilder& StateBuilder::AddService(std::shared_ptr<const ServiceBase> service, float interval)
{
    if (!service) throw std::invalid_argument(name_ + ": null service");
    services_.push_back({std::move(service), interval, 0});
    return *this;
}
StateBuilder& StateBuilder::EnterIf(ConditionFn condition)
{
    RequireCondition(condition, name_ + ": EnterIf");
    enter_.push_back(std::move(condition));
    return *this;
}
StateBuilder& StateBuilder::Linked(MachinePtr machine)
{
    if (!machine) throw std::invalid_argument(name_ + ": Linked needs a machine");
    linked_ = std::move(machine);
    return *this;
}
TransitionBuilder& StateBuilder::GoTo(std::string state)
{
    transitions_.emplace_back();
    transitions_.back().def_.targetName = std::move(state);
    return transitions_.back();
}
TransitionBuilder& StateBuilder::GoTo(Target target)
{
    transitions_.emplace_back();
    transitions_.back().special_ = true;
    transitions_.back().specialTarget_ = target;
    transitions_.back().def_.targetName = target == Target::Next ? "Next" : "Finish";
    return transitions_.back();
}

// Machine builder
MachineBuilder::MachineBuilder(std::string name) : name_(std::move(name)) {}
StateBuilder& MachineBuilder::State(std::string name) { return Create(std::move(name), nullptr); }
MachineBuilder& MachineBuilder::AddService(std::shared_ptr<const ServiceBase> service, float interval)
{
    if (!service) throw std::invalid_argument(name_ + ": null service");
    services_.push_back({std::move(service), interval, 0});
    return *this;
}
StateBuilder& MachineBuilder::Create(std::string name, StateBuilder* parent)
{
    states_.push_back(std::unique_ptr<StateBuilder>(new StateBuilder(*this, std::move(name), parent)));
    StateBuilder& state = *states_.back();
    (parent ? parent->children_ : roots_).push_back(&state);
    return state;
}

MachinePtr MachineBuilder::Build()
{
    const std::string where = "State machine \"" + name_ + "\"";
    if (roots_.empty()) throw std::invalid_argument(where + ": no states");
    auto machine = std::make_shared<StateMachine>();
    machine->name_ = name_;
    auto& states = machine->states_;
    size_t offset = 0;
    std::map<std::string, int> byName;
    std::map<const StateBuilder*, int> indexOf;

    auto addState = [&](auto&& self, const StateBuilder& builder, int parent) -> void {
        if (builder.name_.empty()) throw std::invalid_argument(where + ": a state has no name");
        if (builder.name_.find('/') != std::string::npos) throw std::invalid_argument(where + ": state \"" + builder.name_ + "\" contains '/'");
        if (!byName.emplace(builder.name_, static_cast<int>(states.size())).second)
            throw std::invalid_argument(where + ": two states are named \"" + builder.name_ + "\"");
        const int index = static_cast<int>(states.size());
        indexOf[&builder] = index;
        Detail::StateDef def;
        def.name = builder.name_;
        def.parent = parent;
        def.enterConditions = builder.enter_;
        for (const auto& task : builder.tasks_)
            def.tasks.push_back({task, Allocate(offset, task->DataSize(), task->DataAlign())});
        for (auto service : builder.services_)
        {
            if (!(service.interval >= 0)) throw std::invalid_argument(where + ": negative service interval in \"" + builder.name_ + "\"");
            service.offset = Allocate(offset, service.service->DataSize(), service.service->DataAlign());
            def.services.push_back(service);
        }
        states.push_back(std::move(def));
        if (parent >= 0) states[static_cast<size_t>(parent)].children.push_back(index);
        else machine->roots_.push_back(index);

        if (builder.linked_)
        {
            if (!builder.children_.empty())
                throw std::invalid_argument(where + ": \"" + builder.name_ + "\" is Linked and cannot have its own child states");
            const StateMachine& linked = *builder.linked_;
            const int base = static_cast<int>(states.size());
            for (const auto& source : linked.states_)
            {
                Detail::StateDef copy = source;
                copy.linked = true;
                copy.parent = source.parent < 0 ? index : base + source.parent;
                for (auto& child : copy.children) child += base;
                for (auto& slot : copy.tasks) slot.offset = Allocate(offset, slot.task->DataSize(), slot.task->DataAlign());
                for (auto& slot : copy.services) slot.offset = Allocate(offset, slot.service->DataSize(), slot.service->DataAlign());
                for (auto& transition : copy.transitions)
                {
                    if (transition.kind == Detail::TransitionDef::Kind::State || transition.kind == Detail::TransitionDef::Kind::CompleteHost)
                        transition.target += base;
                    if (transition.kind == Detail::TransitionDef::Kind::Finish)
                    {
                        transition.kind = Detail::TransitionDef::Kind::CompleteHost;
                        transition.target = index;
                    }
                }
                states.push_back(std::move(copy));
            }
            for (const int root : linked.roots_) states[static_cast<size_t>(index)].children.push_back(base + root);
            for (auto service : linked.services_)
            {
                service.offset = Allocate(offset, service.service->DataSize(), service.service->DataAlign());
                states[static_cast<size_t>(index)].services.push_back(service);
            }
        }
        for (const StateBuilder* child : builder.children_) self(self, *child, index);
    };
    for (const StateBuilder* root : roots_) addState(addState, *root, -1);

    // Transitions are resolved once every name is known.
    for (const auto& builder : states_)
    {
        const int index = indexOf.at(builder.get());
        for (const auto& transition : builder->transitions_)
        {
            Detail::TransitionDef def = transition.def_;
            const std::string from = where + ": transition from \"" + builder->name_ + "\"";
            if (transition.special_ && transition.specialTarget_ == Target::Finish) def.kind = Detail::TransitionDef::Kind::Finish;
            else if (transition.special_)
            {
                const int parent = states[static_cast<size_t>(index)].parent;
                const auto& siblings = parent < 0 ? machine->roots_ : states[static_cast<size_t>(parent)].children;
                const auto at = std::find(siblings.begin(), siblings.end(), index);
                if (at == siblings.end() || at + 1 == siblings.end()) throw std::invalid_argument(from + " to Next: it is the last state of its parent");
                def.target = *(at + 1);
            }
            else
            {
                const auto target = byName.find(def.targetName);
                if (target == byName.end()) throw std::invalid_argument(from + " to unknown state \"" + def.targetName + "\"");
                def.target = target->second;
            }
            states[static_cast<size_t>(index)].transitions.push_back(std::move(def));
        }
    }
    for (auto service : services_)
    {
        if (!(service.interval >= 0)) throw std::invalid_argument(where + ": negative service interval");
        service.offset = Allocate(offset, service.service->DataSize(), service.service->DataAlign());
        machine->services_.push_back(service);
    }
    // Parents always precede their children, so one pass gives paths and depths.
    for (auto& state : states)
    {
        if (state.parent < 0) { state.path = state.name; state.depth = 0; }
        else { const auto& parent = states[static_cast<size_t>(state.parent)]; state.path = parent.path + "/" + state.name; state.depth = parent.depth + 1; }
    }
    machine->dataSize_ = offset;
    return machine;
}
} // namespace Ai

namespace Ai
{
namespace
{
struct NamedMachine
{
    MachineFactory factory;
    MachinePtr built;
};
// A function-local static: registrations from other files' static objects may run before main.
std::map<std::string, NamedMachine, std::less<>>& NamedMachines()
{
    static std::map<std::string, NamedMachine, std::less<>> machines;
    return machines;
}
} // namespace

void RegisterMachine(std::string name, MachineFactory factory)
{
    if (name.empty()) throw std::invalid_argument("Ai::RegisterMachine: empty name");
    if (!factory) throw std::invalid_argument("Ai::RegisterMachine: no factory for \"" + name + "\"");
    NamedMachines()[std::move(name)] = {std::move(factory), nullptr};
}

MachinePtr FindMachine(std::string_view name)
{
    auto& machines = NamedMachines();
    const auto found = machines.find(name);
    if (found == machines.end()) return nullptr;
    if (!found->second.built)
    {
        MachinePtr built = found->second.factory();
        if (!built) throw std::invalid_argument("Ai::FindMachine: the factory of \"" + std::string(name) + "\" returned no machine");
        found->second.built = std::move(built);
    }
    return found->second.built;
}

MachinePtr GetMachine(std::string_view name)
{
    if (MachinePtr machine = FindMachine(name)) return machine;
    std::string known;
    for (const std::string& registered : MachineNames()) known += (known.empty() ? "" : ", ") + registered;
    throw std::invalid_argument("No state machine named \"" + std::string(name) + "\". Registered: " + (known.empty() ? "none" : known) +
                                ". Register one with Ai::RegisterMachine or ENGINE_AI_MACHINE.");
}

std::vector<std::string> MachineNames()
{
    std::vector<std::string> names;
    for (const auto& entry : NamedMachines()) names.push_back(entry.first);
    return names;
}
} // namespace Ai
