#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>
#include <glm/glm.hpp>
#include <DirectXMath.h>

namespace Trace
{
enum class Status
{
    Success,
    Hit,
    Miss,
    InvalidRay,
    InvalidQuery,
    InvalidViewport,
    OutsideViewport,
    InvalidMatrix,
    NoCamera,
    CursorUnavailable,
    InputBlocked
};
enum class Geometry
{
    Exact,
    Bounds,
    EditorSelection
};
enum class Backfaces
{
    Include,
    Cull
};
constexpr uint32_t DefaultChannel = 1u, AllChannels = ~0u;
constexpr uint32_t NoTriangle = UINT32_MAX, NoInstance = UINT32_MAX;
struct Ray
{
    glm::vec3 origin{0}, direction{0, 0, -1};
    float maxDistance = 10000;
};
// Physical render-target pixels, top-left origin, +X right, +Y down. Fractional values are retained.
struct ViewportPoint
{
    float x = 0, y = 0;
    static ViewportPoint Pixel(uint32_t x, uint32_t y)
    {
        return {float(x) + 0.5f, float(y) + 0.5f};
    }
    static ViewportPoint Logical(float x, float y, float pixelsPerUnit)
    {
        return {x * pixelsPerUnit, y * pixelsPerUnit};
    }
};
struct Query
{
    uint32_t channels = AllChannels;
    std::vector<uint64_t> ignore;
    float maxDistance = 10000;
    Geometry geometry = Geometry::Exact;
    Backfaces backfaces = Backfaces::Include;
};
struct Hit
{
    uint64_t componentId = 0;
    glm::vec3 position{0}, normal{0}; // world-space, normal follows geometric winding
    float distance = 0;
    uint32_t triangleIndex = NoTriangle, instanceIndex = NoInstance;
};
struct Statistics
{
    uint64_t queries = 0, bounds = 0, triangles = 0, unsupported = 0;
    double microseconds = 0;
    Statistics &operator+=(const Statistics &b)
    {
        queries += b.queries;
        bounds += b.bounds;
        triangles += b.triangles;
        unsupported += b.unsupported;
        microseconds += b.microseconds;
        return *this;
    }
};
struct Result
{
    Status status = Status::Miss;
    std::optional<Hit> hit;
    Statistics statistics;
    explicit operator bool() const
    {
        return status == Status::Hit && hit.has_value();
    }
};
struct RayResult
{
    Status status = Status::InvalidRay;
    Ray ray;
    explicit operator bool() const
    {
        return status == Status::Success;
    }
};
bool Finite(glm::vec3 v);
RayResult Normalize(Ray ray); // accepts any finite, nonzero direction; distances are always world units
bool Valid(const Query &query);
Result IntersectPlane(Ray ray, glm::vec3 point, glm::vec3 normal, Backfaces backfaces = Backfaces::Include);
Result IntersectAabb(Ray ray, glm::vec3 minimum, glm::vec3 maximum);
Result IntersectTriangle(Ray ray, glm::vec3 a, glm::vec3 b, glm::vec3 c, Backfaces backfaces = Backfaces::Include);
// Matrices use Player's stored (shader-transposed) convention. Ray starts on the near plane.
RayResult ScreenPointToRay(ViewportPoint point, uint32_t width, uint32_t height, const DirectX::XMFLOAT4X4 &view,
                           const DirectX::XMFLOAT4X4 &projection, float maxDistance = 10000);
const char *StatusName(Status status);
} // namespace Trace
