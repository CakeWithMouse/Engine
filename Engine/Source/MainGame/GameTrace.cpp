#define NOMINMAX
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <chrono>
#include <algorithm>
#include <cmath>

Trace::RayResult Game::ScreenPointToRay(Trace::ViewportPoint point, float distance) const
{
    if (!FirstPlayer)
        return {Trace::Status::NoCamera, {}};
    if (!DisplayPtr || bMinimized || bResizePending)
        return {Trace::Status::InvalidViewport, {}};
    FirstPlayer->RefreshMatrices(); // includes the first PreUpdate and changes made since the last frame
    return Trace::ScreenPointToRay(point, DisplayPtr->GetWidth(), DisplayPtr->GetHeight(), FirstPlayer->GetViewMatrix(),
                                   FirstPlayer->GetProjectionMatrix(), distance);
}
Trace::Result Game::RaycastScreen(Trace::ViewportPoint point, const Trace::Query &query)
{
    if (!Trace::Valid(query))
        return {Trace::Status::InvalidQuery};
    auto ray = ScreenPointToRay(point, query.maxDistance);
    if (!ray)
        return {ray.status};
    return scene.Raycast(ray.ray, query);
}
Trace::Result Game::RaycastCursor(const Trace::Query &query)
{
    if (!Trace::Valid(query))
        return {Trace::Status::InvalidQuery};
    if (!ActionInput.IsContextEnabled(Input::Context::Game) || ActionInput.IsPointerBlocked())
        return {Trace::Status::InputBlocked};
    const auto cursor = ActionInput.GetCursorPosition();
    if (!cursor.valid)
        return {Trace::Status::CursorUnavailable};
    if (!DisplayPtr || cursor.width != uint32_t(DisplayPtr->GetWidth()) ||
        cursor.height != uint32_t(DisplayPtr->GetHeight()))
        return {Trace::Status::InvalidViewport};
    return RaycastScreen({cursor.x, cursor.y}, query);
}
namespace
{
double TraceTime()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace
void Game::DebugDrawTrace(const Trace::Ray &input, const Trace::Result &result, float seconds)
{
    auto normalized = Trace::Normalize(input);
    if (!normalized || !std::isfinite(seconds) || seconds <= 0)
        return;
    const auto &ray = normalized.ray;
    const auto end = result ? result.hit->position : ray.origin + ray.direction * ray.maxDistance;
    if (!Trace::Finite(end) || (result && !Trace::Finite(result.hit->normal)))
        return;
    const double expiry = TraceTime() + (std::min)(seconds, 3600.f);
    auto line = [&](glm::vec3 a, glm::vec3 b, uint32_t color) {
        if (TraceDebugLines.size() >= 4096)
            TraceDebugLines.erase(TraceDebugLines.begin());
        TraceDebugLines.push_back({{a.x, a.y, a.z, b.x, b.y, b.z, color, 2u | (2u << 8)}, expiry});
    };
    line(ray.origin, end, result ? 0xff40ff40u : 0xff4040ffu);
    if (result)
    {
        const auto p = result.hit->position;
        line(p, p + result.hit->normal, 0xffffff00u);
        for (int i = 0; i < 3; ++i)
        {
            glm::vec3 d(0);
            d[i] = .05f;
            line(p - d, p + d, 0xffffffffu);
        }
    }
}
void Game::ClearDebugTraces()
{
    TraceDebugLines.clear();
}
std::vector<EngineOverlayLine> Game::CollectTraceDebugLines()
{
    const auto now = TraceTime();
    TraceDebugLines.erase(
        std::remove_if(TraceDebugLines.begin(), TraceDebugLines.end(), [&](const auto &l) { return l.expiry <= now; }),
        TraceDebugLines.end());
    std::vector<EngineOverlayLine> lines;
    lines.reserve(TraceDebugLines.size());
    for (const auto &item : TraceDebugLines)
        lines.push_back(item.line);
    return lines;
}
size_t Game::GetDebugTraceLineCount()
{
    return CollectTraceDebugLines().size();
}
