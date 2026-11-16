#include <Engine/AI/Navigation.h>
#include <Engine/AI/World.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Ai
{
namespace
{
constexpr float Root2 = 1.41421356f;
const glm::ivec2 Directions[8] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
constexpr float Half2 = 0.70710678f;
const glm::vec2 UnitDirections[8] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {Half2, Half2}, {Half2, -Half2}, {-Half2, Half2}, {-Half2, -Half2}};
const float StepCost[8] = {1, 1, 1, 1, Root2, Root2, Root2, Root2};
const uint8_t Opposite[8] = {1, 0, 3, 2, 7, 6, 5, 4};   // Directions[Opposite[d]] == -Directions[d]
constexpr float Infinity = std::numeric_limits<float>::infinity();

float Octile(glm::ivec2 a, glm::ivec2 b)
{
    const float dx = static_cast<float>(std::abs(a.x - b.x)), dy = static_cast<float>(std::abs(a.y - b.y));
    return dx + dy + (Root2 - 2) * std::min(dx, dy);
}

// Every cell the segment touches; at an exact corner both side cells are visited.
template<class F> bool Traverse(const NavGrid& grid, glm::vec2 a, glm::vec2 b, F&& visit)
{
    const float cell = grid.CellSize();
    const glm::vec2 pa = (a - grid.Origin()) / cell, pb = (b - grid.Origin()) / cell;
    glm::ivec2 c(static_cast<int>(std::floor(pa.x)), static_cast<int>(std::floor(pa.y)));
    const glm::ivec2 end(static_cast<int>(std::floor(pb.x)), static_cast<int>(std::floor(pb.y)));
    const glm::vec2 d = pb - pa;
    const int sx = d.x > 0 ? 1 : (d.x < 0 ? -1 : 0), sy = d.y > 0 ? 1 : (d.y < 0 ? -1 : 0);
    float tMaxX = sx ? ((sx > 0 ? std::floor(pa.x) + 1 - pa.x : pa.x - std::floor(pa.x)) / std::abs(d.x)) : Infinity;
    float tMaxY = sy ? ((sy > 0 ? std::floor(pa.y) + 1 - pa.y : pa.y - std::floor(pa.y)) / std::abs(d.y)) : Infinity;
    const float tDeltaX = sx ? 1 / std::abs(d.x) : Infinity, tDeltaY = sy ? 1 / std::abs(d.y) : Infinity;
    if (!visit(c)) return false;
    const int limit = std::abs(end.x - c.x) + std::abs(end.y - c.y) + 4;
    for (int i = 0; i < limit && c != end; ++i)
    {
        if (std::abs(tMaxX - tMaxY) < 1e-6f)
        {
            if (!visit(c + glm::ivec2(sx, 0)) || !visit(c + glm::ivec2(0, sy))) return false;
            c += glm::ivec2(sx, sy);
            tMaxX += tDeltaX;
            tMaxY += tDeltaY;
        }
        else if (tMaxX < tMaxY) { c.x += sx; tMaxX += tDeltaX; }
        else { c.y += sy; tMaxY += tDeltaY; }
        if (!visit(c)) return false;
    }
    return true;
}

bool CanStep(const NavGrid& grid, glm::ivec2 from, int direction, float radius)
{
    const glm::ivec2 d = Directions[direction];
    if (!grid.IsWalkableCell(from + d, radius)) return false;
    // Diagonals never cut a corner.
    return direction < 4 || (grid.IsWalkableCell(from + glm::ivec2(d.x, 0), radius) && grid.IsWalkableCell(from + glm::ivec2(0, d.y), radius));
}

// One A* search over the grid, continued in portions.
struct AStar
{
    std::vector<float> g;
    std::vector<int32_t> parent;
    std::vector<uint32_t> stamp, closed;
    uint32_t current = 0;
    std::vector<std::pair<float, int32_t>> heap;
    glm::vec2 from{0}, to{0}, goalPoint{0};
    float radius = 0;
    int32_t start = -1, goal = -1, best = -1;
    float bestH = Infinity;
    bool done = false, found = false, goalWalkable = false;
    uint32_t expanded = 0;

    void Begin(const NavGrid& grid, glm::vec2 a, glm::vec2 b, float r)
    {
        const size_t cells = static_cast<size_t>(grid.Width()) * static_cast<size_t>(grid.Height());
        if (g.size() != cells) { g.assign(cells, 0); parent.assign(cells, -1); stamp.assign(cells, 0); closed.assign(cells, 0); current = 0; }
        if (++current == 0) { std::fill(stamp.begin(), stamp.end(), 0u); std::fill(closed.begin(), closed.end(), 0u); current = 1; }
        heap.clear();
        from = a; to = b; radius = r; done = found = false; expanded = 0; bestH = Infinity;
        const glm::vec2 startPoint = grid.NearestWalkable(a, r);
        goalPoint = grid.NearestWalkable(b, r);
        const glm::ivec2 sc = grid.CellOf(startPoint), gc = grid.CellOf(goalPoint);
        goalWalkable = grid.IsWalkableCell(gc, r);
        if (!grid.IsWalkableCell(sc, r)) { done = true; start = -1; return; }
        start = static_cast<int32_t>(grid.CellIndex(sc));
        goal = grid.Contains(gc) ? static_cast<int32_t>(grid.CellIndex(gc)) : -1;
        best = start;
        g[start] = 0; parent[start] = -1; stamp[start] = current;
        heap.push_back({Octile(sc, gc), start});
        bestH = Octile(sc, gc);
        (void)gc;
    }

    glm::ivec2 CellAt(const NavGrid& grid, int32_t index) const { return {index % grid.Width(), index / grid.Width()}; }

    bool Step(const NavGrid& grid, uint32_t& budget)
    {
        if (done) return true;
        const auto later = [](const auto& a, const auto& b) { return a.first > b.first; };
        const glm::ivec2 gc = goal >= 0 ? CellAt(grid, goal) : grid.CellOf(goalPoint);
        while (budget > 0 && !heap.empty())
        {
            std::pop_heap(heap.begin(), heap.end(), later);
            const int32_t index = heap.back().second;
            heap.pop_back();
            if (closed[index] == current) continue;
            closed[index] = current;
            --budget;
            ++expanded;
            if (index == goal && goalWalkable) { found = true; done = true; return true; }
            const glm::ivec2 c = CellAt(grid, index);
            const float h = Octile(c, gc);
            if (h < bestH) { bestH = h; best = index; }
            for (int d = 0; d < 8; ++d)
            {
                if (!CanStep(grid, c, d, radius)) continue;
                const glm::ivec2 n = c + Directions[d];
                const int32_t ni = static_cast<int32_t>(grid.CellIndex(n));
                if (closed[ni] == current) continue;
                const float cost = g[index] + StepCost[d] * static_cast<float>(std::max<uint8_t>(1, grid.CostAt(n)));
                if (stamp[ni] != current || cost < g[ni])
                {
                    g[ni] = cost; parent[ni] = index; stamp[ni] = current;
                    heap.push_back({cost + Octile(n, gc), ni});
                    std::push_heap(heap.begin(), heap.end(), later);
                }
            }
        }
        if (heap.empty()) done = true;
        return done;
    }

    PathResult Finish(const NavGrid& grid)
    {
        PathResult result;
        result.expanded = expanded;
        if (start < 0) return result;
        const int32_t target = found ? goal : best;
        if (!found && target == start) { result.kind = PathResult::Kind::NotFound; return result; }
        std::vector<glm::ivec2> cells;
        for (int32_t i = target; i >= 0; i = parent[i]) cells.push_back(CellAt(grid, i));
        std::reverse(cells.begin(), cells.end());
        std::vector<glm::vec2> points;
        for (size_t i = 1; i < cells.size(); ++i)
        {
            const bool last = i + 1 == cells.size();
            if (last || cells[i + 1] - cells[i] != cells[i] - cells[i - 1]) points.push_back(grid.CenterOf(cells[i]));
        }
        if (found)
        {
            if (points.empty()) points.push_back(goalPoint);
            else if (grid.CanWalkStraight(points.size() > 1 ? points[points.size() - 2] : from, goalPoint, radius)) points.back() = goalPoint;
            else points.push_back(goalPoint);
        }
        grid.Straighten(from, points, radius);
        result.points = std::move(points);
        result.kind = found ? PathResult::Kind::Found : PathResult::Kind::Partial;
        return result;
    }
};
} // namespace

// Grid
void NavGrid::SetBounds(glm::vec2 minimum, glm::vec2 maximum, float cellSize)
{
    if (!std::isfinite(minimum.x) || !std::isfinite(minimum.y) || !std::isfinite(maximum.x) || !std::isfinite(maximum.y))
        throw std::invalid_argument("NavGrid::SetBounds: bounds are not finite");
    if (!(cellSize > 0) || !(maximum.x > minimum.x) || !(maximum.y > minimum.y))
        throw std::invalid_argument("NavGrid::SetBounds: empty bounds or cell size");
    const double w = std::ceil((maximum.x - minimum.x) / cellSize), h = std::ceil((maximum.y - minimum.y) / cellSize);
    if (w * h > 16.0 * 1024 * 1024) throw std::invalid_argument("NavGrid::SetBounds: more than 16M cells; use a bigger cell");
    origin_ = minimum;
    cellSize_ = cellSize;
    width_ = static_cast<int>(w);
    height_ = static_cast<int>(h);
    cost_.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_), 1);
    Changed();
}

void NavGrid::Reset()
{
    width_ = height_ = 0;
    cost_.clear();
    clearance_.clear();
    regions_.clear();
    Changed();
}

void NavGrid::Changed() { ++version_; }

template<class F> void NavGrid::ForCells(glm::vec2 minimum, glm::vec2 maximum, F&& f)
{
    if (!IsConfigured()) throw std::logic_error("NavGrid: call SetBounds first");
    const glm::vec2 a = (glm::min(minimum, maximum) - origin_) / cellSize_, b = (glm::max(minimum, maximum) - origin_) / cellSize_;
    const int x0 = std::max(0, static_cast<int>(std::floor(a.x))), y0 = std::max(0, static_cast<int>(std::floor(a.y)));
    const int x1 = std::min(width_ - 1, static_cast<int>(std::ceil(b.x)) - 1), y1 = std::min(height_ - 1, static_cast<int>(std::ceil(b.y)) - 1);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) f(glm::ivec2(x, y));
    Changed();
}

void NavGrid::BlockBox(glm::vec2 minimum, glm::vec2 maximum) { ForCells(minimum, maximum, [&](glm::ivec2 c) { cost_[CellIndex(c)] = Blocked; }); }
void NavGrid::ClearBox(glm::vec2 minimum, glm::vec2 maximum) { ForCells(minimum, maximum, [&](glm::ivec2 c) { cost_[CellIndex(c)] = 1; }); }
void NavGrid::SetCost(glm::vec2 minimum, glm::vec2 maximum, uint8_t cost)
{
    const uint8_t value = std::max<uint8_t>(1, cost);
    ForCells(minimum, maximum, [&](glm::ivec2 c) { cost_[CellIndex(c)] = value; });
}
void NavGrid::BlockCircle(glm::vec2 center, float radius)
{
    ForCells(center - glm::vec2(radius), center + glm::vec2(radius), [&](glm::ivec2 c) {
        const glm::vec2 low = origin_ + glm::vec2(c) * cellSize_;
        const glm::vec2 nearest = glm::clamp(center, low, low + glm::vec2(cellSize_));
        if (glm::length(nearest - center) <= radius) cost_[CellIndex(c)] = Blocked;
    });
}

glm::ivec2 NavGrid::CellOf(glm::vec2 point) const
{
    const glm::vec2 p = (point - origin_) / cellSize_;
    return {static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y))};
}
glm::vec2 NavGrid::CenterOf(glm::ivec2 cell) const { return origin_ + (glm::vec2(cell) + 0.5f) * cellSize_; }
uint8_t NavGrid::CostAt(glm::ivec2 cell) const { return Contains(cell) ? cost_[CellIndex(cell)] : Blocked; }

void NavGrid::UpdateClearance() const
{
    if (clearanceVersion_ == version_ && clearance_.size() == cost_.size()) return;
    const size_t cells = cost_.size();
    std::vector<float> d(cells, Infinity);
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x)
        {
            const size_t i = CellIndex({x, y});
            if (cost_[i] == Blocked) d[i] = 0;
            else if (x == 0 || y == 0 || x == width_ - 1 || y == height_ - 1) d[i] = 1; // outside is blocked
        }
    // Two-pass chamfer distance (1, sqrt 2): close enough to Euclidean for clearance.
    auto relax = [&](int x, int y, int dx, int dy, float step) {
        const glm::ivec2 n(x + dx, y + dy);
        if (!Contains(n)) return;
        float& value = d[CellIndex({x, y})];
        value = std::min(value, d[CellIndex(n)] + step);
    };
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x)
        { relax(x, y, -1, 0, 1); relax(x, y, 0, -1, 1); relax(x, y, -1, -1, Root2); relax(x, y, 1, -1, Root2); }
    for (int y = height_ - 1; y >= 0; --y)
        for (int x = width_ - 1; x >= 0; --x)
        { relax(x, y, 1, 0, 1); relax(x, y, 0, 1, 1); relax(x, y, 1, 1, Root2); relax(x, y, -1, 1, Root2); }
    clearance_.resize(cells);
    for (size_t i = 0; i < cells; ++i) clearance_[i] = cost_[i] == Blocked ? 0 : std::max(0.0f, d[i] - 0.5f) * cellSize_;
    clearanceVersion_ = version_;
}

float NavGrid::ClearanceAt(glm::ivec2 cell) const
{
    if (!Contains(cell)) return 0;
    UpdateClearance();
    return clearance_[CellIndex(cell)];
}

bool NavGrid::IsWalkableCell(glm::ivec2 cell, float radius) const
{
    if (!Contains(cell)) return false;
    UpdateClearance();
    const size_t i = CellIndex(cell);
    return cost_[i] != Blocked && clearance_[i] + 1e-4f >= radius;
}

bool NavGrid::IsWalkable(glm::vec2 point, float radius) const { return !IsConfigured() || IsWalkableCell(CellOf(point), radius); }

bool NavGrid::HasLineOfSight(glm::vec3 from, glm::vec3 to) const
{
    if (!IsConfigured()) return true;
    return Traverse(*this, {from.x, from.z}, {to.x, to.z}, [&](glm::ivec2 c) { return CostAt(c) != Blocked; });
}

bool NavGrid::CanWalkStraight(glm::vec2 from, glm::vec2 to, float radius) const
{
    if (!IsConfigured()) return true;
    return Traverse(*this, from, to, [&](glm::ivec2 c) { return IsWalkableCell(c, radius); });
}

glm::vec2 NavGrid::NearestWalkable(glm::vec2 point, float radius, float searchRadius) const
{
    if (!IsConfigured()) return point;
    const glm::ivec2 center = CellOf(point);
    if (IsWalkableCell(center, radius)) return point;
    const int rings = std::max(1, static_cast<int>(std::ceil(searchRadius / cellSize_)));
    for (int k = 1; k <= rings; ++k)
    {
        float bestDistance = Infinity;
        glm::vec2 best = point;
        for (int y = -k; y <= k; ++y)
            for (int x = -k; x <= k; ++x)
            {
                if (std::max(std::abs(x), std::abs(y)) != k) continue;
                const glm::ivec2 c = center + glm::ivec2(x, y);
                if (!IsWalkableCell(c, radius)) continue;
                const float distance = glm::length(CenterOf(c) - point);
                if (distance < bestDistance) { bestDistance = distance; best = CenterOf(c); }
            }
        if (bestDistance < Infinity) return best;
    }
    return point;
}

bool NavGrid::Connected(glm::vec2 from, glm::vec2 to, float radius) const
{
    if (!IsConfigured()) return true;
    if (regionsVersion_ != version_ || regionsRadius_ != radius || regions_.size() != cost_.size())
    {
        regions_.assign(cost_.size(), -1);
        int32_t label = 0;
        std::vector<glm::ivec2> stack;
        for (int y = 0; y < height_; ++y)
            for (int x = 0; x < width_; ++x)
            {
                if (regions_[CellIndex({x, y})] >= 0 || !IsWalkableCell({x, y}, radius)) continue;
                regions_[CellIndex({x, y})] = label;
                stack.assign(1, {x, y});
                while (!stack.empty())
                {
                    const glm::ivec2 c = stack.back();
                    stack.pop_back();
                    for (int d = 0; d < 8; ++d)
                    {
                        if (!CanStep(*this, c, d, radius)) continue;
                        const glm::ivec2 n = c + Directions[d];
                        if (regions_[CellIndex(n)] >= 0) continue;
                        regions_[CellIndex(n)] = label;
                        stack.push_back(n);
                    }
                }
                ++label;
            }
        regionsVersion_ = version_;
        regionsRadius_ = radius;
    }
    const glm::ivec2 a = CellOf(NearestWalkable(from, radius)), b = CellOf(NearestWalkable(to, radius));
    if (!IsWalkableCell(a, radius) || !IsWalkableCell(b, radius)) return false;
    return regions_[CellIndex(a)] == regions_[CellIndex(b)];
}

PathResult NavGrid::FindPath(glm::vec2 from, glm::vec2 to, float radius, uint32_t maxNodes) const
{
    if (!IsConfigured())
    {
        PathResult direct;
        direct.kind = PathResult::Kind::Found;
        direct.points.push_back(to);
        return direct;
    }
    AStar search;
    search.Begin(*this, from, to, radius);
    uint32_t budget = maxNodes;
    search.Step(*this, budget);
    return search.Finish(*this);
}

void NavGrid::Straighten(glm::vec2 from, std::vector<glm::vec2>& points, float radius) const
{
    if (points.size() < 2) return;
    // A shortcut may not cross cells dearer than its ends: the search went around a swamp for a reason.
    const auto cheapStraight = [&](glm::vec2 a, glm::vec2 b) {
        if (!IsConfigured()) return true;
        const uint8_t limit = std::max(CostAt(CellOf(a)), CostAt(CellOf(b)));
        return Traverse(*this, a, b, [&](glm::ivec2 c) { return IsWalkableCell(c, radius) && CostAt(c) <= limit; });
    };
    std::vector<glm::vec2> result;
    glm::vec2 current = from;
    for (size_t i = 0; i < points.size();)
    {
        size_t j = i;
        while (j + 1 < points.size() && cheapStraight(current, points[j + 1])) ++j;
        result.push_back(points[j]);
        current = points[j];
        i = j + 1;
    }
    points = std::move(result);
}

// Neighbours
void SpatialHash::Build(float cellSize)
{
    const std::vector<Entry>& entries = input_;
    // The area: finite positions only, so one broken entry cannot stretch the grid.
    glm::vec2 low(std::numeric_limits<float>::max()), high(-std::numeric_limits<float>::max());
    for (const Entry& e : entries)
        if (std::isfinite(e.position.x) && std::isfinite(e.position.y)) { low = glm::min(low, e.position); high = glm::max(high, e.position); }
    if (low.x > high.x) low = high = glm::vec2(0);
    float cell = cellSize > 0 ? cellSize : 4.0f;
    const double maxCells = std::max(4096.0, 4.0 * static_cast<double>(entries.size()));
    for (;;)
    {
        width_ = static_cast<int>((high.x - low.x) / cell) + 1;
        height_ = static_cast<int>((high.y - low.y) / cell) + 1;
        if (static_cast<double>(width_) * height_ <= maxCells) break;
        cell *= 2;
    }
    origin_ = low;
    inverseCell_ = 1.0f / cell;
    const size_t cells = static_cast<size_t>(width_) * height_;
    start_.assign(cells + 1, 0);
    cellOf_.resize(entries.size());
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const uint32_t c = static_cast<uint32_t>(Row(entries[i].position.y) * width_ + Column(entries[i].position.x));
        cellOf_[i] = c;
        ++start_[c + 1];
    }
    for (size_t i = 1; i < start_.size(); ++i) start_[i] += start_[i - 1];
    fill_.assign(start_.begin(), start_.end() - 1);
    entries_.resize(entries.size());
    positions_.resize(entries.size());
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const uint32_t k = fill_[cellOf_[i]]++;
        entries_[k] = entries[i];
        positions_[k] = entries[i].position;
    }
}

namespace Detail
{
// Paths
struct PathService::Search
{
    uint64_t id = 0;
    bool active = false;
    AStar astar;
};

PathService::PathService() : search_(std::make_unique<Search>()) {}
PathService::~PathService() = default;

uint64_t PathService::Request(glm::vec2 from, glm::vec2 to, float radius)
{
    queue_.push_back({++next_, from, to, radius});
    return next_;
}

void PathService::Cancel(uint64_t id)
{
    queue_.erase(std::remove_if(queue_.begin(), queue_.end(), [&](const Request_& r) { return r.id == id; }), queue_.end());
    if (search_->active && search_->id == id) search_->active = false;
    done_.erase(std::remove_if(done_.begin(), done_.end(), [&](const auto& d) { return d.first == id; }), done_.end());
}

bool PathService::Take(uint64_t id, PathResult& result)
{
    const auto found = std::find_if(done_.begin(), done_.end(), [&](const auto& d) { return d.first == id; });
    if (found == done_.end()) return false;
    result = std::move(found->second);
    done_.erase(found);
    return true;
}

void PathService::Process(const NavGrid& grid, uint32_t nodeBudget, Stats& stats)
{
    uint32_t budget = std::max<uint32_t>(nodeBudget, 1);
    const uint32_t start = budget;
    while (budget > 0)
    {
        if (!search_->active)
        {
            if (queue_.empty()) break;
            const Request_ request = queue_.front();
            queue_.pop_front();
            search_->id = request.id;
            search_->active = true;
            search_->astar.Begin(grid, request.from, request.to, request.radius);
        }
        if (search_->astar.Step(grid, budget))
        {
            done_.push_back({search_->id, search_->astar.Finish(grid)});
            search_->active = false;
            ++stats.pathsFinished;
        }
    }
    stats.pathNodes += start - budget;
}

void PathService::Clear()
{
    queue_.clear();
    done_.clear();
    search_->active = false;
}

// Flow fields
bool FlowField::Reaches(const NavGrid& grid, glm::vec2 point) const
{
    const glm::ivec2 c = grid.CellOf(point);
    return ready && Contains(c) && direction[Index(c)] != 255;
}

glm::vec2 FlowField::Sample(const NavGrid& grid, glm::vec2 point) const
{
    if (!ready) return glm::vec2(0);
    const glm::ivec2 c = grid.CellOf(point);
    if (!Contains(c)) return glm::vec2(0);
    const uint8_t own = direction[Index(c)];
    if (own == 8 || own == 255) return glm::vec2(0);
    // Blend the directions of the four nearest cells so soldiers do not jerk at cell borders.
    const glm::vec2 p = (point - grid.Origin()) / grid.CellSize() - 0.5f;
    const glm::ivec2 base(static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)));
    const glm::vec2 t = p - glm::vec2(base);
    glm::vec2 sum(0);
    for (int y = 0; y <= 1; ++y)
        for (int x = 0; x <= 1; ++x)
        {
            const glm::ivec2 n = base + glm::ivec2(x, y);
            if (!Contains(n)) continue;
            const uint8_t code = direction[Index(n)];
            if (code > 7) continue;
            const float w = (x ? t.x : 1 - t.x) * (y ? t.y : 1 - t.y);
            sum += UnitDirections[code] * w;
        }
    if (glm::length(sum) < 1e-3f) sum = glm::vec2(Directions[own]);
    return glm::normalize(sum);
}

struct FlowFieldCache::Build
{
    std::shared_ptr<FlowField> field;
    std::vector<std::pair<float, int32_t>> heap;
    std::vector<uint8_t> closed;
    std::vector<uint8_t> walkable;   // per window cell: 0 not asked yet, 1 walkable, 2 not; each cell is asked once
    bool started = false;
};

FlowFieldCache::FlowFieldCache() = default;
FlowFieldCache::~FlowFieldCache() = default;

size_t FlowFieldCache::Cells() const
{
    size_t cells = 0;
    for (const auto& field : fields_) cells += field->direction.size();
    return cells;
}

std::shared_ptr<const FlowField> FlowFieldCache::Request(const NavGrid& grid, glm::vec2 goal, float radius,
                                                         glm::vec2 areaMin, glm::vec2 areaMax, float margin)
{
    return Request(grid, goal, radius, areaMin, areaMax, margin, -1);
}

std::shared_ptr<const FlowField> FlowFieldCache::Request(const NavGrid& grid, glm::vec2 goal, float radius,
                                                         glm::vec2 areaMin, glm::vec2 areaMax, float margin, float goalTolerance)
{
    const float step = 0.5f * grid.CellSize();
    const float classRadius = std::ceil(std::max(radius, 0.01f) / step) * step;
    const auto coarse = [&](glm::vec2 p) { return glm::ivec2(glm::floor((p - grid.Origin()) / coarseCell)); };
    const glm::vec2 snapped = grid.NearestWalkable(goal, classRadius);
    // The window: the area, the goal and a margin around them, inside the grid.
    const glm::ivec2 last(grid.Width() - 1, grid.Height() - 1);
    glm::ivec2 low(0), high = last;
    if (margin >= 0)
    {
        const glm::vec2 a = glm::min(glm::min(areaMin, areaMax), snapped) - glm::vec2(margin);
        const glm::vec2 b = glm::max(glm::max(areaMin, areaMax), snapped) + glm::vec2(margin);
        low = glm::clamp(grid.CellOf(a), glm::ivec2(0), last);
        high = glm::clamp(grid.CellOf(b), glm::ivec2(0), last);
    }
    ++useClock_;
    for (size_t i = 0; i < fields_.size(); ++i)
    {
        const auto& f = fields_[i];
        if (f->version != grid.Version() || f->radius != classRadius || coarse(f->goal) != coarse(goal)) continue;
        if (goalTolerance >= 0 && glm::length(f->goal - snapped) > goalTolerance) continue;
        // Sharing a coarse cell must not redirect an order to the other side of a wall.
        if (!grid.CanWalkStraight(f->goal, snapped, classRadius)) continue;
        const bool covers = f->low.x <= low.x && f->low.y <= low.y && f->low.x + f->size.x > high.x && f->low.y + f->size.y > high.y;
        if (!covers)
        {
            // One order to several squads in one frame: a field nobody started yet grows to cover them all.
            const auto build = std::find_if(builds_.begin(), builds_.end(), [&](const auto& b) { return b->field == f && !b->started; });
            if (build == builds_.end()) continue;
            const glm::ivec2 joinedLow = glm::min(f->low, low), joinedHigh = glm::max(f->low + f->size - glm::ivec2(1), high);
            f->low = joinedLow;
            f->size = joinedHigh - joinedLow + glm::ivec2(1);
            const size_t cells = static_cast<size_t>(f->size.x) * static_cast<size_t>(f->size.y);
            f->direction.assign(cells, 255);
            f->distance.assign(cells, Infinity);
        }
        lastUse_[i] = useClock_;
        return f;
    }
    auto field = std::make_shared<FlowField>();
    field->goal = snapped;
    field->goalCell = grid.CellOf(field->goal);
    field->radius = classRadius;
    field->version = grid.Version();
    field->low = low;
    field->size = high - low + glm::ivec2(1);
    const size_t cells = static_cast<size_t>(field->size.x) * static_cast<size_t>(field->size.y);
    field->direction.assign(cells, 255);
    field->distance.assign(cells, Infinity);
    fields_.push_back(field);
    lastUse_.push_back(useClock_);
    auto build = std::make_shared<Build>();
    build->field = field;
    builds_.push_back(build);
    // Forget the least recently used fields that nobody holds.
    while (fields_.size() > capacity)
    {
        size_t victim = fields_.size();
        for (size_t i = 0; i < fields_.size(); ++i)
            if (fields_[i].use_count() == 1 && fields_[i] != field && (victim == fields_.size() || lastUse_[i] < lastUse_[victim])) victim = i;
        if (victim == fields_.size()) break;
        builds_.erase(std::remove_if(builds_.begin(), builds_.end(), [&](const auto& b) { return b->field == fields_[victim]; }), builds_.end());
        fields_.erase(fields_.begin() + static_cast<std::ptrdiff_t>(victim));
        lastUse_.erase(lastUse_.begin() + static_cast<std::ptrdiff_t>(victim));
    }
    return field;
}

void FlowFieldCache::Process(const NavGrid& grid, uint32_t cellBudget, Stats& stats)
{
    uint32_t budget = std::max<uint32_t>(cellBudget, 1);
    const auto later = [](const auto& a, const auto& b) { return a.first > b.first; };
    while (budget > 0 && !builds_.empty())
    {
        Build& build = *builds_.front();
        FlowField& field = *build.field;
        if (field.version != grid.Version()) { builds_.pop_front(); continue; } // stale: the next request builds a new one
        const size_t cells = field.distance.size();
        if (!build.started)
        {
            build.started = true;
            build.closed.assign(cells, 0);
            build.walkable.assign(cells, 0);
            if (field.Contains(field.goalCell) && grid.IsWalkableCell(field.goalCell, field.radius))
            {
                const size_t g = field.Index(field.goalCell);
                field.distance[g] = 0;
                field.direction[g] = 8;
                build.heap.push_back({0.0f, static_cast<int32_t>(g)});
            }
        }
        const auto walkable = [&](glm::ivec2 cell) {
            uint8_t& known = build.walkable[field.Index(cell)];
            if (!known) known = grid.IsWalkableCell(cell, field.radius) ? 1 : 2;
            return known == 1;
        };
        // CanStep with the answers cached; the corner cells of a diagonal lie inside the window
        // whenever both ends do.
        const auto canStep = [&](glm::ivec2 from, int d) {
            const glm::ivec2 step = Directions[d];
            if (!walkable(from + step)) return false;
            return d < 4 || (walkable(from + glm::ivec2(step.x, 0)) && walkable(from + glm::ivec2(0, step.y)));
        };
        while (budget > 0 && !build.heap.empty())
        {
            std::pop_heap(build.heap.begin(), build.heap.end(), later);
            const auto [distance, index] = build.heap.back();
            build.heap.pop_back();
            if (build.closed[static_cast<size_t>(index)]) continue;
            build.closed[static_cast<size_t>(index)] = 1;
            --budget;
            ++stats.fieldCells;
            const glm::ivec2 c = field.low + glm::ivec2(index % field.size.x, index / field.size.x);
            const float cost = static_cast<float>(std::max<uint8_t>(1, grid.CostAt(c)));
            for (int d = 0; d < 8; ++d)
            {
                const glm::ivec2 n = c + Directions[d];
                if (!field.Contains(n) || !canStep(c, d)) continue;
                const size_t ni = field.Index(n);
                // Reverse step from n back to c: the same cost because CanStep is symmetric.
                const float next = distance + StepCost[d] * cost;
                if (next < field.distance[ni])
                {
                    // n is reached through c: its arrow points back to c. The last improvement is the best one.
                    field.distance[ni] = next;
                    field.direction[ni] = Opposite[d];
                    build.heap.push_back({next, static_cast<int32_t>(ni)});
                    std::push_heap(build.heap.begin(), build.heap.end(), later);
                }
            }
        }
        if (!build.heap.empty()) break;
        // Wave finished. The arrows were set while it spread, so there is no pass over the window
        // outside the budget.
        field.ready = true;
        ++stats.fieldsFinished;
        build.closed = {};
        build.walkable = {};
        builds_.pop_front();
    }
}

void FlowFieldCache::Clear()
{
    fields_.clear();
    builds_.clear();
    lastUse_.clear();
}
} // namespace Detail
} // namespace Ai
