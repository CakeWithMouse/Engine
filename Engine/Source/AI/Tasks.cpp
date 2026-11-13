#include <Engine/AI/Controller.h>
#include <Engine/AI/SmartObject.h>
#include <Engine/AI/Tasks.h>
#include <Engine/AI/World.h>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace Ai
{
// Perception
void Perception::EnableSight(float radius, float fieldOfViewDegrees, float memorySeconds)
{
    if (!(radius > 0) || !std::isfinite(radius)) throw std::invalid_argument("EnableSight: radius must be positive");
    if (!(memorySeconds >= 0)) throw std::invalid_argument("EnableSight: memory must be zero or more");
    sightRadius_ = radius;
    fieldOfView_ = std::fmin(std::fmax(fieldOfViewDegrees, 1.0f), 360.0f);
    memory_ = memorySeconds;
}

void Perception::EnableHearing(float radius)
{
    if (!(radius >= 0) || !std::isfinite(radius)) throw std::invalid_argument("EnableHearing: invalid radius");
    hearingRadius_ = radius;
}

const Perceived* Perception::Best() const
{
    const Perceived* best = nullptr;
    for (const auto& known : known_)
    {
        if (!best) { best = &known; continue; }
        if (known.visibleNow != best->visibleNow) { if (known.visibleNow) best = &known; continue; }
        if (known.visibleNow ? known.distance < best->distance : known.age < best->age) best = &known;
    }
    return best;
}

bool Perception::CanSee(ObjectRef who) const
{
    for (const auto& known : known_) if (known.who == who) return known.visibleNow;
    return false;
}

namespace Tasks
{
namespace
{
Status MoveResult(Controller& self)
{
    switch (self.GetMoveStatus())
    {
    case MoveStatus::Arrived: return Status::Succeeded;
    case MoveStatus::Waiting:
    case MoveStatus::Moving: return Status::Running;
    default: return Status::Failed;
    }
}
} // namespace

Status WaitRandom::Enter(Context& ctx, float& left) const
{
    left = ctx.Random(minimum_, maximum_);
    return Status::Running;
}

Status MoveTo::Enter(Context& ctx, NoData&) const
{
    Controller& self = ctx.Self();
    if (!object_.empty())
    {
        const ObjectRef target = ctx.Memory().Get(Key<ObjectRef>{object_.c_str()});
        if (!target) return Status::Failed;
        self.MoveTo(target, radius_);
    }
    else
    {
        if (!ctx.Memory().Has(Key<glm::vec3>{point_.c_str()})) return Status::Failed;
        self.MoveTo(ctx.Memory().Get(Key<glm::vec3>{point_.c_str()}), radius_);
    }
    return MoveResult(self);
}
Status MoveTo::Tick(Context& ctx, NoData&, float) const { return MoveResult(ctx.Self()); }
void MoveTo::Exit(Context& ctx, NoData&, Status result) const { if (result == Status::Running) ctx.Self().StopMovement(); }
std::string MoveTo::Name() const { return "MoveTo " + (object_.empty() ? point_ : object_); }

Status FollowRoute::Enter(Context& ctx, NoData&) const
{
    Controller& self = ctx.Self();
    if (self.Route().empty()) return Status::Failed;
    self.MoveTo(self.Route()[self.RouteIndex()], radius_);
    return MoveResult(self);
}
Status FollowRoute::Tick(Context& ctx, NoData&, float) const
{
    Controller& self = ctx.Self();
    const Status status = MoveResult(self);
    // The next leg starts from the next point, whatever happened to this one.
    if (status != Status::Running) self.SetRouteIndex(self.RouteIndex() + 1);
    return status;
}
void FollowRoute::Exit(Context& ctx, NoData&, Status result) const { if (result == Status::Running) ctx.Self().StopMovement(); }

Status LookAt::Enter(Context& ctx, NoData& data) const { return Tick(ctx, data, 0); }
Status LookAt::Tick(Context& ctx, NoData&, float) const
{
    const ObjectRef target = ctx.Memory().Get(Key<ObjectRef>{target_.c_str()});
    if (target) ctx.Self().SetFocus(target);
    else ctx.Self().ClearFocus();
    return Status::Running;
}
void LookAt::Exit(Context& ctx, NoData&, Status) const { ctx.Self().ClearFocus(); }

Status SendEvent::Enter(Context& ctx, NoData&) const
{
    ctx.Self().SendEvent({event_.c_str()});
    return Status::Succeeded;
}

Status ClearKey::Enter(Context& ctx, NoData&) const
{
    ctx.Memory().Clear(Key<ObjectRef>{key_.c_str()});
    return Status::Succeeded;
}

Status RunQuery::Enter(Context& ctx, NoData&) const
{
    const auto items = query_->Run(ctx);
    if (items.empty()) return Status::Failed;
    ctx.Memory().Set(Key<glm::vec3>{result_.c_str()}, items.front().point);
    return Status::Succeeded;
}
std::string RunQuery::Name() const { return "RunQuery " + query_->Name(); }

Status Log::Enter(Context& ctx, NoData&) const
{
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "%6.2f  ", ctx.Time());
    ctx.Self().Log(stamp + text_);
    return Status::Succeeded;
}

Status UseSmartObject::Enter(Context& ctx, SmartObjectUse& use) const
{
    Controller& self = ctx.Self();
    SmartObject* object = ctx.GetWorld().FindSmartObject(tag_, self.Position(), search_);
    if (!object || !object->Claim(self.Serial())) return Status::Failed;
    use.object = object->GetComponentId();
    use.left = seconds_;
    use.arrived = !self.CanMove();
    if (!use.arrived) self.MoveTo(object->GetWorldPosition(), 0.6f);
    return Status::Running;
}
Status UseSmartObject::Tick(Context& ctx, SmartObjectUse& use, float dt) const
{
    if (!ctx.ResolveComponent({use.object})) return Status::Failed;
    if (!use.arrived)
    {
        const Status move = MoveResult(ctx.Self());
        if (move == Status::Failed) return Status::Failed;
        if (move == Status::Running) return Status::Running;
        use.arrived = true;
    }
    use.left -= dt;
    return use.left > 0 ? Status::Running : Status::Succeeded;
}
void UseSmartObject::Exit(Context& ctx, SmartObjectUse& use, Status result) const
{
    if (auto* object = dynamic_cast<SmartObject*>(ctx.ResolveComponent({use.object}))) object->Release(ctx.Self().Serial());
    if (result == Status::Running && !use.arrived) ctx.Self().StopMovement();
}
} // namespace Tasks

namespace Conditions
{
ConditionFn HasKey(Key<ObjectRef> key) { return [name = std::string(key.name)](Context& ctx) { return ctx.Memory().Has(Key<ObjectRef>{name.c_str()}); }; }
ConditionFn HasKey(Key<glm::vec3> key) { return [name = std::string(key.name)](Context& ctx) { return ctx.Memory().Has(Key<glm::vec3>{name.c_str()}); }; }
ConditionFn HasKey(Key<bool> key) { return [name = std::string(key.name)](Context& ctx) { return ctx.Memory().Has(Key<bool>{name.c_str()}); }; }
ConditionFn IsTrue(Key<bool> key) { return [name = std::string(key.name)](Context& ctx) { return ctx.Memory().Get(Key<bool>{name.c_str()}, false); }; }
ConditionFn Near(Key<ObjectRef> target, float distance)
{
    return [name = std::string(target.name), distance](Context& ctx) { return ctx.DistanceTo(Key<ObjectRef>{name.c_str()}) <= distance; };
}
ConditionFn Near(Key<glm::vec3> point, float distance)
{
    return [name = std::string(point.name), distance](Context& ctx) { return ctx.DistanceTo(Key<glm::vec3>{name.c_str()}) <= distance; };
}
ConditionFn CanSee(Key<ObjectRef> target)
{
    return [name = std::string(target.name)](Context& ctx) {
        return ctx.Self().GetPerception().CanSee(ctx.Memory().Get(Key<ObjectRef>{name.c_str()}));
    };
}
ConditionFn TimeInState(float seconds) { return [seconds](Context& ctx) { return ctx.TimeInState() >= seconds; }; }
ConditionFn Chance(float probability) { return [probability](Context& ctx) { return ctx.Random(0, 1) < probability; }; }
} // namespace Conditions
} // namespace Ai
