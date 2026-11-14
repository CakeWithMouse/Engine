#pragma once
#include <Engine/AI/MachineInstance.h>
#include <Engine/AI/Perception.h>
#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

class GameComponent;
class SaveArchive;

namespace Ai
{
class World;
class Controller;

/** What a task, condition or service sees: its controller, memory and the AI world. */
class Context
{
public:
    Context(World& world, Controller& self) : world_(&world), self_(&self) {}
    Controller& Self() { return *self_; }
    World& GetWorld() { return *world_; }
    Blackboard& Memory();
    /** The object this controller drives; null when it has none or the object was removed. */
    template<class T> T* Owner() { return dynamic_cast<T*>(OwnerComponent()); }
    /** Resolves a reference; null when the object was removed. */
    template<class T> T* Resolve(ObjectRef reference) { return dynamic_cast<T*>(ResolveComponent(reference)); }
    GameComponent* OwnerComponent();
    GameComponent* ResolveComponent(ObjectRef reference);
    /** Horizontal distance from the controller to the value in memory; infinity when unknown. */
    float DistanceTo(Key<ObjectRef> target);
    float DistanceTo(Key<glm::vec3> point);
    float Random(float minimum, float maximum);
    /** Seconds in the state whose condition or task is being run. */
    float TimeInState() const;
    float Time() const;

private:
    friend class Detail::MachineInstance;
    World* world_;
    Controller* self_;
    double stateEnteredAt_ = 0;
};

/**
 * The brain of one unit, like Unreal AIController: runs a state machine, owns memory and senses,
 * moves its body. The body is a scene object, a squad (Group) or nothing (a director).
 * Stores the ID of its body, never a pointer.
 */
class Controller
{
public:
    explicit Controller(World& world);
    virtual ~Controller();
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    // Decisions
    void Run(MachinePtr machine);
    void Stop();
    bool IsRunning() const { return machine_.Running(); }
    MachinePtr Machine() const { return machine_.Machine(); }
    /** Delivered at the next think, which happens in the next frame regardless of the interval. */
    void SendEvent(Event event);
    void WakeUp() { wake_ = true; }
    /** 0 thinks every frame; hundreds of units are usually fine with 0.2. */
    void SetThinkInterval(float seconds);
    float ThinkInterval() const { return thinkInterval_; }
    /** Active path such as "Combat/Chase"; empty when nothing is active. */
    std::string ActiveState() const { return machine_.ActivePath(); }
    const Detail::MachineInstance& Instance() const { return machine_; }
    /** Recent transitions and log lines, oldest first. */
    const std::vector<std::string>& History() const { return history_; }
    void Log(const std::string& text);

    Blackboard& Memory() { return memory_; }
    const Blackboard& Memory() const { return memory_; }
    Perception& GetPerception() { return perception_; }
    const Perception& GetPerception() const { return perception_; }
    World& GetWorld() const { return *world_; }
    ObjectRef OwnerRef() const { return {owner_}; }
    GameComponent* Owner() const;
    const std::string& Name() const { return name_; }
    /** Unique number of this controller in its world. */
    uint32_t Serial() const { return serial_; }

    // Body
    glm::vec3 Position() const { return position_; }
    float Yaw() const { return yaw_; }
    void SetRadius(float radius);
    float Radius() const { return radius_; }
    /** Movement speed in metres per second; 0 uses Character::GetMoveSpeed. */
    void SetSpeed(float speed);
    float Speed() const;
    bool CanMove() const { return canMove_; }

    // Movement
    virtual void MoveTo(glm::vec3 point, float acceptanceRadius = 0.5f);
    /** Follows a moving object; the path is updated by itself. */
    virtual void MoveTo(ObjectRef target, float acceptanceRadius = 1.5f);
    virtual void StopMovement();
    MoveStatus GetMoveStatus() const { return moveStatus_; }
    /** Remaining corner points of the current path. */
    const std::vector<glm::vec3>& Path() const { return path_; }
    glm::vec3 Destination() const { return goalPoint_; }
    void SetRoute(std::vector<glm::vec3> points);
    const std::vector<glm::vec3>& Route() const { return route_; }
    size_t RouteIndex() const { return routeIndex_; }
    void SetRouteIndex(size_t index) { routeIndex_ = route_.empty() ? 0 : index % route_.size(); }
    /** Look at an object while moving or standing. */
    void SetFocus(ObjectRef target) { focus_ = target; }
    void ClearFocus() { focus_ = {}; }
    ObjectRef Focus() const { return focus_; }

    // Hooks for subclasses
    virtual void OnPossess(GameComponent&) {}
    virtual void OnPerception(const Stimulus&) {}
    virtual void OnMoveFinished(MoveStatus) {}
    /** Overrides call their immediate base, like every SerializeSaveGame in the engine. */
    virtual void SerializeSaveGame(SaveArchive& archive);

protected:
    /** Groups override how they move; a plain controller moves its own body. */
    virtual void UpdateMovement(float dt);
    virtual void WriteBody(float dt);
    /** Reads position and yaw of the body; false when the body was removed. */
    virtual bool SyncBody();
    void FinishMove(MoveStatus status);

private:
    friend class World;
    friend class Group;
    friend class Detail::MachineInstance;
    friend class Context;
    void Think(float dt);
    void RequestPath();
    void AdvancePath();
    glm::vec3 ObjectPosition(ObjectRef reference) const;

    World* world_;
    ComponentId owner_ = 0;
    std::string name_;
    uint32_t serial_ = 0;
    Detail::MachineInstance machine_;
    Blackboard memory_;
    Perception perception_;
    std::vector<std::string> history_;
    std::vector<std::string> events_;
    std::mt19937 random_;
    float thinkInterval_ = 0, sinceThink_ = 0, thinkPhase_ = 0;
    bool wake_ = false, dead_ = false, synced_ = false, needsPath_ = false;

    // Body
    glm::vec3 position_{0};
    float yaw_ = 0, radius_ = 0.4f, speed_ = 0;
    bool canMove_ = false, isCharacter_ = false;

    // Movement
    enum class Goal { None, Point, Object };
    Goal goal_ = Goal::None;
    glm::vec3 goalPoint_{0};
    ObjectRef goalObject_;
    float acceptance_ = 0.5f;
    MoveStatus moveStatus_ = MoveStatus::Idle;
    std::vector<glm::vec3> path_;
    bool partial_ = false;
    uint64_t pathRequest_ = 0;
    uint32_t pathVersion_ = 0;
    glm::vec3 pathGoal_{0};
    float repathTimer_ = 0, progressTimer_ = 0, progressDistance_ = 0;
    glm::vec3 progressFrom_{0};
    int stuckCount_ = 0;
    glm::vec3 velocity_{0}, desired_{0};
    bool settled_ = true;
    std::vector<glm::vec3> route_;
    size_t routeIndex_ = 0;
    ObjectRef focus_;
};
} // namespace Ai
