// Stage 2: grid, A*, straightening, sight, costs, time-sliced service.
#include "TestSupport.h"
#include <algorithm>

namespace
{
// 20 x 20 metres, 1 m cells. A wall along x = 10 with a 1 m gap at z = 4.
Ai::NavGrid WallWithGap(float gapWidth = 1)
{
    Ai::NavGrid grid;
    grid.SetBounds({0, 0}, {20, 20}, 1);
    grid.BlockBox({10, 0}, {11, 4});
    grid.BlockBox({10, 4 + gapWidth}, {11, 20});
    return grid;
}

// Every straight segment of the path must be walkable for the radius.
bool PathIsWalkable(const Ai::NavGrid& grid, glm::vec2 from, const Ai::PathResult& path, float radius)
{
    glm::vec2 current = from;
    for (const glm::vec2 point : path.points)
    {
        if (!grid.CanWalkStraight(current, point, radius)) return false;
        current = point;
    }
    return true;
}
} // namespace

void NavGridBasicsTests()
{
    Ai::NavGrid grid;
    CHECK(!grid.IsConfigured() && grid.IsWalkable({5, 5}, 10));       // no grid: open field
    CHECK(grid.HasLineOfSight({0, 0, 0}, {100, 0, 100}));
    const uint32_t before = grid.Version();
    grid.SetBounds({-10, -10}, {10, 10}, 0.5f);
    CHECK(grid.Width() == 40 && grid.Height() == 40 && grid.Version() != before);
    CHECK(grid.CellOf({-10, -10}) == glm::ivec2(0, 0) && grid.CellOf({9.9f, 9.9f}) == glm::ivec2(39, 39));
    CHECK(glm::length(grid.CenterOf({0, 0}) - glm::vec2(-9.75f, -9.75f)) < 1e-4f);
    CHECK(grid.CostAt({-1, 0}) == Ai::NavGrid::Blocked);              // outside is blocked
    grid.SetCost({0, 0}, {2, 2}, 7);
    CHECK(grid.CostAt(grid.CellOf({1, 1})) == 7);
    grid.BlockCircle({5, 5}, 1);
    CHECK(!grid.IsWalkable({5, 5}, 0.1f) && grid.IsWalkable({7, 5}, 0.1f));
    grid.ClearBox({4, 4}, {6, 6});
    CHECK(grid.IsWalkable({5, 5}, 0.1f));
    CHECK(Throws([] { Ai::NavGrid g; g.SetBounds({0, 0}, {0, 5}, 1); }, "empty bounds"));
    CHECK(Throws([] { Ai::NavGrid g; g.BlockBox({0, 0}, {1, 1}); }, "SetBounds first"));
}

void NavPathAroundWallTests()
{
    const Ai::NavGrid grid = WallWithGap();
    const glm::vec2 from(3, 15), to(17, 15);
    const Ai::PathResult path = grid.FindPath(from, to, 0.4f);
    CHECK(path.kind == Ai::PathResult::Kind::Found);
    CHECK(glm::length(path.points.back() - to) < 1e-4f);
    CHECK(PathIsWalkable(grid, from, path, 0.4f));
    // The only way is the gap: some corner must be near it.
    CHECK(std::any_of(path.points.begin(), path.points.end(), [](glm::vec2 p) { return p.y > 3 && p.y < 6; }));
    // Straightening leaves only real corners.
    CHECK(path.points.size() <= 4);
    const Ai::PathResult open = grid.FindPath({2, 2}, {8, 18}, 0.4f);
    CHECK(open.kind == Ai::PathResult::Kind::Found && open.points.size() == 1);
}

void NavWideUnitTests()
{
    const Ai::NavGrid grid = WallWithGap(1);
    // A 0.4 m unit fits through the 1 m gap, a 0.7 m one does not and gets as close as it can.
    CHECK(grid.FindPath({3, 15}, {17, 15}, 0.4f).kind == Ai::PathResult::Kind::Found);
    const Ai::PathResult wide = grid.FindPath({3, 15}, {17, 15}, 0.7f);
    CHECK(wide.kind == Ai::PathResult::Kind::Partial);
    CHECK(PathIsWalkable(grid, {3, 15}, wide, 0.7f));
    CHECK(wide.points.back().x < 10);
    const Ai::NavGrid wideGap = WallWithGap(3);
    CHECK(wideGap.FindPath({3, 15}, {17, 15}, 0.7f).kind == Ai::PathResult::Kind::Found);
}

void NavCornerCuttingTests()
{
    // Two blocked cells touching at a corner: the diagonal between them is closed.
    Ai::NavGrid grid;
    grid.SetBounds({0, 0}, {6, 6}, 1);
    grid.BlockBox({0, 3}, {3, 4});   // left half of row 3
    grid.BlockBox({3, 2}, {6, 3});   // right half of row 2
    CHECK(!grid.CanWalkStraight({2.5f, 2.5f}, {3.5f, 3.5f}, 0.1f));
    CHECK(grid.FindPath({0.5f, 0.5f}, {0.5f, 5.5f}, 0.1f).kind != Ai::PathResult::Kind::Found);
    CHECK(!grid.Connected({0.5f, 0.5f}, {0.5f, 5.5f}, 0.1f));
    CHECK(grid.Connected({0.5f, 0.5f}, {5.5f, 0.5f}, 0.1f));
}

void NavUnreachableTests()
{
    Ai::NavGrid grid;
    grid.SetBounds({0, 0}, {20, 20}, 1);
    grid.BlockBox({12, 12}, {18, 13});
    grid.BlockBox({12, 17}, {18, 18});
    grid.BlockBox({12, 12}, {13, 18});
    grid.BlockBox({17, 12}, {18, 18});
    const Ai::PathResult closed = grid.FindPath({2, 2}, {15, 15}, 0.3f);   // the goal is inside a closed room
    CHECK(closed.kind == Ai::PathResult::Kind::Partial);
    CHECK(glm::length(closed.points.back() - glm::vec2(15, 15)) < 6);
    CHECK(!grid.Connected({2, 2}, {15, 15}, 0.3f));
    // From inside the room: as close to the goal as the room allows.
    const Ai::PathResult trapped = grid.FindPath({15, 15}, {2, 2}, 0.3f);
    CHECK(trapped.kind == Ai::PathResult::Kind::Partial);
    CHECK(trapped.points.back().x > 13 && trapped.points.back().y > 13);
    // A single free cell walled in: nowhere to go at all.
    Ai::NavGrid cell;
    cell.SetBounds({0, 0}, {9, 9}, 1);
    cell.BlockBox({3, 3}, {6, 6});
    cell.ClearBox({4, 4}, {5, 5});
    CHECK(cell.FindPath({4.5f, 4.5f}, {0.5f, 0.5f}, 0.3f).kind == Ai::PathResult::Kind::NotFound);
    // Start inside an obstacle: the search begins from the nearest free cell.
    const Ai::PathResult fromWall = grid.FindPath({12.5f, 15}, {2, 2}, 0.3f);
    CHECK(fromWall.kind != Ai::PathResult::Kind::NotFound);
}

void NavCostTests()
{
    Ai::NavGrid grid;
    grid.SetBounds({0, 0}, {30, 10}, 1);
    grid.SetCost({8, 0}, {22, 7}, 50);                // a swamp across most of the field
    const Ai::PathResult path = grid.FindPath({2, 2}, {28, 2}, 0.3f);
    CHECK(path.kind == Ai::PathResult::Kind::Found);
    // The cheap way goes around the swamp through the dry strip at z >= 7.
    CHECK(std::any_of(path.points.begin(), path.points.end(), [](glm::vec2 p) { return p.y >= 7; }));
}

void NavSightTests()
{
    const Ai::NavGrid grid = WallWithGap();
    CHECK(!grid.HasLineOfSight({5, 0, 15}, {15, 0, 15}));   // through the wall
    CHECK(grid.HasLineOfSight({5, 0, 4.5f}, {15, 0, 4.5f})); // through the gap
    CHECK(grid.HasLineOfSight({5, 0, 15}, {5, 0, 2}));
    CHECK(glm::length(grid.NearestWalkable({10.5f, 12}, 0.3f) - glm::vec2(10.5f, 12)) > 0.4f);
}

void NavServiceTests()
{
    const Ai::NavGrid grid = WallWithGap();
    Ai::Detail::PathService service;
    Ai::Stats stats;
    const uint64_t first = service.Request({3, 15}, {17, 15}, 0.4f);
    const uint64_t second = service.Request({3, 3}, {17, 3}, 0.4f);
    const uint64_t cancelled = service.Request({1, 1}, {2, 2}, 0.4f);
    service.Cancel(cancelled);
    Ai::PathResult result;
    int frames = 0;
    while (!service.Take(first, result) && frames < 1000) { service.Process(grid, 10, stats); ++frames; }
    CHECK(frames > 3);                                  // ten nodes per frame: the search took several frames
    CHECK(result.kind == Ai::PathResult::Kind::Found);
    const Ai::PathResult direct = grid.FindPath({3, 15}, {17, 15}, 0.4f);
    CHECK(result.points == direct.points);              // same answer as the synchronous search
    while (!service.Take(second, result) && frames < 2000) { service.Process(grid, 10, stats); ++frames; }
    CHECK(result.kind == Ai::PathResult::Kind::Found);
    service.Process(grid, 100000, stats);
    CHECK(!service.Take(cancelled, result) && service.Queued() == 0);
}

void NavClearanceTests()
{
    Ai::NavGrid grid;
    grid.SetBounds({0, 0}, {10, 10}, 1);
    grid.BlockBox({5, 0}, {6, 10});
    CHECK(grid.ClearanceAt(grid.CellOf({5.5f, 5})) == 0);
    CHECK(std::abs(grid.ClearanceAt(grid.CellOf({4.5f, 5})) - 0.5f) < 1e-4f);   // next to the wall
    CHECK(std::abs(grid.ClearanceAt(grid.CellOf({2.5f, 5})) - 2.5f) < 1e-4f);
    CHECK(std::abs(grid.ClearanceAt(grid.CellOf({0.5f, 5})) - 0.5f) < 1e-4f);   // next to the edge
}
