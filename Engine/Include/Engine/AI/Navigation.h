#pragma once
#include <Engine/AI/AiTypes.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

namespace Ai
{
struct Stats;

struct PathResult
{
    enum class Kind { Found, Partial, NotFound };
    Kind kind = Kind::NotFound;
    std::vector<glm::vec2> points;   // corner points after straightening, without the start
    uint32_t expanded = 0;
};

/**
 * Flat walkability grid on the XZ plane, one level, no floors. Each cell keeps a cost
 * (1 normal, up to 254 expensive, 255 blocked) and the distance to the nearest obstacle,
 * so a wide unit does not squeeze through a narrow gap. Outside the bounds is blocked.
 */
class NavGrid
{
public:
    static constexpr uint8_t Blocked = 255;
    /** Resets the grid: every cell walkable with cost 1. */
    void SetBounds(glm::vec2 minimum, glm::vec2 maximum, float cellSize);
    void Reset();
    bool IsConfigured() const { return width_ > 0; }
    void BlockBox(glm::vec2 minimum, glm::vec2 maximum);
    void BlockCircle(glm::vec2 center, float radius);
    /** Swamp, road, slope: walkable but more or less expensive. 255 blocks. */
    void SetCost(glm::vec2 minimum, glm::vec2 maximum, uint8_t cost);
    /** Makes the area walkable again with cost 1 (a destroyed building). */
    void ClearBox(glm::vec2 minimum, glm::vec2 maximum);
    /** Grows with every change; paths and fields made for an older version are rechecked. */
    uint32_t Version() const { return version_; }

    float CellSize() const { return cellSize_; }
    int Width() const { return width_; }
    int Height() const { return height_; }
    glm::vec2 Origin() const { return origin_; }
    glm::ivec2 CellOf(glm::vec2 point) const;
    glm::vec2 CenterOf(glm::ivec2 cell) const;
    bool Contains(glm::ivec2 cell) const { return cell.x >= 0 && cell.y >= 0 && cell.x < width_ && cell.y < height_; }
    uint8_t CostAt(glm::ivec2 cell) const;
    /** Metres from the cell centre to the nearest blocked cell or the edge of the grid. */
    float ClearanceAt(glm::ivec2 cell) const;
    bool IsWalkableCell(glm::ivec2 cell, float radius) const;
    bool IsWalkable(glm::vec2 point, float radius) const;
    /** Sight: only blocked cells stop it. Heights are ignored. Without a grid everything is visible. */
    bool HasLineOfSight(glm::vec3 from, glm::vec3 to) const;
    /** Movement: every crossed cell must be walkable for this radius. */
    bool CanWalkStraight(glm::vec2 from, glm::vec2 to, float radius) const;
    /** Nearest walkable point, or the point itself when nothing is found within searchRadius. */
    glm::vec2 NearestWalkable(glm::vec2 point, float radius, float searchRadius = 16) const;
    /** Same connected area for this radius: Ai::Tests::Reachable without a full search. */
    bool Connected(glm::vec2 from, glm::vec2 to, float radius) const;
    /** Synchronous A*. The world uses the time-sliced PathService instead. */
    PathResult FindPath(glm::vec2 from, glm::vec2 to, float radius, uint32_t maxNodes = 1u << 24) const;
    /** Removes corners that can be cut while staying walkable. */
    void Straighten(glm::vec2 from, std::vector<glm::vec2>& points, float radius) const;
    size_t CellIndex(glm::ivec2 cell) const { return static_cast<size_t>(cell.y) * static_cast<size_t>(width_) + static_cast<size_t>(cell.x); }

private:
    void Changed();
    void UpdateClearance() const;
    template<class F> void ForCells(glm::vec2 minimum, glm::vec2 maximum, F&& f);
    glm::vec2 origin_{0};
    float cellSize_ = 1;
    int width_ = 0, height_ = 0;
    uint32_t version_ = 1;
    std::vector<uint8_t> cost_;
    mutable std::vector<float> clearance_;
    mutable uint32_t clearanceVersion_ = 0;
    mutable std::vector<int32_t> regions_;
    mutable uint32_t regionsVersion_ = 0;
    mutable float regionsRadius_ = -1;
};

/**
 * Neighbours within a radius, rebuilt once per frame. Used by senses and by avoidance.
 * A dense grid over the area the entries occupy: the entries of one row of cells lie in memory one
 * after another, so a query reads a few runs instead of jumping around. The storage is kept between
 * frames, so a rebuild allocates nothing once the crowd size is reached.
 */
class SpatialHash
{
public:
    struct Entry
    {
        glm::vec2 position{0};
        float radius = 0;
        ComponentId object = 0;     // scene object; 0 for squad soldiers
        uint32_t controller = 0;    // serial of the controller, 0 for plain targets
        uint32_t member = 0;        // soldier index inside a group
        Team team{};
        bool settled = false;       // standing still: an obstacle that is not pushed
        glm::vec2 velocity{0};      // last frame, for predicting where it goes
    };
    /** A new frame: Add the entries, then Build. Queries see the previous Build until then. */
    void Begin() { input_.clear(); }
    void Add(const Entry& entry) { input_.push_back(entry); }
    /** cellSize is a wish: a few far-away entries make the cells larger instead of the grid huge. */
    void Build(float cellSize);
    template<class F> void Query(glm::vec2 center, float radius, F&& visit) const
    {
        if (entries_.empty()) return;
        const float radiusSquared = radius * radius;
        const int x0 = Column(center.x - radius), x1 = Column(center.x + radius);
        const int y0 = Row(center.y - radius), y1 = Row(center.y + radius);
        for (int y = y0; y <= y1; ++y)
        {
            const size_t row = static_cast<size_t>(y) * width_;
            for (uint32_t i = start_[row + x0], end = start_[row + x1 + 1]; i < end; ++i)   // one run per row
            {
                const glm::vec2 delta = positions_[i] - center;
                if (glm::dot(delta, delta) <= radiusSquared) visit(entries_[i]);
            }
        }
    }
    /** All entries, row by row. */
    const std::vector<Entry>& Entries() const { return entries_; }

private:
    // max(0, min(v, last)) and not clamp: it also turns NaN into 0.
    int Column(float x) const { return static_cast<int>(std::max(0.0f, std::min((x - origin_.x) * inverseCell_, static_cast<float>(width_ - 1)))); }
    int Row(float y) const { return static_cast<int>(std::max(0.0f, std::min((y - origin_.y) * inverseCell_, static_cast<float>(height_ - 1)))); }
    std::vector<Entry> entries_, input_;
    std::vector<glm::vec2> positions_;   // entries_[i].position, packed: the distance test reads only these
    std::vector<uint32_t> start_, cellOf_, fill_;
    glm::vec2 origin_{0};
    float inverseCell_ = 0.25f;
    int width_ = 1, height_ = 1;
};

namespace Detail
{
/**
 * Desired velocity bent away from neighbours. Two parts: a push out of contact, and a dodge from
 * where a neighbour will be within the next second (its velocity against ours). Exactly head-on,
 * each unit steps to its own right, so two units walking towards each other pass instead of locking.
 * skip(entry) is true for the unit itself.
 */
template<class Skip>
glm::vec2 Steer(const SpatialHash& hash, glm::vec2 position, glm::vec2 desired, float speed, float radius, float largest, Skip&& skip)
{
    constexpr float horizon = 1.0f;
    const float length = glm::length(desired);
    if (length < 1e-5f || speed <= 0) return desired;
    const glm::vec2 heading = desired / length, right(heading.y, -heading.x);
    glm::vec2 push(0), dodge(0);
    hash.Query(position, radius + largest + speed * horizon + 0.5f, [&](const SpatialHash::Entry& other) {
        if (skip(other)) return;
        const glm::vec2 offset = other.position - position;
        const float distance = glm::length(offset);
        const float minimum = radius + other.radius + 0.1f;
        if (distance < minimum + 0.1f)
            push += (distance > 1e-4f ? -offset / distance : right) * ((minimum + 0.1f - distance) / minimum);
        const glm::vec2 relative = other.velocity - desired;
        const float rr = glm::dot(relative, relative);
        if (rr < 1e-6f) return;
        const float t = -glm::dot(offset, relative) / rr;
        if (t <= 0 || t > horizon) return;
        const glm::vec2 closest = offset + relative * t;
        const float miss = glm::length(closest);
        if (miss >= minimum) return;
        const glm::vec2 away = miss > 1e-3f ? -closest / miss : right;
        dodge += away * (1 - t / horizon) * ((minimum - miss) / minimum);
    });
    glm::vec2 velocity = desired + (push * 1.5f + dodge) * speed;
    if (glm::length(velocity) > speed) velocity = glm::normalize(velocity) * speed;
    return velocity;
}

/** Time-sliced A*: one search at a time, continued over frames within the node budget. */
class PathService
{
public:
    PathService();
    ~PathService();
    uint64_t Request(glm::vec2 from, glm::vec2 to, float radius);
    void Cancel(uint64_t id);
    /** True and the result when the request is finished. */
    bool Take(uint64_t id, PathResult& result);
    void Process(const NavGrid& grid, uint32_t nodeBudget, Stats& stats);
    void Clear();
    size_t Queued() const { return queue_.size(); }

private:
    struct Request_ { uint64_t id; glm::vec2 from, to; float radius; };
    struct Search;
    std::deque<Request_> queue_;
    std::vector<std::pair<uint64_t, PathResult>> done_;
    std::unique_ptr<Search> search_;
    uint64_t next_ = 0;
};

/**
 * Direction field to one goal, for a crowd. Built by a Dijkstra wave, in portions, and only
 * inside a window around the squad and its goal: a march costs the cells it can use, not the map.
 */
class FlowField
{
public:
    glm::vec2 goal{0};
    glm::ivec2 goalCell{0};
    float radius = 0;
    uint32_t version = 0;
    bool ready = false;
    glm::ivec2 low{0}, size{0};       // the window, in grid cells
    std::vector<uint8_t> direction;   // 0..7 neighbour, 8 = goal, 255 = unreachable or outside
    std::vector<float> distance;
    bool Contains(glm::ivec2 cell) const { return cell.x >= low.x && cell.y >= low.y && cell.x < low.x + size.x && cell.y < low.y + size.y; }
    size_t Index(glm::ivec2 cell) const { return static_cast<size_t>(cell.y - low.y) * static_cast<size_t>(size.x) + static_cast<size_t>(cell.x - low.x); }
    /** Interpolated direction at a point; zero when unreachable, outside the window or at the goal. */
    glm::vec2 Sample(const class NavGrid& grid, glm::vec2 point) const;
    /** The point is inside the window and the goal can be reached from it. */
    bool Reaches(const class NavGrid& grid, glm::vec2 point) const;
};

class FlowFieldCache
{
public:
    FlowFieldCache();
    ~FlowFieldCache();
    /**
     * Shared field for this goal that covers the area [areaMin, areaMax] plus margin; it may not be
     * ready yet. Goals are rounded to coarse cells, so close goals share one field. margin < 0 covers the whole grid.
     */
    std::shared_ptr<const FlowField> Request(const NavGrid& grid, glm::vec2 goal, float radius,
                                             glm::vec2 areaMin, glm::vec2 areaMax, float margin = 64);
    std::shared_ptr<const FlowField> Request(const NavGrid& grid, glm::vec2 goal, float radius) { return Request(grid, goal, radius, goal, goal, -1); }
    /** Cells held by every cached field. */
    size_t Cells() const;
    void Process(const NavGrid& grid, uint32_t cellBudget, Stats& stats);
    void Clear();
    size_t Size() const { return fields_.size(); }
    float coarseCell = 16;
    size_t capacity = 16;

private:
    struct Build;
    std::vector<std::shared_ptr<FlowField>> fields_;
    std::deque<std::shared_ptr<Build>> builds_;
    uint64_t useClock_ = 0;
    std::vector<uint64_t> lastUse_;
};
} // namespace Detail
} // namespace Ai
