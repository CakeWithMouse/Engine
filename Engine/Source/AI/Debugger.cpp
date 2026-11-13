#include <Engine/AI/Debugger.h>
#include <Engine/AI/Group.h>
#include <Engine/AI/World.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Ai
{
namespace
{
constexpr uint32_t Green = 0x40ff60ff, Yellow = 0xffd040ff, Blue = 0x50a0ffff, Red = 0xff4040ff, Grey = 0xb0b0b0ff, White = 0xffffffff;
constexpr float Pi = 3.14159265358979f;
}

void Debugger::Select(Controller& controller) { selected_ = controller.Serial(); }

Controller* Debugger::Selected() const
{
    if (!selected_) return nullptr;
    for (Controller* c : world_.Controllers()) if (c->Serial() == selected_) return c;
    return nullptr;
}

void Debugger::Update()
{
    lines_.clear();
    if (!visible_) return;
    const glm::vec3 lift(0, 0.3f, 0);
    for (Controller* c : world_.Controllers())
    {
        auto* group = dynamic_cast<Group*>(c);
        if (!c->OwnerRef() && !group) continue;
        const bool selected = c->Serial() == selected_;
        const glm::vec3 p = c->Position() + lift;
        // Path to the destination.
        glm::vec3 from = p;
        for (const glm::vec3& point : c->Path()) { lines_.push_back({from, point + lift, Green}); from = point + lift; }
        if (group && (c->GetMoveStatus() == MoveStatus::Moving || c->GetMoveStatus() == MoveStatus::Waiting))
            lines_.push_back({p, c->Destination() + lift, Green});
        // Focus.
        if (GameComponent* focus = world_.Resolve(c->Focus().id)) lines_.push_back({p, focus->GetWorldPosition() + lift, Yellow});
        // Sight cone.
        const Perception& senses = c->GetPerception();
        if (senses.SightEnabled() && (selected || !group))
        {
            const float half = senses.FieldOfView() >= 360 ? Pi : senses.FieldOfView() * Pi / 360.0f;
            const int segments = 12;
            glm::vec3 previous = p;
            for (int i = 0; i <= segments; ++i)
            {
                const float angle = c->Yaw() - half + 2 * half * i / segments;
                const glm::vec3 edge = p + glm::vec3(std::sin(angle), 0, std::cos(angle)) * senses.SightRadius();
                if (i == 0 || i == segments) { if (half < Pi) lines_.push_back({p, edge, Blue}); }
                if (i > 0) lines_.push_back({previous, edge, Blue});
                previous = edge;
            }
        }
        // What it knows about.
        for (const Perceived& known : senses.Known())
            lines_.push_back({p, known.lastPosition + lift, known.visibleNow ? Red : Grey});
        if (selected)
        {
            lines_.push_back({p, p + glm::vec3(0, 2.5f, 0), White});
            if (group)
                for (const auto& m : group->Members())
                    if (m.alive && m.hasSlot)
                    {
                        const glm::vec3 s(m.slot.x, p.y, m.slot.y);
                        lines_.push_back({s - glm::vec3(0.3f, 0, 0), s + glm::vec3(0.3f, 0, 0), Yellow});
                        lines_.push_back({s - glm::vec3(0, 0, 0.3f), s + glm::vec3(0, 0, 0.3f), Yellow});
                    }
        }
    }
}

std::string Debugger::Describe(const Controller& c) const
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    out << c.Name() << "  state: " << (c.ActiveState().empty() ? "-" : c.ActiveState()) << '\n';
    for (const auto& task : c.Instance().ActiveTasks()) out << "  task " << task.state << ": " << task.task << " [" << ToString(task.status) << "]\n";
    out << "movement: " << ToString(c.GetMoveStatus());
    if (c.GetMoveStatus() == MoveStatus::Moving || c.GetMoveStatus() == MoveStatus::Waiting)
        out << " to (" << c.Destination().x << ", " << c.Destination().z << "), corners left " << c.Path().size();
    out << '\n';
    if (const auto* group = dynamic_cast<const Group*>(&c)) out << "soldiers: " << group->Size() << '\n';
    const std::string memory = c.Memory().Describe();
    if (!memory.empty()) out << "memory:\n" << memory;
    for (const Perceived& known : c.GetPerception().Known())
        out << "knows object " << known.who.id << (known.visibleNow ? " (visible)" : " (remembered)") << " at ("
            << known.lastPosition.x << ", " << known.lastPosition.z << "), " << known.age << " s ago\n";
    const auto& history = c.History();
    const size_t first = history.size() > 8 ? history.size() - 8 : 0;
    if (first < history.size()) out << "history:\n";
    for (size_t i = first; i < history.size(); ++i) out << "  " << history[i] << '\n';
    return out.str();
}

std::string Debugger::Overview() const
{
    std::ostringstream out;
    for (Controller* c : world_.Controllers())
        out << c->Name() << "  " << (c->ActiveState().empty() ? "-" : c->ActiveState()) << "  " << ToString(c->GetMoveStatus()) << '\n';
    return out.str();
}

void Debugger::Draw(Game& game) const
{
    if (!visible_) return;
    for (const DebugLine& line : lines_)
    {
        // DebugLine keeps 0xRRGGBBAA for readability; the overlay wants 0xAABBGGRR.
        const uint32_t c = line.rgba;
        const uint32_t abgr = (c & 0xffu) << 24 | ((c >> 8) & 0xffu) << 16 | ((c >> 16) & 0xffu) << 8 | (c >> 24);
        game.DebugDrawLine(line.from, line.to, abgr);
    }
}
} // namespace Ai
