#pragma once
#include <Engine/AI/AiTypes.h>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

// Hierarchical state machine modelled on Unreal StateTree.
// A StateMachine is immutable and shared by every unit of one type; per-unit data lives in the
// controller: the active chain of states, memory and the Data block of each running task.
namespace Ai
{
class Context;

/** A yes/no decision: may a state be entered, may a transition fire. */
using ConditionFn = std::function<bool(Context&)>;
ConditionFn Not(ConditionFn condition);
namespace Detail
{
ConditionFn AllOf(std::vector<ConditionFn> conditions);
ConditionFn AnyOf(std::vector<ConditionFn> conditions);
}
template<class... T> ConditionFn All(T&&... conditions) { return Detail::AllOf({ConditionFn(std::forward<T>(conditions))...}); }
template<class... T> ConditionFn Any(T&&... conditions) { return Detail::AnyOf({ConditionFn(std::forward<T>(conditions))...}); }

struct NoData {};

/** Type-erased part of a task; derive from Ai::Task<Data>, not from this class. */
class TaskBase
{
public:
    virtual ~TaskBase() = default;
    /** Shown in the debugger and the history. Defaults to the C++ type name. */
    virtual std::string Name() const;
    // Engine side: the controller owns Data, the task only interprets it.
    virtual size_t DataSize() const = 0;
    virtual size_t DataAlign() const = 0;
    virtual void ConstructData(void* data) const = 0;
    virtual void DestroyData(void* data) const = 0;
    virtual Status RunEnter(Context& ctx, void* data) const = 0;
    virtual Status RunTick(Context& ctx, void* data, float dt) const = 0;
    virtual void RunExit(Context& ctx, void* data, Status result) const = 0;
};

/**
 * What a unit does while a state is active. The object is shared by every unit running the
 * machine, so the methods are const: per-unit values belong in Data or in memory.
 */
template<class Data = NoData>
class Task : public TaskBase
{
    static_assert(std::is_default_constructible_v<Data>, "Task Data must be default constructible");
    static_assert(alignof(Data) <= alignof(std::max_align_t), "Task Data alignment is too large");
public:
    using DataType = Data;
    /** On entering the state. Running continues; Succeeded or Failed completes at once. */
    virtual Status Enter(Context&, Data&) const { return Status::Running; }
    /** Every think while the task is Running. dt is the time since the previous think. */
    virtual Status Tick(Context&, Data&, float) const { return Status::Running; }
    /** Always called. result is Running when the state was left before the task finished. */
    virtual void Exit(Context&, Data&, Status) const {}

    size_t DataSize() const final { return sizeof(Data); }
    size_t DataAlign() const final { return alignof(Data); }
    void ConstructData(void* data) const final { new (data) Data(); }
    void DestroyData(void* data) const final { static_cast<Data*>(data)->~Data(); }
    Status RunEnter(Context& ctx, void* data) const final { return Enter(ctx, *static_cast<Data*>(data)); }
    Status RunTick(Context& ctx, void* data, float dt) const final { return Tick(ctx, *static_cast<Data*>(data), dt); }
    void RunExit(Context& ctx, void* data, Status result) const final { Exit(ctx, *static_cast<Data*>(data), result); }
};

class ServiceBase
{
public:
    virtual ~ServiceBase() = default;
    virtual std::string Name() const;
    virtual size_t DataSize() const = 0;
    virtual size_t DataAlign() const = 0;
    virtual void ConstructData(void* data) const = 0;
    virtual void DestroyData(void* data) const = 0;
    virtual void RunEnter(Context& ctx, void* data) const = 0;
    virtual void RunTick(Context& ctx, void* data, float dt) const = 0;
    virtual void RunExit(Context& ctx, void* data) const = 0;
};

/** Works in the background with an interval and updates memory: BT Service, StateTree Global Task. */
template<class Data = NoData>
class Service : public ServiceBase
{
    static_assert(std::is_default_constructible_v<Data>, "Service Data must be default constructible");
    static_assert(alignof(Data) <= alignof(std::max_align_t), "Service Data alignment is too large");
public:
    using DataType = Data;
    virtual void Enter(Context&, Data&) const {}
    /** dt is the time since the previous call of this service. */
    virtual void Tick(Context&, Data&, float dt) const = 0;
    virtual void Exit(Context&, Data&) const {}

    size_t DataSize() const final { return sizeof(Data); }
    size_t DataAlign() const final { return alignof(Data); }
    void ConstructData(void* data) const final { new (data) Data(); }
    void DestroyData(void* data) const final { static_cast<Data*>(data)->~Data(); }
    void RunEnter(Context& ctx, void* data) const final { Enter(ctx, *static_cast<Data*>(data)); }
    void RunTick(Context& ctx, void* data, float dt) const final { Tick(ctx, *static_cast<Data*>(data), dt); }
    void RunExit(Context& ctx, void* data) const final { Exit(ctx, *static_cast<Data*>(data)); }
};

/** Special transition targets: the next sibling, or stop the machine. */
enum class Target { Next, Finish };

class StateMachine;
using MachinePtr = std::shared_ptr<const StateMachine>;

// Built definition. Public for the runtime, the debugger and the .fsm loader; game code uses builders.
namespace Detail
{
struct TransitionDef
{
    enum class Trigger { Tick, Succeeded, Failed, Completed, Event };
    enum class Kind { State, Finish, CompleteHost };
    Trigger trigger = Trigger::Tick;
    std::string event;
    std::vector<ConditionFn> conditions;
    float delay = 0;
    Kind kind = Kind::State;
    int target = -1;          // state index for Kind::State, host state for Kind::CompleteHost
    std::string targetName;   // as written, for messages
};
struct TaskSlot { std::shared_ptr<const TaskBase> task; size_t offset = 0; };
struct ServiceSlot { std::shared_ptr<const ServiceBase> service; float interval = 0; size_t offset = 0; };
struct StateDef
{
    std::string name, path;
    int parent = -1, depth = 0;
    bool linked = false;       // copied from a linked machine
    std::vector<int> children;
    std::vector<ConditionFn> enterConditions;
    std::vector<TaskSlot> tasks;
    std::vector<ServiceSlot> services;
    std::vector<TransitionDef> transitions;
};
}

class StateMachine
{
public:
    const std::string& Name() const { return name_; }
    size_t StateCount() const { return states_.size(); }
    const Detail::StateDef& State(int index) const { return states_.at(static_cast<size_t>(index)); }
    /** Index of a state by its path ("Combat/Chase"), or by a unique name; -1 when missing. */
    int Find(std::string_view nameOrPath) const;
    const std::vector<int>& RootStates() const { return roots_; }
    const std::vector<Detail::ServiceSlot>& GlobalServices() const { return services_; }
    /** Bytes of Data for every task and service of the machine; one block per controller. */
    size_t DataSize() const { return dataSize_; }

private:
    friend class MachineBuilder;
    std::string name_;
    std::vector<Detail::StateDef> states_;
    std::vector<int> roots_;
    std::vector<Detail::ServiceSlot> services_;
    size_t dataSize_ = 0;
};

class StateBuilder;

class TransitionBuilder
{
public:
    TransitionBuilder& OnSucceeded();
    TransitionBuilder& OnFailed();
    TransitionBuilder& OnCompleted();
    TransitionBuilder& OnEvent(Event event);
    /** Checked every think; combined with On... it is an extra condition. */
    TransitionBuilder& If(ConditionFn condition);
    /** The transition fires this many seconds after its trigger, if the state is still active. */
    TransitionBuilder& After(float seconds);

private:
    friend class StateBuilder;
    friend class MachineBuilder;
    Detail::TransitionDef def_;
    bool special_ = false;
    Target specialTarget_ = Target::Next;
};

class StateBuilder
{
public:
    /** A child state. The parent is never active alone: it enters its first allowed child. */
    StateBuilder& State(std::string name);
    template<class T, class... Args> StateBuilder& Task(Args&&... args)
    {
        static_assert(std::is_base_of_v<TaskBase, T>, "Task<T>: T must derive from Ai::Task");
        static_assert(std::is_constructible_v<T, Args...>, "Task<T>: no matching constructor");
        tasks_.push_back(std::make_shared<T>(std::forward<Args>(args)...));
        return *this;
    }
    template<class T, class... Args> StateBuilder& Service(float interval, Args&&... args)
    {
        static_assert(std::is_base_of_v<ServiceBase, T>, "Service<T>: T must derive from Ai::Service");
        static_assert(std::is_constructible_v<T, Args...>, "Service<T>: no matching constructor");
        services_.push_back({std::make_shared<T>(std::forward<Args>(args)...), interval, 0});
        return *this;
    }
    /** Add an already created task object, for example one built from a .fsm file. */
    StateBuilder& AddTask(std::shared_ptr<const TaskBase> task);
    StateBuilder& AddService(std::shared_ptr<const ServiceBase> service, float interval);
    /** The state may be entered only when every condition holds. */
    StateBuilder& EnterIf(ConditionFn condition);
    /** Insert another machine as the children of this state. Finish inside it completes this state. */
    StateBuilder& Linked(MachinePtr machine);
    TransitionBuilder& GoTo(std::string state);
    TransitionBuilder& GoTo(Target target);
    const std::string& Name() const { return name_; }

private:
    friend class MachineBuilder;
    StateBuilder(class MachineBuilder& owner, std::string name, StateBuilder* parent);
    MachineBuilder* owner_;
    StateBuilder* parent_;
    std::string name_;
    std::vector<StateBuilder*> children_;
    std::vector<std::shared_ptr<const TaskBase>> tasks_;
    std::vector<Detail::ServiceSlot> services_;
    std::vector<ConditionFn> enter_;
    std::deque<TransitionBuilder> transitions_;
    MachinePtr linked_;
};

class MachineBuilder
{
public:
    explicit MachineBuilder(std::string name);
    MachineBuilder(const MachineBuilder&) = delete;
    MachineBuilder& operator=(const MachineBuilder&) = delete;
    /** A top-level state. The first one that may be entered starts the machine. */
    StateBuilder& State(std::string name);
    template<class T, class... Args> MachineBuilder& Service(float interval, Args&&... args)
    {
        static_assert(std::is_base_of_v<ServiceBase, T>, "Service<T>: T must derive from Ai::Service");
        static_assert(std::is_constructible_v<T, Args...>, "Service<T>: no matching constructor");
        services_.push_back({std::make_shared<T>(std::forward<Args>(args)...), interval, 0});
        return *this;
    }
    MachineBuilder& AddService(std::shared_ptr<const ServiceBase> service, float interval);
    /** Checks names and targets; throws std::invalid_argument with a readable message. */
    MachinePtr Build();

private:
    friend class StateBuilder;
    StateBuilder& Create(std::string name, StateBuilder* parent);
    std::string name_;
    std::vector<std::unique_ptr<StateBuilder>> states_;
    std::vector<StateBuilder*> roots_;
    std::vector<Detail::ServiceSlot> services_;
};

// Machines by name. A machine is built in C++ and given a name; a scene object then picks it by
// that name (AiCharacter's "Автомат" property), and no subclass is needed for it.
using MachineFactory = std::function<MachinePtr()>;
/** Registers or replaces a named machine. The factory runs once, on the first FindMachine. */
void RegisterMachine(std::string name, MachineFactory factory);
/** The machine with this name, built on first use; null when no such name is registered. */
MachinePtr FindMachine(std::string_view name);
/** Like FindMachine, but throws std::invalid_argument that lists the registered names. */
MachinePtr GetMachine(std::string_view name);
/** Registered names, sorted. */
std::vector<std::string> MachineNames();

/** For ENGINE_AI_MACHINE: registers while the program (or the game module) starts. */
struct MachineRegistration
{
    MachineRegistration(const char* name, MachineFactory factory) { RegisterMachine(name, std::move(factory)); }
};
} // namespace Ai

#define ENGINE_AI_DETAIL_JOIN2(a, b) a##b
#define ENGINE_AI_DETAIL_JOIN(a, b) ENGINE_AI_DETAIL_JOIN2(a, b)
/**
 * ENGINE_AI_MACHINE("Guard", BuildGuard) in a .cpp of the game project registers the machine that
 * Ai::MachinePtr BuildGuard() builds. Not for static libraries: the linker may drop the file.
 */
#define ENGINE_AI_MACHINE(Name, Factory) \
    static const ::Ai::MachineRegistration ENGINE_AI_DETAIL_JOIN(engineAiMachine_, __LINE__){Name, Factory};
