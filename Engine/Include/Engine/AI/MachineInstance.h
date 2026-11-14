#pragma once
#include <Engine/AI/StateMachine.h>
#include <string>
#include <vector>

namespace Ai::Detail
{
/**
 * The running copy of a machine inside one controller: active chain of states, task statuses,
 * Data block, pending delayed transitions. Rules follow Unreal StateTree:
 *  - a parent is never active alone, it enters its first child whose EnterIf conditions hold;
 *  - tasks of every active state run together, the first to finish completes its state;
 *  - transitions are checked from the leaf to the root, in declaration order; one per think;
 *  - a completed state with no matching transition anywhere restarts selection from the root.
 */
class MachineInstance
{
public:
    MachineInstance() = default;
    MachineInstance(const MachineInstance&) = delete;
    MachineInstance& operator=(const MachineInstance&) = delete;
    ~MachineInstance();

    /** Selection happens on the next Think. A running machine must be stopped first. */
    void Start(MachinePtr machine);
    /** Leaves every active state, calling Exit of their tasks. */
    void Stop(Context& ctx);
    bool Running() const { return running_; }
    /** False until the first think has selected the starting states. */
    bool Started() const { return started_; }
    const MachinePtr& Machine() const { return machine_; }
    void Think(Context& ctx, float dt, const std::vector<std::string>& events);
    /** True when a task finished inside Enter: the next frame should think regardless of the interval. */
    bool WantsWake() const { return wake_; }
    /** "Combat/Chase", empty when nothing is active. */
    std::string ActivePath() const;
    std::vector<int> ActiveStates() const;
    struct TaskView { std::string state, task; Status status; };
    std::vector<TaskView> ActiveTasks() const;
    /** Enters exactly this path (save games). False when the path is not in the machine. */
    bool Restore(Context& ctx, const std::string& path);

private:
    struct Active
    {
        int index = -1;
        double enteredAt = 0;
        Status completed = Status::Running;
        std::vector<Status> tasks;
        std::vector<float> serviceTime;
        unsigned activation = 0;
    };
    struct Delayed { int depth; unsigned activation; int transition; float left; };

    void* DataAt(size_t offset) { return reinterpret_cast<unsigned char*>(data_.data()) + offset; }
    bool SelectInitial(Context& ctx, const char* reason);
    bool Select(int target, std::vector<int>& path, Context& ctx) const;
    bool SelectChildren(int state, std::vector<int>& path, Context& ctx) const;
    bool PassesEnter(int state, Context& ctx) const;
    bool Transition(Context& ctx, int fromDepth, const TransitionDef& transition, const char* reason);
    void Enter(Context& ctx, int state);
    void ExitTo(Context& ctx, size_t keep);
    void SetDepthContext(Context& ctx, size_t depth) const;
    std::string PathOf(const std::vector<int>& chain) const;

    MachinePtr machine_;
    std::vector<std::max_align_t> data_;
    std::vector<Active> active_;
    std::vector<float> globalServiceTime_;
    std::vector<Delayed> delayed_;
    unsigned activations_ = 0;
    bool running_ = false, started_ = false, wake_ = false, globalEntered_ = false;
};
} // namespace Ai::Detail
