#include <Engine/AI/Ai.h>
#include <Engine/Runtime/SceneEditSession.h>
// Viewport diagnostics: geometry is owned by the active Game, never by Qt or a borrowed controller.
#include <Engine/AI/Navigation.h>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unordered_map>

EngineAiDiagnosticsApi SceneEditSession::GetAiDiagnosticsApi() noexcept
{
    return {
        1, this,
        [](void *context, const char *request, void(ENGINE_MODULE_CALL * visitor)(void *, const char *),
           void *user) -> uint32_t {
            if (!request || !visitor)
                return EngineSceneInvalidArgument;
            return Invoke(context, [&](SceneEditSession &s) -> uint32_t {
                using J = nlohmann::json;
                const auto q = J::parse(request, nullptr, false);
                if (!q.is_object())
                    return EngineSceneInvalidArgument;
                const unsigned mask = q.value("layers", 0u);
                if (!mask)
                {
                    s.edit_->SetAiDiagnosticOverlay({}, {});
                    if (s.play_)
                        s.play_->SetAiDiagnosticOverlay({}, {});
                    visitor(user, "{\"source\":\"off\"}");
                    return EngineSceneOk;
                }
                auto number = [&](const char *name, float fallback, float lo, float hi) {
                    const float n = q.value(name, fallback);
                    return std::isfinite(n) ? std::clamp(n, lo, hi) : fallback;
                };
                const float y = number("height", .15f, -10000, 10000);
                float radius = number("radius", .5f, 0, 100);
                const float extent = number("extent", 60, 5, 500);
                const glm::vec2 center(number("centerX", 0, -1e6f, 1e6f), number("centerZ", 0, -1e6f, 1e6f));
                const glm::vec2 from(number("fromX", -10, -1e6f, 1e6f), number("fromZ", 0, -1e6f, 1e6f));
                const glm::vec2 to(number("toX", 10, -1e6f, 1e6f), number("toZ", 0, -1e6f, 1e6f));
                const uint64_t selectedId = q.value("object", uint64_t(0));
                const uint32_t serial = q.value("controller", 0u);
                Ai::World *world = nullptr;
                if (s.play_)
                {
                    if (auto *subsystem = s.play_->GetSubsystem<Ai::Subsystem>())
                        world = &subsystem->GetWorld();
                }
                else
                {
                    // Same bake rules as Play, refreshed only when a navigation volume changes.
                    std::ostringstream key;
                    key.precision(std::numeric_limits<float>::max_digits10);
                    for (const auto &entry : s.edit_->GetScene().GetComponents())
                        if (auto *v = dynamic_cast<NavVolume *>(entry.second.get()))
                        {
                            glm::vec2 a, b;
                            v->GetArea(a, b);
                            key << entry.first << ':' << a.x << ',' << a.y << ',' << b.x << ',' << b.y << ',' << v->mode
                                << ',' << v->cost << ',' << v->cellSize << ';';
                        }
                    if (!s.diagnosticPreview_)
                        s.diagnosticPreview_ = std::make_unique<Ai::World>();
                    if (key.str() != s.diagnosticSceneKey_ || s.diagnosticPreview_->GetScene() != &s.edit_->GetScene())
                    {
                        s.diagnosticPreview_->Attach(&s.edit_->GetScene());
                        s.diagnosticSceneKey_ = key.str();
                    }
                    world = s.diagnosticPreview_.get();
                }
                J result = {{"source", s.play_ ? (s.state_ == EnginePaused ? "paused" : "live") : "preview"},
                            {"units", J::array()}};
                std::vector<EngineOverlayLine> lines;
                std::vector<EngineOverlayTriangle> triangles;
                const uint32_t flags = q.value("xray", false) ? OverlayFlags::AlwaysOnTop : OverlayFlags::DepthTest;
                auto line = [&](glm::vec3 a, glm::vec3 b, uint32_t color) {
                    if (lines.size() < 50000)
                        lines.push_back({a.x, a.y, a.z, b.x, b.y, b.z, color, flags | OverlayFlags::Thickness(2)});
                };
                auto p3 = [&](glm::vec2 p) { return glm::vec3(p.x, y, p.y); };
                auto cross = [&](glm::vec3 p, float r, uint32_t color) {
                    line(p - glm::vec3(r, 0, 0), p + glm::vec3(r, 0, 0), color);
                    line(p - glm::vec3(0, 0, r), p + glm::vec3(0, 0, r), color);
                };
                auto circle = [&](glm::vec3 p, float r, uint32_t color) {
                    for (int i = 0; i < 32; ++i)
                    {
                        float a = i * 6.2831853f / 32, b = (i + 1) * 6.2831853f / 32;
                        line(p + glm::vec3(std::cos(a) * r, 0, std::sin(a) * r),
                             p + glm::vec3(std::cos(b) * r, 0, std::sin(b) * r), color);
                    }
                };
                Ai::Controller *chosen = nullptr;
                if (world)
                {
                    std::string selectedName;
                    if (auto *object = s.edit_->GetScene().FindComponent(selectedId))
                        for (const auto &e : s.edit_->GetScene().GetComponents())
                            if (e.second.get() == object)
                                selectedName = e.first;
                    const auto controllers = world->Controllers();
                    std::unordered_map<ComponentId, std::string> names;
                    for (const auto &entry : s.Active().GetScene().GetComponents())
                        names.emplace(entry.second->GetComponentId(), entry.first);
                    for (auto *c : controllers)
                    {
                        const auto owner = names.find(c->OwnerRef().id);
                        const std::string name = owner == names.end() ? c->Name() : owner->second;
                        result["units"].push_back({{"id", c->Serial()}, {"name", name}});
                        if ((serial && c->Serial() == serial) ||
                            (!serial && !selectedName.empty() && name == selectedName))
                            chosen = c;
                    }
                    if (chosen && q.value("agentRadius", true))
                        radius = chosen->Radius();
                    if (chosen)
                        result["details"] = world->Debug().Describe(*chosen);
                    auto &grid = world->Navigation();
                    result["configured"] = grid.IsConfigured();
                    result["radius"] = radius;
                    if (grid.IsConfigured())
                    {
                        result["width"] = grid.Width();
                        result["height"] = grid.Height();
                        result["version"] = grid.Version();
                        result["cellSize"] = grid.CellSize();
                        auto lo = glm::max(glm::ivec2(0), grid.CellOf(center - glm::vec2(extent)));
                        auto hi = glm::min(glm::ivec2(grid.Width() - 1, grid.Height() - 1),
                                           grid.CellOf(center + glm::vec2(extent)));
                        const int stride = std::max(
                            1, int(std::ceil(std::sqrt(
                                   std::max(0.0, double(hi.x - lo.x + 1) * double(hi.y - lo.y + 1)) / 6000.0))));
                        result["stride"] = stride;
                        const Ai::Detail::FlowField *field = nullptr;
                        if (auto *group = dynamic_cast<Ai::Group *>(chosen))
                            field = group->DiagnosticField();
                        std::shared_ptr<const Ai::Detail::FlowField> previewField;
                        if ((mask & 16) && !s.play_)
                        {
                            world->Fields().coarseCell = grid.CellSize();
                            previewField = world->Fields().Request(grid, to, radius, from, to, 16);
                            Ai::Stats stats;
                            world->Fields().Process(grid, 4096, stats);
                            field = previewField.get();
                        }
                        const bool fieldCurrent = field && field->version == grid.Version();
                        result["field"] = field ? (!fieldCurrent  ? "stale"
                                                   : field->ready ? "ready"
                                                                  : "building")
                                                : "none";
                        if (mask & 23)
                            for (int z = lo.y; z <= hi.y; z += stride)
                                for (int x = lo.x; x <= hi.x; x += stride)
                                {
                                    const glm::ivec2 cell(x, z);
                                    const auto p = grid.CenterOf(cell);
                                    const float h = grid.CellSize() * .46f;
                                    const auto cost = grid.CostAt(cell);
                                    const bool walk = (mask & 4) && grid.IsWalkableCell(cell, radius);
                                    if (mask & 7)
                                    {
                                        uint32_t col = cost == 255 ? 0x994646ef : 0x6657b866;
                                        if ((mask & 2) && cost > 1 && cost != 255)
                                        {
                                            const auto green =
                                                uint32_t(210 - 140 * std::log(float(cost)) / std::log(254.0f));
                                            col = 0x995000f0u | (green << 8);
                                        }
                                        if ((mask & 4) && cost != 255)
                                            col = walk ? 0x6657b866 : 0xaa35a6ff;
                                        const auto a = p3(p + glm::vec2(-h, -h)), b = p3(p + glm::vec2(h, -h)),
                                                   c = p3(p + glm::vec2(h, h)), d = p3(p + glm::vec2(-h, h));
                                        triangles.push_back({a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z, col, flags});
                                        triangles.push_back({a.x, a.y, a.z, c.x, c.y, c.z, d.x, d.y, d.z, col, flags});
                                        if (mask & 1)
                                        {
                                            line(a, b, 0x886a806e);
                                            line(b, c, 0x886a806e);
                                        }
                                    }
                                    if ((mask & 16) && fieldCurrent && field->ready && field->Reaches(grid, p))
                                    {
                                        const auto v = field->Sample(grid, p);
                                        if (glm::length(v) > .01f)
                                        {
                                            const auto a = p3(p), b = p3(p + v * grid.CellSize() * .65f);
                                            line(a, b, 0xffffdb60);
                                            glm::vec2 side(-v.y, v.x);
                                            line(b, p3(p + v * grid.CellSize() * .38f + side * grid.CellSize() * .18f),
                                                 0xffffdb60);
                                            line(b, p3(p + v * grid.CellSize() * .38f - side * grid.CellSize() * .18f),
                                                 0xffffdb60);
                                        }
                                    }
                                }
                        if ((mask & 16) && field)
                        {
                            auto a = p3(grid.Origin() + glm::vec2(field->low) * grid.CellSize());
                            auto b = p3(grid.Origin() + glm::vec2(field->low + field->size) * grid.CellSize());
                            line(a, {b.x, y, a.z}, 0xffffdb60);
                            line({b.x, y, a.z}, b, 0xffffdb60);
                            line(b, {a.x, y, b.z}, 0xffffdb60);
                            line({a.x, y, b.z}, a, 0xffffdb60);
                        }
                        if (mask & 128)
                        {
                            const auto path = grid.FindPath(from, to, radius, 30000);
                            auto prev = p3(from);
                            for (auto p : path.points)
                            {
                                line(prev, p3(p), 0xfff5d755);
                                prev = p3(p);
                            }
                            circle(p3(from), std::max(radius, .35f), 0xff70ed78);
                            cross(p3(to), .8f, 0xffdb78f5);
                            result["path"] = path.kind == Ai::PathResult::Kind::Found     ? "found"
                                             : path.kind == Ai::PathResult::Kind::Partial ? "partial"
                                                                                          : "notFound";
                            result["expanded"] = path.expanded;
                        }
                        if (q.value("hover", false))
                        {
                            auto cell =
                                grid.CellOf({number("hoverX", 0, -1e6f, 1e6f), number("hoverZ", 0, -1e6f, 1e6f)});
                            if (grid.Contains(cell))
                            {
                                result["cell"] = {{"x", cell.x},
                                                  {"z", cell.y},
                                                  {"cost", grid.CostAt(cell)},
                                                  {"clearance", grid.ClearanceAt(cell)},
                                                  {"walkable", grid.IsWalkableCell(cell, radius)}};
                                cross(p3(grid.CenterOf(cell)), grid.CellSize() * .5f, 0xffffffff);
                            }
                        }
                    }
                    for (auto *c : controllers)
                    {
                        if (q.value("selectedOnly", true) && c != chosen)
                            continue;
                        const auto p = c->Position() + glm::vec3(0, .2f, 0);
                        if (mask & 8)
                        {
                            auto prev = p;
                            for (auto point : c->Path())
                            {
                                line(prev, point + glm::vec3(0, .2f, 0), 0xff70ed78);
                                prev = point + glm::vec3(0, .2f, 0);
                            }
                            if (c->GetMoveStatus() == Ai::MoveStatus::Moving ||
                                c->GetMoveStatus() == Ai::MoveStatus::Waiting)
                                cross(c->Destination() + glm::vec3(0, .2f, 0), .8f, 0xffdb78f5);
                            circle(p, c->Radius(), 0xff70ed78);
                        }
                        if (mask & 32)
                        {
                            const auto &senses = c->GetPerception();
                            if (senses.SightEnabled())
                            {
                                const float half = senses.FieldOfView() * 3.14159265f / 360;
                                glm::vec3 prev;
                                for (int i = 0; i <= 32; ++i)
                                {
                                    float a = c->Yaw() - half + 2 * half * i / 32;
                                    auto edge = p + glm::vec3(std::sin(a), 0, std::cos(a)) * senses.SightRadius();
                                    if (i == 0 || i == 32)
                                        line(p, edge, 0xffffb350);
                                    if (i)
                                        line(prev, edge, 0xffffb350);
                                    prev = edge;
                                }
                            }
                            for (const auto &known : senses.Known())
                                line(p, known.lastPosition + glm::vec3(0, .2f, 0),
                                     known.visibleNow ? 0xff6464f5 : 0xffaaaaaa);
                            if (auto *target = world->Resolve(c->Focus().id))
                                line(p, target->GetWorldPosition(), 0xff4bd9ff);
                        }
                        if ((mask & 64))
                            if (auto *group = dynamic_cast<Ai::Group *>(c))
                                for (const auto &m : group->Members())
                                    if (m.alive && m.hasSlot)
                                    {
                                        const glm::vec3 slot(m.slot.x, p.y, m.slot.y);
                                        cross(slot, .35f, 0xff4bd9ff);
                                        const glm::vec3 position(m.position.x, m.height + .2f, m.position.y);
                                        circle(position, m.radius, 0xffffdb60);
                                        line(position, slot, 0x6650bbff);
                                    }
                    }
                }
                result["lines"] = lines.size();
                result["triangles"] = triangles.size();
                s.Active().SetAiDiagnosticOverlay(std::move(lines), std::move(triangles));
                const auto output = result.dump();
                visitor(user, output.c_str());
                return EngineSceneOk;
            });
        }};
}
