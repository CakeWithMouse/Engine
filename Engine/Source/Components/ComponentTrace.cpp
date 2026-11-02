#define NOMINMAX
#include <Engine/Components/GameComponents.h>
#include <algorithm>
#include <cmath>

Trace::Result GameComponent::RaycastGeometry(const Trace::Ray &ray, const Trace::Query &query, Trace::Statistics &stats)
{
    // Read the existing visual geometry without lazy material conversion, texture loads or GPU work.
    if (VisualComponent && !VisualModelAsset.empty())
        return VisualComponent->RaycastGeometry(ray, query, stats);
    // Assigned primitives use their own static geometry, even on animated/instanced owners.
    auto* geometry = VisualPrimitive && VisualComponent ? VisualComponent.get() : this;
    if (InstanceArray.empty())
        return geometry->RaycastGeometryAt(ray, query, stats,
            geometry == this ? GetTraceMatrix() : GetWorldMatrix());
    Trace::Result best;
    for (size_t i = 0; i < InstanceArray.size(); ++i)
    {
        auto hit = geometry->RaycastGeometryAt(ray, query, stats, InstanceArray[i]->GetTraceMatrix());
        if (hit && (!best || hit.hit->distance < best.hit->distance))
        {
            best = hit;
            best.hit->instanceIndex = uint32_t(i);
        }
    }
    return best;
}
Trace::Result GameComponent::RaycastGeometryAt(const Trace::Ray &ray, const Trace::Query &query,
                                               Trace::Statistics &stats, const DirectX::XMFLOAT4X4 &transform)
{
    using namespace DirectX;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (!std::isfinite(transform.m[i][j]))
            {
                ++stats.unsupported;
                return {};
            }
    const auto matrix = XMLoadFloat4x4(&transform);
    const float determinant = XMVectorGetX(XMMatrixDeterminant(matrix));
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-30f)
    {
        ++stats.unsupported;
        return {};
    }
    auto world = [&](glm::vec3 p) {
        XMFLOAT3 v;
        XMStoreFloat3(&v, XMVector3TransformCoord(XMVectorSet(p.x, p.y, p.z, 1), matrix));
        return glm::vec3(v.x, v.y, v.z);
    };
    if (!TraceBoundsValid)
    {
        if (!points.empty())
        {
            ++stats.unsupported;
            return {};
        }
        if (query.geometry != Trace::Geometry::EditorSelection)
            return {};
        ++stats.bounds;
        const auto center = world(glm::vec3(0));
        const float radius = (std::max)(.25f, GetWorldBoundingRadius());
        const auto delta = ray.origin - center;
        const float b = glm::dot(delta, ray.direction), disc = b * b - glm::dot(delta, delta) + radius * radius;
        if (!Trace::Finite(center) || !std::isfinite(radius) || !std::isfinite(disc) || disc < 0)
            return {};
        float t = -b - std::sqrt(disc);
        if (t < 0)
            t = -b + std::sqrt(disc);
        if (!std::isfinite(t) || t < 0 || t > ray.maxDistance)
            return {};
        const auto p = ray.origin + t * ray.direction;
        const auto normal = glm::normalize(p - center);
        if (!Trace::Finite(p) || !Trace::Finite(normal))
            return {};
        return {Trace::Status::Hit, Trace::Hit{0, p, normal, t}};
    }
    glm::vec3 lo(std::numeric_limits<float>::max()), hi = -lo;
    for (int i = 0; i < 8; ++i)
    {
        auto p = world({(i & 1) ? TraceMaximum.x : TraceMinimum.x, (i & 2) ? TraceMaximum.y : TraceMinimum.y,
                        (i & 4) ? TraceMaximum.z : TraceMinimum.z});
        if (!Trace::Finite(p))
        {
            ++stats.unsupported;
            return {};
        }
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    ++stats.bounds;
    auto bounds = Trace::IntersectAabb(ray, lo, hi);
    if (query.geometry == Trace::Geometry::Bounds)
        return bounds;
    const bool inside = glm::all(glm::greaterThanEqual(ray.origin, lo)) && glm::all(glm::lessThanEqual(ray.origin, hi));
    if (!bounds && !inside)
        return {};
    if (!SupportsExactTrace())
    {
        if (query.geometry == Trace::Geometry::EditorSelection)
            return bounds;
        ++stats.unsupported;
        return {};
    }
    Trace::Result best;
    auto vertex = [&](int index) {
        const auto &p = points[size_t(index) * 2];
        return world({p.x, p.y, p.z});
    };
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        int a = indices[i], b = indices[i + 1], c = indices[i + 2];
        if (a < 0 || b < 0 || c < 0 || size_t((std::max)({a, b, c})) >= points.size() / 2)
            continue;
        ++stats.triangles;
        auto hit = Trace::IntersectTriangle(ray, vertex(a), vertex(b), vertex(c), query.backfaces);
        if (hit && (!best || hit.hit->distance < best.hit->distance))
        {
            best = hit;
            best.hit->triangleIndex = uint32_t(i / 3);
        }
    }
    return best;
}
