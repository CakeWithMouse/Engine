#define NOMINMAX
#include <Engine/Scene/Trace.h>
#include <algorithm>
#include <cmath>
namespace Trace
{
bool Finite(glm::vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
RayResult Normalize(Ray ray)
{
    if (!Finite(ray.origin) || !Finite(ray.direction) || !std::isfinite(ray.maxDistance) || ray.maxDistance < 0)
        return {Status::InvalidRay, {}};
    const double length =
        std::sqrt(double(ray.direction.x) * ray.direction.x + double(ray.direction.y) * ray.direction.y +
                  double(ray.direction.z) * ray.direction.z);
    if (length == 0 || !std::isfinite(length))
        return {Status::InvalidRay, {}};
    ray.direction = glm::vec3(glm::dvec3(ray.direction) / length);
    return {Status::Success, ray};
}
bool Valid(const Query &q)
{
    return std::isfinite(q.maxDistance) && q.maxDistance >= 0 &&
           (q.geometry == Geometry::Exact || q.geometry == Geometry::Bounds ||
            q.geometry == Geometry::EditorSelection) &&
           (q.backfaces == Backfaces::Include || q.backfaces == Backfaces::Cull);
}
static Result HitAt(const Ray &r, float distance, glm::vec3 normal)
{
    const auto point = r.origin + r.direction * distance;
    if (!Finite(point) || !Finite(normal))
        return {};
    return {Status::Hit, Hit{0, point, normal, distance}};
}
Result IntersectPlane(Ray ray, glm::vec3 point, glm::vec3 normal, Backfaces backfaces)
{
    auto r = Normalize(ray);
    if (!r)
        return {r.status};
    ray = r.ray;
    if (!Finite(point) || !Finite(normal) || glm::length(glm::dvec3(normal)) < 1e-20 ||
        (backfaces != Backfaces::Cull && backfaces != Backfaces::Include))
        return {Status::InvalidQuery};
    normal = glm::vec3(glm::normalize(glm::dvec3(normal)));
    float den = glm::dot(normal, ray.direction);
    if (std::abs(den) < 1e-8f || (backfaces == Backfaces::Cull && den >= 0))
        return {};
    const float t = glm::dot(point - ray.origin, normal) / den;
    if (!std::isfinite(t) || t < 0 || t > ray.maxDistance)
        return {};
    return HitAt(ray, t, normal);
}
Result IntersectAabb(Ray ray, glm::vec3 lo, glm::vec3 hi)
{
    auto r = Normalize(ray);
    if (!r)
        return {r.status};
    ray = r.ray;
    if (!Finite(lo) || !Finite(hi) || glm::any(glm::greaterThan(lo, hi)))
        return {Status::InvalidQuery};
    float enter = -std::numeric_limits<float>::infinity(), leave = std::numeric_limits<float>::infinity();
    glm::vec3 en(0), ex(0);
    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(ray.direction[i]) < 1e-20f)
        {
            if (ray.origin[i] < lo[i] || ray.origin[i] > hi[i])
                return {};
            continue;
        }
        float a = (lo[i] - ray.origin[i]) / ray.direction[i], b = (hi[i] - ray.origin[i]) / ray.direction[i];
        glm::vec3 an(0), bn(0);
        an[i] = -1;
        bn[i] = 1;
        if (a > b)
        {
            std::swap(a, b);
            std::swap(an, bn);
        }
        if (a > enter)
        {
            enter = a;
            en = an;
        }
        if (b < leave)
        {
            leave = b;
            ex = bn;
        }
        if (enter > leave)
            return {};
    }
    // Starting inside returns the exit surface, not an invented normal at distance zero.
    const bool inside = enter < 0;
    const float t = inside ? leave : enter;
    if (!std::isfinite(t) || t < 0 || t > ray.maxDistance)
        return {};
    return HitAt(ray, t, inside ? ex : en);
}
Result IntersectTriangle(Ray ray, glm::vec3 a, glm::vec3 b, glm::vec3 c, Backfaces backfaces)
{
    auto r = Normalize(ray);
    if (!r)
        return {r.status};
    ray = r.ray;
    if (!Finite(a) || !Finite(b) || !Finite(c) || (backfaces != Backfaces::Cull && backfaces != Backfaces::Include))
        return {Status::InvalidQuery};
    const glm::dvec3 e1 = glm::dvec3(b) - glm::dvec3(a), e2 = glm::dvec3(c) - glm::dvec3(a), d(ray.direction);
    auto normal = glm::cross(e1, e2);
    const double area = glm::length(normal);
    if (area < 1e-20)
        return {};
    const auto p = glm::cross(d, e2);
    const double det = glm::dot(e1, p);
    if (std::abs(det) < area * 1e-12 || (backfaces == Backfaces::Cull && det <= 0))
        return {};
    const auto delta = glm::dvec3(ray.origin) - glm::dvec3(a);
    const double u = glm::dot(delta, p) / det;
    if (u < 0 || u > 1)
        return {};
    const auto q = glm::cross(delta, e1);
    const double v = glm::dot(d, q) / det;
    if (v < 0 || u + v > 1)
        return {};
    const double t = glm::dot(e2, q) / det;
    if (!std::isfinite(t) || t < 0 || t > ray.maxDistance)
        return {};
    return HitAt(ray, float(t), glm::vec3(normal / area));
}
RayResult ScreenPointToRay(ViewportPoint p, uint32_t w, uint32_t h, const DirectX::XMFLOAT4X4 &view,
                           const DirectX::XMFLOAT4X4 &proj, float distance)
{
    if (!std::isfinite(distance) || distance < 0)
        return {Status::InvalidQuery, {}};
    if (!w || !h)
        return {Status::InvalidViewport, {}};
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        return {Status::InvalidQuery, {}};
    if (p.x < 0 || p.y < 0 || p.x >= w || p.y >= h)
        return {Status::OutsideViewport, {}};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (!std::isfinite(view.m[i][j]) || !std::isfinite(proj.m[i][j]))
                return {Status::InvalidMatrix, {}};
    using namespace DirectX;
    const auto vp = XMMatrixTranspose(XMLoadFloat4x4(&view)) * XMMatrixTranspose(XMLoadFloat4x4(&proj));
    XMVECTOR determinant;
    const auto inverse = XMMatrixInverse(&determinant, vp);
    const float det = XMVectorGetX(determinant);
    if (!std::isfinite(det) || std::abs(det) < 1e-30f)
        return {Status::InvalidMatrix, {}};
    const float x = 2 * p.x / w - 1, y = 1 - 2 * p.y / h;
    auto unproject = [&](float z, glm::vec3 &out) {
        const auto v = XMVector4Transform(XMVectorSet(x, y, z, 1), inverse);
        const float weight = XMVectorGetW(v);
        if (!std::isfinite(weight) || std::abs(weight) < 1e-20f)
            return false;
        XMFLOAT3 result;
        XMStoreFloat3(&result, v / weight);
        out = {result.x, result.y, result.z};
        return Finite(out);
    };
    glm::vec3 nearPoint, farPoint;
    if (!unproject(0, nearPoint) || !unproject(1, farPoint))
        return {Status::InvalidMatrix, {}};
    auto result = Normalize({nearPoint, farPoint - nearPoint, distance});
    if (!result)
        result.status = Status::InvalidMatrix;
    return result;
}
const char *StatusName(Status s)
{
    switch (s)
    {
#define NAME(x)                                                                                                        \
    case Status::x:                                                                                                    \
        return #x;
        NAME(Success)
        NAME(Hit)
        NAME(Miss) NAME(InvalidRay) NAME(InvalidQuery) NAME(InvalidViewport) NAME(OutsideViewport) NAME(InvalidMatrix)
            NAME(NoCamera) NAME(CursorUnavailable) NAME(InputBlocked)
#undef NAME
    }
    return "Unknown";
}
} // namespace Trace
