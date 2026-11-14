#include <Engine/AI/Controller.h>
#include <Engine/AI/World.h>
#include <algorithm>
#include <cstdio>
#include <exception>
#include <stdexcept>

namespace Ai::Detail
{
namespace
{
std::string Stamp(float time)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%6.2f  ", time);
    return text;
}
} // namespace

MachineInstance::~MachineInstance()
{
    // Normally Stop already ran Exit. Here only the memory is released.
    if (!machine_) return;
    for (auto& active : active_)
    {
        const auto& def = machine_->State(active.index);
        for (const auto& slot : def.tasks) slot.task->DestroyData(DataAt(slot.offset));
        for (const auto& slot : def.services) slot.service->DestroyData(DataAt(slot.offset));
    }
    if (globalEntered_)
        for (const auto& slot : machine_->GlobalServices()) slot.service->DestroyData(DataAt(slot.offset));
}

void MachineInstance::Start(MachinePtr machine)
{
    if (!machine) throw std::invalid_argument("Run: null state machine");
    if (running_ || !active_.empty() || globalEntered_) throw std::logic_error("Run: the previous machine is still active");
    machine_ = std::move(machine);
    data_.assign((machine_->DataSize() + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t), std::max_align_t{});
    globalServiceTime_.assign(machine_->GlobalServices().size(), 0.0f);
    delayed_.clear();
    running_ = true;
    started_ = false;
    wake_ = true;
}

void MachineInstance::Stop(Context& ctx)
{
    if (!machine_) return;
    std::exception_ptr failure;
    try { ExitTo(ctx, 0); } catch (...) { failure = std::current_exception(); }
    if (globalEntered_)
    {
        const auto& services = machine_->GlobalServices();
        for (size_t i = services.size(); i-- > 0;)
        {
            try { services[i].service->RunExit(ctx, DataAt(services[i].offset)); }
            catch (...) { if (!failure) failure = std::current_exception(); }
            services[i].service->DestroyData(DataAt(services[i].offset));
        }
        globalEntered_ = false;
    }
    running_ = false;
    delayed_.clear();
    if (failure) std::rethrow_exception(failure);
}

void MachineInstance::SetDepthContext(Context& ctx, size_t depth) const
{
    ctx.stateEnteredAt_ = depth < active_.size() ? active_[depth].enteredAt : ctx.Time();
}

bool MachineInstance::PassesEnter(int state, Context& ctx) const
{
    ctx.stateEnteredAt_ = ctx.Time();
    for (const auto& condition : machine_->State(state).enterConditions)
        if (!condition(ctx)) return false;
    return true;
}

bool MachineInstance::SelectChildren(int state, std::vector<int>& path, Context& ctx) const
{
    for (const int child : machine_->State(state).children)
    {
        if (!PassesEnter(child, ctx)) continue;
        path.push_back(child);
        if (machine_->State(child).children.empty() || SelectChildren(child, path, ctx)) return true;
        path.pop_back();
    }
    return false;
}

void MachineInstance::Enter(Context& ctx, int state)
{
    const auto& def = machine_->State(state);
    Active active;
    active.index = state;
    active.enteredAt = ctx.Time();
    active.activation = ++activations_;
    active.tasks.assign(def.tasks.size(), Status::Running);
    active.serviceTime.assign(def.services.size(), 0.0f);
    // Data first: Exit may run for every slot later, even if an Enter below throws.
    for (const auto& slot : def.services) slot.service->ConstructData(DataAt(slot.offset));
    for (const auto& slot : def.tasks) slot.task->ConstructData(DataAt(slot.offset));
    active_.push_back(std::move(active));
    const size_t depth = active_.size() - 1;
    SetDepthContext(ctx, depth);
    for (const auto& slot : def.services) slot.service->RunEnter(ctx, DataAt(slot.offset));
    for (size_t i = 0; i < def.tasks.size(); ++i)
    {
        const Status status = def.tasks[i].task->RunEnter(ctx, DataAt(def.tasks[i].offset));
        if (status == Status::Running) continue;
        auto& current = active_[depth];
        current.tasks[i] = status;
        if (current.completed == Status::Running || status == Status::Failed) current.completed = status;
        wake_ = true;
    }
}

void MachineInstance::ExitTo(Context& ctx, size_t keep)
{
    std::exception_ptr failure;
    while (active_.size() > keep)
    {
        SetDepthContext(ctx, active_.size() - 1);
        Active active = std::move(active_.back());
        active_.pop_back();
        const auto& def = machine_->State(active.index);
        for (size_t i = def.tasks.size(); i-- > 0;)
        {
            try { def.tasks[i].task->RunExit(ctx, DataAt(def.tasks[i].offset), active.tasks[i]); }
            catch (...) { if (!failure) failure = std::current_exception(); }
            def.tasks[i].task->DestroyData(DataAt(def.tasks[i].offset));
        }
        for (size_t i = def.services.size(); i-- > 0;)
        {
            try { def.services[i].service->RunExit(ctx, DataAt(def.services[i].offset)); }
            catch (...) { if (!failure) failure = std::current_exception(); }
            def.services[i].service->DestroyData(DataAt(def.services[i].offset));
        }
    }
    delayed_.erase(std::remove_if(delayed_.begin(), delayed_.end(),
        [&](const Delayed& d) { return static_cast<size_t>(d.depth) >= keep; }), delayed_.end());
    if (failure) std::rethrow_exception(failure);
}

std::string MachineInstance::PathOf(const std::vector<int>& chain) const
{
    std::string path;
    for (const int state : chain) { if (!path.empty()) path += '/'; path += machine_->State(state).name; }
    return path;
}

std::string MachineInstance::ActivePath() const
{
    if (!machine_) return {};
    std::vector<int> chain;
    for (const auto& active : active_) chain.push_back(active.index);
    return PathOf(chain);
}

std::vector<int> MachineInstance::ActiveStates() const
{
    std::vector<int> chain;
    for (const auto& active : active_) chain.push_back(active.index);
    return chain;
}

std::vector<MachineInstance::TaskView> MachineInstance::ActiveTasks() const
{
    std::vector<TaskView> tasks;
    for (const auto& active : active_)
    {
        const auto& def = machine_->State(active.index);
        for (size_t i = 0; i < def.tasks.size(); ++i) tasks.push_back({def.name, def.tasks[i].task->Name(), active.tasks[i]});
    }
    return tasks;
}

bool MachineInstance::SelectInitial(Context& ctx, const char* reason)
{
    std::vector<int> path;
    for (const int root : machine_->RootStates())
    {
        if (!PassesEnter(root, ctx)) continue;
        path.assign(1, root);
        if (machine_->State(root).children.empty() || SelectChildren(root, path, ctx)) break;
        path.clear();
    }
    if (path.empty()) return false;
    for (const int state : path) Enter(ctx, state);
    ctx.Self().Log(Stamp(ctx.Time()) + "-> " + ActivePath() + "   " + reason);
    return true;
}

bool MachineInstance::Transition(Context& ctx, int fromDepth, const TransitionDef& transition, const char* reason)
{
    (void)fromDepth;
    if (transition.kind == TransitionDef::Kind::Finish)
    {
        const std::string old = ActivePath();
        Stop(ctx);
        ctx.Self().Log(Stamp(ctx.Time()) + old + " -> (finished)   " + reason);
        return true;
    }
    std::vector<int> chain;
    for (int state = transition.target; state >= 0; state = machine_->State(state).parent) chain.push_back(state);
    std::reverse(chain.begin(), chain.end());
    size_t keep = 0;
    while (keep < active_.size() && keep < chain.size() && active_[keep].index == chain[keep]) ++keep;
    if (keep == chain.size()) keep = chain.size() - 1; // the target is active: enter it again
    for (size_t i = keep; i < chain.size(); ++i)
        if (!PassesEnter(chain[i], ctx)) return false;
    std::vector<int> path = chain;
    if (!machine_->State(transition.target).children.empty() && !SelectChildren(transition.target, path, ctx)) return false;
    const std::string old = ActivePath();
    ExitTo(ctx, keep);
    for (size_t i = keep; i < path.size(); ++i) Enter(ctx, path[i]);
    ctx.Self().Log(Stamp(ctx.Time()) + old + " -> " + ActivePath() + "   " + reason);
    return true;
}

bool MachineInstance::Restore(Context& ctx, const std::string& path)
{
    if (!machine_) return false;
    const int target = machine_->Find(path);
    if (target < 0) return false;
    std::vector<int> chain;
    for (int state = target; state >= 0; state = machine_->State(state).parent) chain.push_back(state);
    std::reverse(chain.begin(), chain.end());
    if (!machine_->State(target).children.empty() && !SelectChildren(target, chain, ctx)) return false;
    ExitTo(ctx, 0);
    for (const int state : chain) Enter(ctx, state);
    started_ = true;
    ctx.Self().Log(Stamp(ctx.Time()) + "-> " + ActivePath() + "   restored from a save");
    return true;
}

void MachineInstance::Think(Context& ctx, float dt, const std::vector<std::string>& events)
{
    if (!running_) return;
    wake_ = false;
    if (!globalEntered_)
    {
        for (const auto& slot : machine_->GlobalServices()) slot.service->ConstructData(DataAt(slot.offset));
        globalEntered_ = true;
        for (const auto& slot : machine_->GlobalServices()) slot.service->RunEnter(ctx, DataAt(slot.offset));
    }
    if (!started_)
    {
        started_ = true;
        if (!SelectInitial(ctx, "start")) wake_ = false;
        return;
    }
    if (active_.empty())
    {
        SelectInitial(ctx, "no state could be entered before");
        return;
    }

    // Services, then tasks, from the root to the leaf.
    const auto& globals = machine_->GlobalServices();
    for (size_t i = 0; i < globals.size(); ++i)
    {
        globalServiceTime_[i] += dt;
        if (globalServiceTime_[i] + 1e-6f < globals[i].interval) continue;
        const float elapsed = globalServiceTime_[i];
        globalServiceTime_[i] = 0;
        globals[i].service->RunTick(ctx, DataAt(globals[i].offset), elapsed);
    }
    for (size_t depth = 0; depth < active_.size(); ++depth)
    {
        SetDepthContext(ctx, depth);
        const auto& def = machine_->State(active_[depth].index);
        for (size_t i = 0; i < def.services.size(); ++i)
        {
            active_[depth].serviceTime[i] += dt;
            if (active_[depth].serviceTime[i] + 1e-6f < def.services[i].interval) continue;
            const float elapsed = active_[depth].serviceTime[i];
            active_[depth].serviceTime[i] = 0;
            def.services[i].service->RunTick(ctx, DataAt(def.services[i].offset), elapsed);
        }
        for (size_t i = 0; i < def.tasks.size(); ++i)
        {
            if (active_[depth].tasks[i] != Status::Running) continue;
            const Status status = def.tasks[i].task->RunTick(ctx, DataAt(def.tasks[i].offset), dt);
            if (status == Status::Running) continue;
            auto& active = active_[depth];
            active.tasks[i] = status;
            if (active.completed == Status::Running || status == Status::Failed) active.completed = status;
        }
    }

    // Delayed transitions whose time has come.
    for (size_t i = 0; i < delayed_.size(); ++i)
    {
        Delayed& delayed = delayed_[i];
        delayed.left -= dt;
        if (delayed.left > 0) continue;
        const Delayed due = delayed;
        delayed_.erase(delayed_.begin() + static_cast<std::ptrdiff_t>(i));
        if (static_cast<size_t>(due.depth) >= active_.size() || active_[due.depth].activation != due.activation) { --i; continue; }
        SetDepthContext(ctx, static_cast<size_t>(due.depth));
        const auto& transition = machine_->State(active_[due.depth].index).transitions[static_cast<size_t>(due.transition)];
        if (Transition(ctx, due.depth, transition, "after a delay")) return;
        --i;
    }

    // Transitions from the leaf to the root; the deepest completion counts for every parent.
    Status completion = Status::Running;
    for (int depth = static_cast<int>(active_.size()) - 1; depth >= 0; --depth)
    {
        SetDepthContext(ctx, static_cast<size_t>(depth));
        Active& active = active_[static_cast<size_t>(depth)];
        if (completion == Status::Running) completion = active.completed;
        const auto& transitions = machine_->State(active.index).transitions;
        for (size_t t = 0; t < transitions.size(); ++t)
        {
            const TransitionDef& transition = transitions[t];
            bool triggered = false;
            std::string reason;
            switch (transition.trigger)
            {
            case TransitionDef::Trigger::Tick: triggered = true; reason = "condition"; break;
            case TransitionDef::Trigger::Succeeded: triggered = completion == Status::Succeeded; reason = "task succeeded"; break;
            case TransitionDef::Trigger::Failed: triggered = completion == Status::Failed; reason = "task failed"; break;
            case TransitionDef::Trigger::Completed: triggered = completion != Status::Running; reason = completion == Status::Failed ? "task failed" : "task succeeded"; break;
            case TransitionDef::Trigger::Event:
                triggered = std::find(events.begin(), events.end(), transition.event) != events.end();
                reason = "event " + transition.event;
                break;
            }
            if (!triggered) continue;
            bool allowed = true;
            for (const auto& condition : transition.conditions) if (!condition(ctx)) { allowed = false; break; }
            if (!allowed) continue;
            if (transition.kind == TransitionDef::Kind::CompleteHost)
            {
                // Finish inside a linked machine completes the state that hosts it.
                for (auto& host : active_)
                    if (host.index == transition.target && host.completed == Status::Running) host.completed = Status::Succeeded;
                break;
            }
            if (transition.delay > 0)
            {
                const bool pending = std::any_of(delayed_.begin(), delayed_.end(), [&](const Delayed& d) {
                    return d.depth == depth && d.activation == active.activation && d.transition == static_cast<int>(t); });
                if (!pending) delayed_.push_back({depth, active.activation, static_cast<int>(t), transition.delay});
                continue;
            }
            if (Transition(ctx, depth, transition, reason.c_str())) return;
        }
        if (static_cast<size_t>(depth) < active_.size() && active_[static_cast<size_t>(depth)].completed != Status::Running && completion == Status::Running)
            completion = active_[static_cast<size_t>(depth)].completed;
    }
    if (completion != Status::Running)
    {
        // Nobody handled the completion: choose again from the root, like StateTree.
        const std::string old = ActivePath();
        ExitTo(ctx, 0);
        if (!SelectInitial(ctx, "completed without a transition, selecting from the root"))
            ctx.Self().Log(Stamp(ctx.Time()) + old + " -> (nothing could be entered)");
    }
}
} // namespace Ai::Detail
