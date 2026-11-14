#include <Engine/AI/Group.h>
#include <Engine/AI/World.h>
#include <Engine/Components/Character.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/SaveGame/SaveArchive.h>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace Ai
{
namespace
{
constexpr float Pi = 3.14159265358979f;
float WrapAngle(float a) { while (a > Pi) a -= 2 * Pi; while (a < -Pi) a += 2 * Pi; return a; }
float Turn(float from, float to, float maxStep) { return WrapAngle(from + std::clamp(WrapAngle(to - from), -maxStep, maxStep)); }

// The few instance components of one squad, looked up once per frame instead of once per soldier.
class Batches
{
public:
    explicit Batches(World& world) : world_(world) {}
    InstancedMeshComponent* Get(ComponentId id)
    {
        for (const auto& [key, component] : items_) if (key == id) return component;
        items_.emplace_back(id, dynamic_cast<InstancedMeshComponent*>(world_.Resolve(id)));
        return items_.back().second;
    }
private:
    World& world_;
    std::vector<std::pair<ComponentId, InstancedMeshComponent*>> items_;
};
}

Group::Group(World& world) : Controller(world)
{
    canMove_ = true;
    synced_ = true;
    radius_ = 1;
}

void Group::AddInstance(ComponentId batch, size_t index, float radius, float speed)
{
    if (!(radius > 0) || !(speed >= 0)) throw std::invalid_argument("Group::AddInstance: invalid radius or speed");
    auto* component = dynamic_cast<InstancedMeshComponent*>(world_->Resolve(batch));
    if (!component) throw std::invalid_argument("Group::AddInstance: no InstancedMeshComponent with this ID in the scene");
    if (index >= component->GetMeshInstanceCount()) throw std::out_of_range("Group::AddInstance: no instance with this index");
    const MeshInstance& instance = component->GetMeshInstance(index);
    Member member;
    member.batch = batch;
    member.index = index;
    member.position = {instance.Position.x, instance.Position.z};
    member.height = instance.Position.y;
    member.yaw = instance.Yaw;
    member.radius = radius;
    member.speed = speed;
    member.slot = member.position;
    members_.push_back(member);
    ++alive_;
    largest_ = std::max(largest_, radius);
}

void Group::AddCharacter(Character& character, float radius)
{
    if (!character.GetComponentId()) throw std::invalid_argument("Group::AddCharacter: the character is not in a scene");
    Member member;
    member.character = character.GetComponentId();
    const glm::vec3 p = character.GetWorldPosition();
    member.position = {p.x, p.z};
    member.height = p.y;
    member.radius = radius;
    member.speed = character.GetMoveSpeed();
    member.slot = member.position;
    members_.push_back(member);
    ++alive_;
    largest_ = std::max(largest_, radius);
}

void Group::RemoveMember(size_t index)
{
    if (index >= members_.size() || !members_[index].alive) return;
    Member& member = members_[index];
    member.alive = false;
    member.velocity = glm::vec2(0);
    --alive_;
    if (auto* component = dynamic_cast<InstancedMeshComponent*>(world_->Resolve(member.batch)))
        if (member.index < component->GetMeshInstanceCount()) component->SetInstanceVisible(member.index, false);
}

size_t Group::Size() const { return alive_; }

void Group::Recount()
{
    alive_ = 0;
    largest_ = 0;
    for (const Member& m : members_) if (m.alive) { ++alive_; largest_ = std::max(largest_, m.radius); }
}

Group* Group::FindHostile(float radius) const
{
    Group* best = nullptr;
    float bestDistance = radius;
    for (Controller* c : world_->Controllers())
    {
        auto* other = dynamic_cast<Group*>(c);
        if (!other || other == this || other->Size() == 0) continue;
        if (world_->GetAttitude(perception_.GetTeam(), other->perception_.GetTeam()) != Attitude::Hostile) continue;
        const float distance = glm::length(glm::vec2(other->position_.x - position_.x, other->position_.z - position_.z));
        if (distance <= bestDistance) { best = other; bestDistance = distance; }
    }
    return best;
}

bool Group::SyncBody()
{
    glm::vec3 sum(0);
    size_t alive = 0;
    float largest = 0;
    Batches batches(*world_);
    for (Member& m : members_)
    {
        if (!m.alive) continue;
        if (m.character)
        {
            GameComponent* body = world_->Resolve(m.character);
            if (!body) { m.alive = false; continue; }
            const glm::vec3 p = body->GetWorldPosition();
            m.position = {p.x, p.z};
            m.height = p.y;
        }
        else
        {
            InstancedMeshComponent* batch = batches.Get(m.batch);
            if (!batch || m.index >= batch->GetMeshInstanceCount()) { m.alive = false; continue; }
            if (reloadPositions_)
            {
                const MeshInstance& instance = batch->GetMeshInstance(m.index);
                m.position = {instance.Position.x, instance.Position.z};
                m.height = instance.Position.y;
                m.yaw = instance.Yaw;
            }
        }
        sum += glm::vec3(m.position.x, m.height, m.position.y);
        ++alive;
        largest = std::max(largest, m.radius);
    }
    alive_ = alive;
    largest_ = largest;
    reloadPositions_ = false;
    if (alive) position_ = sum / static_cast<float>(alive);
    yaw_ = facing_;
    return true; // a group outlives its soldiers; the game destroys it
}

void Group::AssignSlots(glm::vec2 center)
{
    std::vector<size_t> alive;
    for (size_t i = 0; i < members_.size(); ++i) if (members_[i].alive) alive.push_back(i);
    if (alive.empty()) return;
    const int count = static_cast<int>(alive.size());
    const int columns = formation_.columns > 0 ? formation_.columns : std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count)))));
    const int rows = (count + columns - 1) / columns;
    const glm::vec2 forward(std::sin(facing_), std::cos(facing_)), right(std::cos(facing_), -std::sin(facing_));
    // Front soldiers take front slots, left soldiers left slots: few paths cross.
    std::sort(alive.begin(), alive.end(), [&](size_t a, size_t b) {
        return glm::dot(members_[a].position - center, forward) > glm::dot(members_[b].position - center, forward); });
    NavGrid& grid = world_->Navigation();
    for (int row = 0; row < rows; ++row)
    {
        const auto first = alive.begin() + row * columns;
        const auto last = alive.begin() + std::min(count, (row + 1) * columns);
        std::sort(first, last, [&](size_t a, size_t b) {
            return glm::dot(members_[a].position - center, right) < glm::dot(members_[b].position - center, right); });
        const int inRow = static_cast<int>(last - first);
        for (int column = 0; column < inRow; ++column)
        {
            Member& m = members_[*(first + column)];
            const glm::vec2 offset = right * ((static_cast<float>(column) - (inRow - 1) * 0.5f) * formation_.spacing)
                                   - forward * ((static_cast<float>(row) - (rows - 1) * 0.5f) * formation_.spacing);
            m.slot = grid.IsConfigured() ? grid.NearestWalkable(center + offset, m.radius) : center + offset;
            m.hasSlot = true;
            m.settled = false;
            m.direct = false;
        }
    }
}

void Group::MoveTo(glm::vec3 point, float acceptanceRadius)
{
    if (Size() == 0) { moveStatus_ = MoveStatus::Unavailable; return; }
    goal_ = Goal::Point;
    goalPoint_ = point;
    acceptance_ = acceptanceRadius;
    orderTime_ = world_->Time();
    moving_ = true;
    moveStatus_ = MoveStatus::Waiting;
    AssignSlots({point.x, point.z});
    field_.reset();
    fieldMargin_ = 64;
    RequestField();
}

void Group::RequestField()
{
    NavGrid& grid = world_->Navigation();
    fieldChecked_ = false;
    if (!grid.IsConfigured()) { field_.reset(); return; }
    // The window covers the soldiers, their slots and the goal.
    float largest = 0;
    glm::vec2 low(goalPoint_.x, goalPoint_.z), high = low;
    for (const Member& m : members_)
    {
        if (!m.alive) continue;
        largest = std::max(largest, m.radius);
        low = glm::min(low, glm::min(m.position, m.slot));
        high = glm::max(high, glm::max(m.position, m.slot));
    }
    field_ = world_->Fields().Request(grid, {goalPoint_.x, goalPoint_.z}, largest, low, high, fieldMargin_);
}

void Group::MoveTo(ObjectRef target, float acceptanceRadius)
{
    const glm::vec3 at = ObjectPosition(target);
    if (!std::isfinite(at.x)) { StopMovement(); moveStatus_ = MoveStatus::Unreachable; return; }
    MoveTo(at, acceptanceRadius);
}

void Group::StopMovement()
{
    moving_ = false;
    goal_ = Goal::None;
    field_.reset();
    for (Member& m : members_) { m.velocity = glm::vec2(0); m.settled = true; }
    if (moveStatus_ == MoveStatus::Waiting || moveStatus_ == MoveStatus::Moving) moveStatus_ = MoveStatus::Idle;
}

void Group::UpdateMovement(float)
{
    if (!moving_) return;
    NavGrid& grid = world_->Navigation();
    const float largest = largest_;
    if (grid.IsConfigured() && (!field_ || field_->version != grid.Version())) RequestField();
    if (grid.IsConfigured() && !field_->ready) { moveStatus_ = MoveStatus::Waiting; return; }
    if (grid.IsConfigured() && !fieldChecked_)
    {
        // If the way leaves the window, most soldiers cannot reach the goal inside it: widen and wait.
        fieldChecked_ = true;
        size_t reach = 0, alive = 0;
        for (const Member& m : members_) if (m.alive) { ++alive; reach += field_->Reaches(grid, m.position); }
        if (fieldMargin_ >= 0 && reach * 2 < alive)
        {
            const float span = std::max(grid.Width(), grid.Height()) * grid.CellSize();
            fieldMargin_ = fieldMargin_ * 4 > span ? -1 : fieldMargin_ * 4;
            RequestField();
            moveStatus_ = MoveStatus::Waiting;
            return;
        }
    }
    moveStatus_ = MoveStatus::Moving;

    // Where the formation stands: soldiers leave the field for their slot once it is close and in sight.
    const int count = static_cast<int>(Size());
    const float formationRadius = formation_.spacing * std::sqrt(static_cast<float>(std::max(count, 1))) * 0.75f + 2 * largest;
    // Choosing a velocity is the expensive part (flow, line to the slot, neighbours). A soldier does it
    // every CrowdStride() frames, soldiers take turns; in between it keeps its velocity.
    const uint32_t stride = world_->CrowdStride();
    const uint64_t frame = world_->Frame();
    SpatialHash& hash = world_->Neighbours();
    size_t alive = 0, settled = 0;
    for (size_t i = 0; i < members_.size(); ++i)
    {
        Member& m = members_[i];
        if (!m.alive) continue;
        ++alive;
        const glm::vec2 toSlot = m.slot - m.position;
        const float distance = glm::length(toSlot);
        if (distance <= std::max(0.3f, 0.25f * m.radius) || (m.settled && distance <= m.radius))
        {
            m.settled = true;
            m.velocity = glm::vec2(0);
            ++settled;
            continue;
        }
        m.settled = false;
        // Soldiers that start moving wait for their turn too (at most a stride): when a field becomes
        // ready, its whole squad must not re-plan in one frame.
        if (stride > 1 && (i + frame) % stride != 0) continue;
        glm::vec2 direction(0);
        // Once the line to the slot is clear the soldier walks it; the line is checked again only now and then.
        if (grid.IsConfigured() && (!m.direct || (frame / stride + i) % 8 == 0))
            m.direct = glm::length(m.position - glm::vec2(goalPoint_.x, goalPoint_.z)) <= formationRadius && grid.CanWalkStraight(m.position, m.slot, m.radius);
        const bool direct = !grid.IsConfigured() || m.direct;
        if (!direct) direction = field_->Sample(grid, m.position);
        if (glm::length(direction) < 1e-3f) direction = toSlot / distance;
        float speed = m.speed;
        if (direct) speed *= std::clamp(distance / std::max(0.01f, 0.5f * m.speed), 0.3f, 1.0f);
        // Keep soldiers apart. Standing soldiers do not move, the others go around them.
        m.velocity = Detail::Steer(hash, m.position, direction * speed, m.speed, m.radius, std::max(largest, 1.0f),
            [&](const SpatialHash::Entry& other) { return other.controller == serial_ && other.member == i; });
    }

    if (alive && static_cast<float>(settled) >= arrivedFraction_ * static_cast<float>(alive))
    {
        // The order is done: stragglers stop where they are instead of drifting on.
        for (Member& m : members_) { m.velocity = glm::vec2(0); m.settled = true; }
        moving_ = false;
        goal_ = Goal::None;
        moveStatus_ = MoveStatus::Arrived;
        OnMoveFinished(MoveStatus::Arrived);
    }
    else if (world_->Time() - orderTime_ > 600)
    {
        StopMovement();
        moveStatus_ = MoveStatus::Stuck;
        OnMoveFinished(MoveStatus::Stuck);
    }
}

void Group::WriteBody(float dt)
{
    if (dt <= 0) return;
    NavGrid& grid = world_->Navigation();
    Batches batches(*world_);
    const float limit = grid.IsConfigured() ? 0.5f * grid.CellSize() : 1e9f;
    for (Member& m : members_)
    {
        if (!m.alive) continue;
        glm::vec2 step = m.velocity * dt;
        const float length = glm::length(step);
        if (length > limit) step *= limit / length;
        // Only entering a blocked cell is refused: a soldier placed inside an obstacle may walk out.
        // A step inside the same cell cannot meet a new obstacle: the grid is only asked at cell borders.
        if (grid.IsConfigured() && length > 0 && grid.CellOf(m.position + step) != grid.CellOf(m.position) &&
            grid.IsWalkable(m.position, m.radius) && !grid.IsWalkable(m.position + step, m.radius))
        {
            if (grid.IsWalkable(m.position + glm::vec2(step.x, 0), m.radius)) step.y = 0;
            else if (grid.IsWalkable(m.position + glm::vec2(0, step.y), m.radius)) step.x = 0;
            else step = glm::vec2(0);
        }
        const bool moved = glm::length(step) > 1e-6f;
        const float targetYaw = moved ? std::atan2(step.x, step.y) : (m.settled && m.hasSlot ? facing_ : m.yaw);
        const float yaw = Turn(m.yaw, targetYaw, 6.0f * dt);
        if (!moved && yaw == m.yaw) continue;
        m.position += step;
        m.yaw = yaw;
        if (m.character)
        {
            if (auto* character = dynamic_cast<Character*>(world_->Resolve(m.character)))
            {
                const glm::vec3 delta(step.x, 0, step.y);
                const float capacity = character->GetMoveSpeed() * dt;
                if (moved)
                {
                    if (capacity > 0 && glm::length(delta) <= capacity * 1.0001f) character->Move(delta / capacity, dt);
                    else character->SetPosition(character->GetLocalPosition() + delta);
                }
                character->SetRotationQuat(glm::angleAxis(yaw, glm::vec3(0, 1, 0)));
            }
            continue;
        }
        InstancedMeshComponent* batch = batches.Get(m.batch);
        if (!batch || m.index >= batch->GetMeshInstanceCount()) continue;
        if (moved) batch->SetInstancePosition(m.index, glm::vec3(m.position.x, m.height, m.position.y));
        batch->SetInstanceYaw(m.index, yaw);
    }

    // Standing soldiers that ended inside a neighbour drift apart a little, on the same turns as re-planning.
    SpatialHash& hash = world_->Neighbours();
    const uint32_t stride = world_->CrowdStride();
    const uint64_t frame = world_->Frame();
    for (size_t i = 0; i < members_.size(); ++i)
    {
        Member& m = members_[i];
        if (!m.alive || !m.settled || m.character) continue;
        if (stride > 1 && (i + frame) % stride != 0) continue;
        glm::vec2 shift(0);
        hash.Query(m.position, 2 * m.radius + 1, [&](const SpatialHash::Entry& other) {
            if (other.controller == serial_ && other.member == i) return;
            const glm::vec2 away = m.position - other.position;
            const float distance = glm::length(away);
            const float overlap = m.radius + other.radius - distance;
            if (overlap > 0.02f) shift += (distance > 1e-4f ? away / distance : glm::vec2(1, 0)) * (overlap * 0.5f);
        });
        if (glm::length(shift) < 1e-4f) continue;
        if (grid.IsConfigured() && grid.IsWalkable(m.position, m.radius) && !grid.IsWalkable(m.position + shift, m.radius)) continue;
        InstancedMeshComponent* batch = batches.Get(m.batch);
        if (!batch || m.index >= batch->GetMeshInstanceCount()) continue;
        m.position += shift;
        batch->SetInstancePosition(m.index, glm::vec3(m.position.x, m.height, m.position.y));
    }
}

void Group::SerializeSaveGame(SaveArchive& archive)
{
    Controller::SerializeSaveGame(archive);
    archive.Section("group", [&](SaveArchive& data) {
        data.Version(1);
        data.Field("facing", facing_);
        data.Field("moving", moving_);
        data.Field("spacing", formation_.spacing);
        data.Field("columns", formation_.columns);
        uint64_t count = members_.size();
        data.Field("count", count);
        if (count > 1000000) throw std::runtime_error("Group: too many saved soldiers");
        std::vector<Member> saved(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i)
            data.Section(std::to_string(i).c_str(), [&](SaveArchive& item) {
                Member& m = data.IsWriting() ? members_[static_cast<size_t>(i)] : saved[static_cast<size_t>(i)];
                uint64_t batch = m.batch, character = m.character, index = m.index;
                item.Reference("batch", batch);
                item.Reference("character", character);
                item.Field("index", index);
                item.Field("alive", m.alive);
                item.Field("settled", m.settled);
                item.Field("hasSlot", m.hasSlot);
                item.Field("slot", m.slot);
                item.Field("radius", m.radius);
                item.Field("speed", m.speed);
                m.batch = batch; m.character = character; m.index = static_cast<size_t>(index);
            });
        if (!data.IsReading()) return;
        // The game may have added the same soldiers again: keep them and take their state.
        if (members_.size() == saved.size())
            for (size_t i = 0; i < saved.size(); ++i)
            {
                // Bodies are new objects after a load: take their remapped IDs too.
                members_[i].batch = saved[i].batch;
                members_[i].character = saved[i].character;
                members_[i].index = saved[i].index;
                members_[i].alive = saved[i].alive;
                members_[i].settled = saved[i].settled;
                members_[i].hasSlot = saved[i].hasSlot;
                members_[i].slot = saved[i].slot;
            }
        else members_ = std::move(saved);
        Recount();
        reloadPositions_ = true;
        field_.reset();
        if (goal_ != Goal::None) { moving_ = true; needsPath_ = false; moveStatus_ = MoveStatus::Waiting; orderTime_ = world_->Time(); }
        else moving_ = false;
    });
}
} // namespace Ai
