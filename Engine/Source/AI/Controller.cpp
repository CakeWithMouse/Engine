#include <Engine/AI/Controller.h>
#include <Engine/AI/Navigation.h>
#include <Engine/AI/World.h>
#include <Engine/Components/Character.h>
#include <Engine/SaveGame/SaveArchive.h>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Ai
{
namespace
{
constexpr float Pi = 3.14159265358979f;
constexpr size_t HistoryLimit = 64;
glm::vec2 Flat(glm::vec3 v) { return {v.x, v.z}; }
float FlatDistance(glm::vec3 a, glm::vec3 b) { return glm::length(Flat(a) - Flat(b)); }
float WrapAngle(float a)
{
    while (a > Pi) a -= 2 * Pi;
    while (a < -Pi) a += 2 * Pi;
    return a;
}
} // namespace

// Context
Blackboard& Context::Memory() { return self_->Memory(); }
GameComponent* Context::OwnerComponent() { return self_->Owner(); }
GameComponent* Context::ResolveComponent(ObjectRef reference) { return reference ? world_->Resolve(reference.id) : nullptr; }
float Context::Time() const { return world_->Time(); }
float Context::TimeInState() const { return static_cast<float>(world_->Time() - stateEnteredAt_); }
float Context::Random(float minimum, float maximum)
{
    if (maximum < minimum) std::swap(minimum, maximum);
    return std::uniform_real_distribution<float>(minimum, std::nextafter(maximum, maximum + 1.0f))(self_->random_);
}
float Context::DistanceTo(Key<ObjectRef> target)
{
    const ObjectRef reference = self_->Memory().Get(target);
    GameComponent* object = ResolveComponent(reference);
    if (!object || (!self_->Owner() && self_->owner_)) return std::numeric_limits<float>::infinity();
    return FlatDistance(self_->Position(), object->GetWorldPosition());
}
float Context::DistanceTo(Key<glm::vec3> point)
{
    if (!self_->Memory().Has(point)) return std::numeric_limits<float>::infinity();
    return FlatDistance(self_->Position(), self_->Memory().Get(point));
}

// Controller
Controller::Controller(World& world)
    : world_(&world), serial_(world.NextSerial()), random_(world.Seed() ^ (serial_ * 2654435761u))
{
}

Controller::~Controller()
{
    // Tasks must see Exit even when the controller dies with its body. Errors here cannot propagate.
    try { Stop(); } catch (...) {}
    if (pathRequest_) world_->Paths().Cancel(pathRequest_);
}

void Controller::Run(MachinePtr machine)
{
    if (!machine) throw std::invalid_argument("Controller::Run: null state machine");
    if (machine_.Machine()) Stop();
    machine_.Start(std::move(machine));
    wake_ = true;
    world_->Restore(*this);
}

void Controller::Stop()
{
    Context ctx(*world_, *this);
    machine_.Stop(ctx);
}

void Controller::SendEvent(Event event)
{
    if (!event.name || !*event.name) return;
    events_.emplace_back(event.name);
    wake_ = true;
}

void Controller::SetThinkInterval(float seconds)
{
    thinkInterval_ = std::isfinite(seconds) && seconds > 0 ? seconds : 0;
    // Spread controllers over the interval so they do not all think in the same frame.
    sinceThink_ = thinkInterval_ > 0 ? std::uniform_real_distribution<float>(0, thinkInterval_)(random_) : 0;
}

void Controller::Log(const std::string& text)
{
    history_.push_back(text);
    if (history_.size() > HistoryLimit) history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - HistoryLimit));
}

GameComponent* Controller::Owner() const { return owner_ ? world_->Resolve(owner_) : nullptr; }

void Controller::SetRadius(float radius)
{
    if (!(radius > 0)) throw std::invalid_argument("Controller::SetRadius: radius must be positive");
    radius_ = radius;
}

void Controller::SetSpeed(float speed)
{
    if (!(speed >= 0) || !std::isfinite(speed)) throw std::invalid_argument("Controller::SetSpeed: invalid speed");
    speed_ = speed;
}

float Controller::Speed() const
{
    if (speed_ > 0) return speed_;
    if (isCharacter_) if (auto* character = dynamic_cast<Character*>(Owner())) return character->GetMoveSpeed();
    return 5.0f;
}

void Controller::SetRoute(std::vector<glm::vec3> points)
{
    route_ = std::move(points);
    routeIndex_ = 0;
}

void Controller::Think(float dt)
{
    sinceThink_ += dt;
    if (!wake_ && !machine_.WantsWake() && sinceThink_ + 1e-6f < thinkInterval_) return;
    const float elapsed = sinceThink_;
    sinceThink_ = 0;
    wake_ = false;
    Context ctx(*world_, *this);
    if (machine_.Running() && !machine_.Started())
    {
        // The first think only selects the starting states: events sent before it wait for the next one.
        machine_.Think(ctx, elapsed, {});
        if (!events_.empty()) wake_ = true;
        return;
    }
    std::vector<std::string> events;
    events.swap(events_);
    machine_.Think(ctx, elapsed, events);
}

bool Controller::SyncBody()
{
    if (!owner_) return true; // a director has no body
    GameComponent* body = world_->Resolve(owner_);
    if (!body) return false;
    position_ = body->GetWorldPosition();
    if (!synced_)
    {
        synced_ = true;
        isCharacter_ = dynamic_cast<Character*>(body) != nullptr;
        canMove_ = isCharacter_;
        const glm::vec3 forward = body->GetLocalRotationQuat() * glm::vec3(0, 0, 1);
        if (std::abs(forward.x) + std::abs(forward.z) > 1e-4f) yaw_ = std::atan2(forward.x, forward.z);
    }
    return true;
}

glm::vec3 Controller::ObjectPosition(ObjectRef reference) const
{
    GameComponent* object = reference ? world_->Resolve(reference.id) : nullptr;
    if (!object) return glm::vec3(std::numeric_limits<float>::quiet_NaN());
    return object->GetWorldPosition();
}

void Controller::MoveTo(glm::vec3 point, float acceptanceRadius)
{
    if (!canMove_) { StopMovement(); moveStatus_ = MoveStatus::Unavailable; return; }
    if (!std::isfinite(point.x) || !std::isfinite(point.z)) throw std::invalid_argument("MoveTo: point is not finite");
    goal_ = Goal::Point;
    goalPoint_ = point;
    goalObject_ = {};
    acceptance_ = std::max(0.05f, acceptanceRadius);
    stuckCount_ = 0;
    progressTimer_ = 0;
    progressFrom_ = position_;
    moveStatus_ = MoveStatus::Waiting;
    RequestPath();
}

void Controller::MoveTo(ObjectRef target, float acceptanceRadius)
{
    if (!canMove_) { StopMovement(); moveStatus_ = MoveStatus::Unavailable; return; }
    const glm::vec3 at = ObjectPosition(target);
    if (!std::isfinite(at.x)) { StopMovement(); moveStatus_ = MoveStatus::Unreachable; return; }
    goal_ = Goal::Object;
    goalObject_ = target;
    goalPoint_ = at;
    acceptance_ = std::max(0.05f, acceptanceRadius);
    stuckCount_ = 0;
    progressTimer_ = 0;
    progressFrom_ = position_;
    moveStatus_ = MoveStatus::Waiting;
    RequestPath();
}

void Controller::StopMovement()
{
    if (pathRequest_) world_->Paths().Cancel(pathRequest_);
    pathRequest_ = 0;
    goal_ = Goal::None;
    path_.clear();
    desired_ = glm::vec3(0);
    if (moveStatus_ == MoveStatus::Waiting || moveStatus_ == MoveStatus::Moving) moveStatus_ = MoveStatus::Idle;
}

void Controller::FinishMove(MoveStatus status)
{
    if (pathRequest_) world_->Paths().Cancel(pathRequest_);
    pathRequest_ = 0;
    goal_ = Goal::None;
    path_.clear();
    desired_ = glm::vec3(0);
    moveStatus_ = status;
    OnMoveFinished(status);
}

void Controller::RequestPath()
{
    NavGrid& grid = world_->Navigation();
    pathGoal_ = goalPoint_;
    repathTimer_ = 0;
    if (!grid.IsConfigured())
    {
        // Open field: straight to the goal.
        path_.assign(1, glm::vec3(goalPoint_.x, position_.y, goalPoint_.z));
        partial_ = false;
        pathVersion_ = grid.Version();
        moveStatus_ = MoveStatus::Moving;
        return;
    }
    if (pathRequest_) world_->Paths().Cancel(pathRequest_);
    pathRequest_ = world_->Paths().Request(Flat(position_), Flat(goalPoint_), radius_);
    if (path_.empty()) moveStatus_ = MoveStatus::Waiting;
}

void Controller::UpdateMovement(float dt)
{
    if (goal_ == Goal::None) { desired_ = glm::vec3(0); return; }
    NavGrid& grid = world_->Navigation();
    if (needsPath_) { needsPath_ = false; RequestPath(); }
    if (goal_ == Goal::Object)
    {
        const glm::vec3 at = ObjectPosition(goalObject_);
        if (!std::isfinite(at.x)) { FinishMove(MoveStatus::Unreachable); return; }
        goalPoint_ = at;
        repathTimer_ += dt;
        const float moved = FlatDistance(pathGoal_, goalPoint_);
        if (!pathRequest_ && (moved > std::max(1.0f, grid.CellSize()) || (repathTimer_ > 1.0f && moved > 0.1f))) RequestPath();
    }
    if (pathRequest_)
    {
        PathResult result;
        if (world_->Paths().Take(pathRequest_, result))
        {
            pathRequest_ = 0;
            if (result.kind == PathResult::Kind::NotFound) { FinishMove(MoveStatus::Unreachable); return; }
            path_.clear();
            for (const glm::vec2 p : result.points) path_.emplace_back(p.x, position_.y, p.y);
            partial_ = result.kind == PathResult::Kind::Partial;
            pathVersion_ = grid.Version();
            moveStatus_ = MoveStatus::Moving;
        }
    }
    if (grid.IsConfigured() && grid.Version() != pathVersion_ && !pathRequest_)
    {
        pathVersion_ = grid.Version();
        if (!path_.empty() && !grid.CanWalkStraight(Flat(position_), Flat(path_.front()), radius_)) RequestPath();
    }

    const float distance = FlatDistance(position_, goalPoint_);
    if (distance <= acceptance_) { FinishMove(MoveStatus::Arrived); return; }
    const float reach = std::max(0.25f, std::min(radius_, 0.5f * grid.CellSize()));
    while (!path_.empty() && FlatDistance(position_, path_.front()) <= reach) path_.erase(path_.begin());
    if (path_.empty())
    {
        if (moveStatus_ == MoveStatus::Waiting || pathRequest_) { desired_ = glm::vec3(0); return; }
        const float snap = grid.IsConfigured() ? 1.5f * grid.CellSize() : 0.0f;
        if (partial_) FinishMove(MoveStatus::Unreachable);
        else if (distance <= acceptance_ + snap) FinishMove(MoveStatus::Arrived);
        else RequestPath();
        return;
    }
    glm::vec2 direction = Flat(path_.front()) - Flat(position_);
    const float length = glm::length(direction);
    direction = length > 1e-5f ? direction / length : glm::vec2(0);
    float speed = Speed();
    if (path_.size() == 1) speed *= std::clamp(length / std::max(0.01f, 0.5f * speed), 0.25f, 1.0f);
    desired_ = glm::vec3(direction.x, 0, direction.y) * speed;

    progressTimer_ += dt;
    if (progressTimer_ >= 1.0f)
    {
        const float moved = FlatDistance(progressFrom_, position_);
        const float expected = Speed() * progressTimer_;
        if (moved < 0.1f * expected)
        {
            if (++stuckCount_ >= 3) { FinishMove(MoveStatus::Stuck); return; }
            RequestPath();
        }
        else stuckCount_ = 0;
        progressTimer_ = 0;
        progressFrom_ = position_;
    }
}

void Controller::WriteBody(float dt)
{
    if (!owner_ || dt <= 0) return;
    GameComponent* body = world_->Resolve(owner_);
    if (!body) return;
    NavGrid& grid = world_->Navigation();
    glm::vec3 step = desired_ * dt;
    if (isCharacter_ && glm::length(step) > 1e-6f)
    {
        // Never cross more than half a cell in one frame, never enter a blocked cell.
        const float limit = grid.IsConfigured() ? 0.5f * grid.CellSize() : 1e9f;
        const float length = glm::length(step);
        if (length > limit) step *= limit / length;
        // Only entering a blocked cell is refused: a unit that starts inside an obstacle may walk out.
        if (grid.IsConfigured() && grid.IsWalkable(Flat(position_), radius_))
        {
            const glm::vec2 from = Flat(position_);
            if (!grid.IsWalkable(from + Flat(step), radius_))
            {
                if (grid.IsWalkable(from + glm::vec2(step.x, 0), radius_)) step.z = 0;
                else if (grid.IsWalkable(from + glm::vec2(0, step.z), radius_)) step.x = 0;
                else step = glm::vec3(0);
            }
        }
        if (glm::length(step) > 1e-6f)
        {
            auto* character = static_cast<Character*>(body);
            const float capacity = character->GetMoveSpeed() * dt;
            if (capacity > 0 && glm::length(step) <= capacity * 1.0001f) character->Move(step / capacity, dt);
            else character->SetPosition(character->GetLocalPosition() + step);
            position_ += step;
        }
        velocity_ = step / dt;
    }
    else velocity_ = glm::vec3(0);

    // Face the focus, otherwise the direction of travel.
    float target = yaw_;
    bool turn = false;
    if (focus_)
    {
        const glm::vec3 at = ObjectPosition(focus_);
        if (std::isfinite(at.x) && FlatDistance(at, position_) > 1e-3f) { target = std::atan2(at.x - position_.x, at.z - position_.z); turn = true; }
    }
    else if (glm::length(Flat(velocity_)) > 0.05f) { target = std::atan2(velocity_.x, velocity_.z); turn = true; }
    if (turn)
    {
        const float delta = WrapAngle(target - yaw_);
        const float maxTurn = 8.0f * dt;
        yaw_ = WrapAngle(yaw_ + std::clamp(delta, -maxTurn, maxTurn));
        body->SetRotationQuat(glm::angleAxis(yaw_, glm::vec3(0, 1, 0)));
    }
}

void Controller::SerializeSaveGame(SaveArchive& archive)
{
    archive.Version(1);
    archive.Section("memory", [&](SaveArchive& memory) { memory_.SerializeSaveGame(memory); });
    archive.Field("thinkInterval", thinkInterval_);
    archive.Field("radius", radius_);
    archive.Field("speed", speed_);
    std::vector<float> route;
    if (archive.IsWriting()) for (const auto& p : route_) { route.push_back(p.x); route.push_back(p.y); route.push_back(p.z); }
    archive.Field("route", route);
    uint64_t index = routeIndex_;
    archive.Field("routeIndex", index);
    int goal = static_cast<int>(goal_);
    archive.Field("goal", goal);
    archive.Field("goalPoint", goalPoint_);
    uint64_t goalObject = goalObject_.id, focus = focus_.id;
    archive.Reference("goalObject", goalObject);
    archive.Reference("focus", focus);
    archive.Field("acceptance", acceptance_);
    if (archive.IsReading())
    {
        if (route.size() % 3) throw std::runtime_error("Controller: invalid saved route");
        route_.clear();
        for (size_t i = 0; i + 2 < route.size(); i += 3) route_.emplace_back(route[i], route[i + 1], route[i + 2]);
        routeIndex_ = route_.empty() ? 0 : static_cast<size_t>(index) % route_.size();
        if (goal < 0 || goal > 2) throw std::runtime_error("Controller: invalid saved movement");
        goalObject_ = {goalObject};
        focus_ = {focus};
        SetThinkInterval(thinkInterval_);
        // No side effects while reading: the path is requested by the next movement step.
        if (pathRequest_) world_->Paths().Cancel(pathRequest_);
        pathRequest_ = 0;
        path_.clear();
        goal_ = static_cast<Goal>(goal);
        needsPath_ = goal_ != Goal::None;
        moveStatus_ = goal_ != Goal::None ? MoveStatus::Waiting : MoveStatus::Idle;
    }
}
} // namespace Ai
