#pragma once
#include <Engine/AI/AiTypes.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Ai
{
class Context;

/** Candidate points around the querier, like an EQS generator. */
struct QueryPoints
{
    std::string name;
    std::function<void(Context&, std::vector<glm::vec3>&)> generate;
};

/** A filter (keep or drop a point) and/or a score (bigger value, depending on Prefer). */
struct QueryTest
{
    std::string name;
    std::function<bool(Context&, glm::vec3)> filter;
    std::function<float(Context&, glm::vec3)> score;
};

enum class Prefer { Near, Far };

struct QueryItem { glm::vec3 point{0}; float score = 0; };

/** Built query, shared by every unit. Small and synchronous: at most MaxPoints candidates. */
class QueryBase
{
public:
    static constexpr size_t MaxPoints = 1024;
    const std::string& Name() const { return name_; }
    /** Surviving points, best first; empty when every point was filtered out. */
    std::vector<QueryItem> Run(Context& ctx) const;

private:
    friend class QueryBuilder;
    std::string name_;
    std::vector<QueryPoints> points_;
    std::vector<QueryTest> filters_;
    struct Scorer { QueryTest test; Prefer prefer; float weight; };
    std::vector<Scorer> scores_;
};
using QueryPtr = std::shared_ptr<const QueryBase>;

namespace Generators
{
/** count points on rings between minRadius and maxRadius around the querier. */
QueryPoints Ring(float minRadius, float maxRadius, int count);
/** Square grid around the querier. */
QueryPoints Grid(float halfSize, float step);
/** Ring around an object from memory instead of the querier. */
QueryPoints Around(Key<ObjectRef> target, float minRadius, float maxRadius, int count);
}

namespace Tests
{
/** The querier can walk to the point. */
QueryTest Reachable();
/** The target in memory cannot see the point. Passes when the target is unknown. */
QueryTest HiddenFrom(Key<ObjectRef> target);
QueryTest VisibleFrom(Key<ObjectRef> target);
/** Score: distance from the point to the target in memory. */
QueryTest Distance(Key<ObjectRef> target);
/** Score: distance from the point to the querier. */
QueryTest DistanceToSelf();
QueryTest Custom(std::string name, std::function<float(Context&, glm::vec3)> score);
}

class QueryBuilder
{
public:
    explicit QueryBuilder(std::string name);
    QueryBuilder& Points(QueryPoints points);
    QueryBuilder& Filter(QueryTest test);
    QueryBuilder& Score(QueryTest test, Prefer prefer, float weight = 1.0f);
    /** Throws std::invalid_argument when the query has no points or a test lacks the needed part. */
    QueryPtr Build();

private:
    std::shared_ptr<QueryBase> query_;
};
} // namespace Ai
