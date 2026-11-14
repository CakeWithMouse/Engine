#include <Engine/AI/Controller.h>
#include <Engine/AI/Navigation.h>
#include <Engine/AI/Query.h>
#include <Engine/AI/World.h>
#include <Engine/Components/GameComponents.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Ai
{
namespace
{
constexpr float Pi = 3.14159265358979f;
glm::vec2 Flat(glm::vec3 v) { return {v.x, v.z}; }

void RingAround(glm::vec3 center, float minRadius, float maxRadius, int count, std::vector<glm::vec3>& out)
{
    if (count <= 0) return;
    const int rings = std::max(1, static_cast<int>(std::ceil(count / 12.0f)));
    const int perRing = (count + rings - 1) / rings;
    int made = 0;
    for (int ring = 0; ring < rings; ++ring)
    {
        const float radius = rings == 1 ? 0.5f * (minRadius + maxRadius) : minRadius + (maxRadius - minRadius) * ring / (rings - 1);
        const float offset = ring * 0.5f / perRing; // stagger rings
        for (int i = 0; i < perRing && made < count; ++i, ++made)
        {
            const float angle = 2 * Pi * (static_cast<float>(i) / perRing + offset);
            out.emplace_back(center.x + std::sin(angle) * radius, center.y, center.z + std::cos(angle) * radius);
        }
    }
}

bool TargetPosition(Context& ctx, const std::string& key, glm::vec3& position)
{
    GameComponent* target = ctx.ResolveComponent(ctx.Memory().Get(Key<ObjectRef>{key.c_str()}));
    if (!target) return false;
    position = target->GetWorldPosition();
    return true;
}
} // namespace

std::vector<QueryItem> QueryBase::Run(Context& ctx) const
{
    std::vector<glm::vec3> points;
    for (const auto& generator : points_) generator.generate(ctx, points);
    if (points.size() > MaxPoints) points.resize(MaxPoints);
    points.erase(std::remove_if(points.begin(), points.end(), [&](const glm::vec3& p) {
        for (const auto& filter : filters_) if (!filter.filter(ctx, p)) return true;
        return false;
    }), points.end());
    std::vector<QueryItem> items;
    for (const auto& p : points) items.push_back({p, 0});
    std::vector<float> raw(items.size());
    for (const auto& scorer : scores_)
    {
        float low = std::numeric_limits<float>::infinity(), high = -low;
        for (size_t i = 0; i < items.size(); ++i)
        {
            raw[i] = scorer.test.score(ctx, items[i].point);
            low = std::min(low, raw[i]);
            high = std::max(high, raw[i]);
        }
        for (size_t i = 0; i < items.size(); ++i)
        {
            const float t = high > low ? (raw[i] - low) / (high - low) : 0.5f;
            items[i].score += scorer.weight * (scorer.prefer == Prefer::Far ? t : 1 - t);
        }
    }
    std::stable_sort(items.begin(), items.end(), [](const QueryItem& a, const QueryItem& b) { return a.score > b.score; });
    return items;
}

namespace Generators
{
QueryPoints Ring(float minRadius, float maxRadius, int count)
{
    if (!(minRadius >= 0) || !(maxRadius >= minRadius) || count <= 0) throw std::invalid_argument("Generators::Ring: invalid radii or count");
    return {"Ring", [=](Context& ctx, std::vector<glm::vec3>& out) { RingAround(ctx.Self().Position(), minRadius, maxRadius, count, out); }};
}
QueryPoints Grid(float halfSize, float step)
{
    if (!(halfSize > 0) || !(step > 0)) throw std::invalid_argument("Generators::Grid: invalid size or step");
    return {"Grid", [=](Context& ctx, std::vector<glm::vec3>& out) {
        const glm::vec3 c = ctx.Self().Position();
        for (float z = -halfSize; z <= halfSize + 1e-4f; z += step)
            for (float x = -halfSize; x <= halfSize + 1e-4f; x += step) out.emplace_back(c.x + x, c.y, c.z + z);
    }};
}
QueryPoints Around(Key<ObjectRef> target, float minRadius, float maxRadius, int count)
{
    if (!(minRadius >= 0) || !(maxRadius >= minRadius) || count <= 0) throw std::invalid_argument("Generators::Around: invalid radii or count");
    return {"Around", [=, key = std::string(target.name)](Context& ctx, std::vector<glm::vec3>& out) {
        glm::vec3 center;
        if (TargetPosition(ctx, key, center)) RingAround(center, minRadius, maxRadius, count, out);
    }};
}
} // namespace Generators

namespace Tests
{
QueryTest Reachable()
{
    QueryTest test{"Reachable", nullptr, nullptr};
    test.filter = [](Context& ctx, glm::vec3 p) {
        const NavGrid& grid = ctx.GetWorld().Navigation();
        const float radius = ctx.Self().Radius();
        return grid.IsWalkable(Flat(p), radius) && grid.Connected(Flat(ctx.Self().Position()), Flat(p), radius);
    };
    return test;
}
QueryTest HiddenFrom(Key<ObjectRef> target)
{
    QueryTest test{"HiddenFrom", nullptr, nullptr};
    test.filter = [key = std::string(target.name)](Context& ctx, glm::vec3 p) {
        glm::vec3 from;
        return !TargetPosition(ctx, key, from) || !ctx.GetWorld().Navigation().HasLineOfSight(from, p);
    };
    return test;
}
QueryTest VisibleFrom(Key<ObjectRef> target)
{
    QueryTest test{"VisibleFrom", nullptr, nullptr};
    test.filter = [key = std::string(target.name)](Context& ctx, glm::vec3 p) {
        glm::vec3 from;
        return TargetPosition(ctx, key, from) && ctx.GetWorld().Navigation().HasLineOfSight(from, p);
    };
    return test;
}
QueryTest Distance(Key<ObjectRef> target)
{
    QueryTest test{"Distance", nullptr, nullptr};
    test.score = [key = std::string(target.name)](Context& ctx, glm::vec3 p) {
        glm::vec3 from;
        return TargetPosition(ctx, key, from) ? glm::length(Flat(p) - Flat(from)) : 0.0f;
    };
    return test;
}
QueryTest DistanceToSelf()
{
    QueryTest test{"DistanceToSelf", nullptr, nullptr};
    test.score = [](Context& ctx, glm::vec3 p) { return glm::length(Flat(p) - Flat(ctx.Self().Position())); };
    return test;
}
QueryTest Custom(std::string name, std::function<float(Context&, glm::vec3)> score)
{
    if (!score) throw std::invalid_argument("Tests::Custom: empty score");
    return {std::move(name), nullptr, std::move(score)};
}
} // namespace Tests

QueryBuilder::QueryBuilder(std::string name) : query_(std::make_shared<QueryBase>()) { query_->name_ = std::move(name); }
QueryBuilder& QueryBuilder::Points(QueryPoints points)
{
    if (!points.generate) throw std::invalid_argument(query_->name_ + ": empty point generator");
    query_->points_.push_back(std::move(points));
    return *this;
}
QueryBuilder& QueryBuilder::Filter(QueryTest test)
{
    if (!test.filter) throw std::invalid_argument(query_->name_ + ": test \"" + test.name + "\" cannot filter, use Score");
    query_->filters_.push_back(std::move(test));
    return *this;
}
QueryBuilder& QueryBuilder::Score(QueryTest test, Prefer prefer, float weight)
{
    if (!test.score) throw std::invalid_argument(query_->name_ + ": test \"" + test.name + "\" cannot score, use Filter");
    if (!std::isfinite(weight)) throw std::invalid_argument(query_->name_ + ": weight is not finite");
    query_->scores_.push_back({std::move(test), prefer, weight});
    return *this;
}
QueryPtr QueryBuilder::Build()
{
    if (query_->points_.empty()) throw std::invalid_argument(query_->name_ + ": the query has no points; add Points(...)");
    QueryPtr built = query_;
    query_ = std::make_shared<QueryBase>(*query_);
    return built;
}
} // namespace Ai
