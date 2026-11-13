#pragma once
#include <Engine/AI/Query.h>
#include <Engine/AI/StateMachine.h>
#include <string>

// Ready tasks and conditions. Game code writes only what belongs to its rules.
namespace Ai
{
namespace Tasks
{
/** Waits, then succeeds. */
class Wait final : public Task<float>
{
public:
    explicit Wait(float seconds) : seconds_(seconds) {}
    Status Enter(Context&, float& left) const override { left = seconds_; return Status::Running; }
    Status Tick(Context&, float& left, float dt) const override { left -= dt; return left > 0 ? Status::Running : Status::Succeeded; }
    std::string Name() const override { return "Wait"; }
private:
    float seconds_;
};

class WaitRandom final : public Task<float>
{
public:
    WaitRandom(float minimum, float maximum) : minimum_(minimum), maximum_(maximum) {}
    Status Enter(Context& ctx, float& left) const override;
    Status Tick(Context&, float& left, float dt) const override { left -= dt; return left > 0 ? Status::Running : Status::Succeeded; }
    std::string Name() const override { return "WaitRandom"; }
private:
    float minimum_, maximum_;
};

/** Goes to an object or a point from memory. Fails when the key is empty or the goal is unreachable. */
class MoveTo final : public Task<>
{
public:
    MoveTo(Key<ObjectRef> target, float acceptanceRadius) : object_(target.name), radius_(acceptanceRadius) {}
    MoveTo(Key<glm::vec3> point, float acceptanceRadius) : point_(point.name), radius_(acceptanceRadius) {}
    Status Enter(Context& ctx, NoData&) const override;
    Status Tick(Context& ctx, NoData&, float) const override;
    void Exit(Context& ctx, NoData&, Status result) const override;
    std::string Name() const override;
private:
    std::string object_, point_;
    float radius_;
};

/** Goes to the next point of the route set with Controller::SetRoute, then succeeds. */
class FollowRoute final : public Task<>
{
public:
    explicit FollowRoute(float acceptanceRadius = 0.5f) : radius_(acceptanceRadius) {}
    Status Enter(Context& ctx, NoData&) const override;
    Status Tick(Context& ctx, NoData&, float) const override;
    void Exit(Context& ctx, NoData&, Status result) const override;
    std::string Name() const override { return "FollowRoute"; }
private:
    float radius_;
};

/** Looks at an object from memory while the state is active. Never finishes by itself. */
class LookAt final : public Task<>
{
public:
    explicit LookAt(Key<ObjectRef> target) : target_(target.name) {}
    Status Enter(Context& ctx, NoData&) const override;
    Status Tick(Context& ctx, NoData&, float) const override;
    void Exit(Context& ctx, NoData&, Status) const override;
    std::string Name() const override { return "LookAt " + target_; }
private:
    std::string target_;
};

class SendEvent final : public Task<>
{
public:
    explicit SendEvent(Event event) : event_(event.name ? event.name : "") {}
    Status Enter(Context& ctx, NoData&) const override;
    std::string Name() const override { return "SendEvent " + event_; }
private:
    std::string event_;
};

class ClearKey final : public Task<>
{
public:
    explicit ClearKey(Key<ObjectRef> key) : key_(key.name) {}
    explicit ClearKey(Key<glm::vec3> key) : key_(key.name) {}
    Status Enter(Context& ctx, NoData&) const override;
    std::string Name() const override { return "ClearKey " + key_; }
private:
    std::string key_;
};

/** Runs a query and writes the best point to memory; fails when nothing passes. */
class RunQuery final : public Task<>
{
public:
    RunQuery(QueryPtr query, Key<glm::vec3> result) : query_(std::move(query)), result_(result.name) {}
    Status Enter(Context& ctx, NoData&) const override;
    std::string Name() const override;
private:
    QueryPtr query_;
    std::string result_;
};

class Log final : public Task<>
{
public:
    explicit Log(std::string text) : text_(std::move(text)) {}
    Status Enter(Context& ctx, NoData&) const override;
    std::string Name() const override { return "Log"; }
private:
    std::string text_;
};

struct SmartObjectUse { ComponentId object = 0; float left = 0; bool arrived = false; };

/** Finds the nearest free smart object with the tag, claims a slot, walks there and uses it. */
class UseSmartObject final : public Task<SmartObjectUse>
{
public:
    UseSmartObject(std::string tag, float seconds, float searchRadius = 50) : tag_(std::move(tag)), seconds_(seconds), search_(searchRadius) {}
    Status Enter(Context& ctx, SmartObjectUse& use) const override;
    Status Tick(Context& ctx, SmartObjectUse& use, float dt) const override;
    void Exit(Context& ctx, SmartObjectUse& use, Status) const override;
    std::string Name() const override { return "UseSmartObject " + tag_; }
private:
    std::string tag_;
    float seconds_, search_;
};
} // namespace Tasks

namespace Conditions
{
ConditionFn HasKey(Key<ObjectRef> key);
ConditionFn HasKey(Key<glm::vec3> key);
ConditionFn HasKey(Key<bool> key);
ConditionFn IsTrue(Key<bool> key);
ConditionFn Near(Key<ObjectRef> target, float distance);
ConditionFn Near(Key<glm::vec3> point, float distance);
ConditionFn CanSee(Key<ObjectRef> target);
ConditionFn TimeInState(float seconds);
ConditionFn Chance(float probability);
}
} // namespace Ai
