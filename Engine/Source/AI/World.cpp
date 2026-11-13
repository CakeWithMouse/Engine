#include <Engine/AI/AiCharacter.h>
#include <Engine/AI/Debugger.h>
#include <Engine/AI/Group.h>
#include <Engine/AI/Navigation.h>
#include <Engine/AI/SmartObject.h>
#include <Engine/AI/World.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/SaveGame/SaveArchive.h>
#include <Engine/Scene/Scene.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace Ai
{
namespace
{
using Clock = std::chrono::steady_clock;
double Ms(Clock::time_point from) { return std::chrono::duration<double, std::milli>(Clock::now() - from).count(); }
glm::vec2 Flat(glm::vec3 v) { return {v.x, v.z}; }
uint16_t PairKey(Team a, Team b) { return static_cast<uint16_t>(a.id << 8 | b.id); }
} // namespace

World::World()
    : nav_(std::make_unique<NavGrid>()), debug_(std::make_unique<Debugger>(*this)), neighbours_(std::make_unique<SpatialHash>()),
      paths_(std::make_unique<Detail::PathService>()), fields_(std::make_unique<Detail::FlowFieldCache>())
{
}

World::~World()
{
    // Controllers first: their tasks get Exit while paths and grids still exist.
    spawned_.clear();
    controllers_.clear();
}

NavGrid& World::Navigation() { return *nav_; }
Debugger& World::Debug() { return *debug_; }
Detail::PathService& World::Paths() { return *paths_; }
Detail::FlowFieldCache& World::Fields() { return *fields_; }
SpatialHash& World::Neighbours() { return *neighbours_; }

GameComponent* World::Resolve(ComponentId id) const { return scene_ && id ? scene_->FindComponent(id) : nullptr; }

void World::Attach(Scene* scene)
{
    if (scene_ && scene_ != scene) Detach();
    scene_ = scene;
    BakeScene();
}

void World::Detach()
{
    // Bodies of these controllers belong to the closing scene. Directors keep running.
    auto sceneBound = [](const std::unique_ptr<Controller>& c) { return c->owner_ != 0 || dynamic_cast<Group*>(c.get()); };
    if (ticking_) { for (auto& c : controllers_) if (sceneBound(c)) c->dead_ = true; }
    else
    {
        controllers_.erase(std::remove_if(controllers_.begin(), controllers_.end(), sceneBound), controllers_.end());
        spawned_.erase(std::remove_if(spawned_.begin(), spawned_.end(), sceneBound), spawned_.end());
    }
    targets_.clear();
    smartObjects_.clear();
    paths_->Clear();
    fields_->Clear();
    nav_->Reset();
    scene_ = nullptr;
}

void World::BakeScene()
{
    nav_->Reset();
    if (!scene_) return;
    std::vector<NavVolume*> volumes;
    for (const auto& entry : scene_->GetComponents())
        if (auto* volume = dynamic_cast<NavVolume*>(entry.second.get())) volumes.push_back(volume);
    glm::vec2 low(1e30f), high(-1e30f);
    float cell = 0;
    for (NavVolume* volume : volumes)
        if (volume->mode == 0)
        {
            glm::vec2 a, b;
            volume->GetArea(a, b);
            low = glm::min(low, a);
            high = glm::max(high, b);
            if (cell == 0) cell = volume->cellSize;
        }
    if (cell == 0) return; // no bounds volume: open field until code configures the grid
    nav_->SetBounds(low, high, cell);
    for (NavVolume* volume : volumes)
    {
        glm::vec2 a, b;
        volume->GetArea(a, b);
        if (volume->mode == 1) nav_->BlockBox(a, b);
        else if (volume->mode == 2) nav_->SetCost(a, b, static_cast<uint8_t>(std::clamp(volume->cost, 1, 254)));
    }
}

Controller& World::Add(std::unique_ptr<Controller> controller)
{
    Controller& added = *controller;
    (ticking_ ? spawned_ : controllers_).push_back(std::move(controller));
    return added;
}

Controller& World::Spawn(GameComponent& owner) { return Spawn(owner, std::make_unique<Controller>(*this)); }

Controller& World::Spawn(GameComponent& owner, std::unique_ptr<Controller> controller)
{
    if (!controller) throw std::invalid_argument("Ai::World::Spawn: null controller");
    if (&controller->GetWorld() != this) throw std::invalid_argument("Ai::World::Spawn: the controller belongs to another world");
    const ComponentId id = owner.GetComponentId();
    if (!id || !scene_ || scene_->FindComponent(id) != &owner)
        throw std::logic_error("Ai::World::Spawn: the object is not in the active scene (call it from BeginPlay)");
    if (Find(id)) throw std::logic_error("Ai::World::Spawn: the object already has an AI controller");
    controller->owner_ = id;
    controller->name_ = "object " + std::to_string(id);
    controller->SyncBody();
    controller->OnPossess(owner);
    return Add(std::move(controller));
}

Controller& World::SpawnDirector(std::string name)
{
    auto controller = std::make_unique<Controller>(*this);
    controller->name_ = name.empty() ? "director " + std::to_string(controller->serial_) : std::move(name);
    return Add(std::move(controller));
}

Group& World::CreateGroup(std::string name)
{
    auto group = std::make_unique<Group>(*this);
    group->name_ = name.empty() ? "group " + std::to_string(group->serial_) : std::move(name);
    Group& created = *group;
    Add(std::move(group));
    return created;
}

void World::Destroy(Controller& controller)
{
    controller.dead_ = true;
    if (!ticking_) Flush();
}

Controller* World::Find(ComponentId owner)
{
    if (!owner) return nullptr;
    for (auto* list : {&controllers_, &spawned_})
        for (auto& c : *list) if (!c->dead_ && c->owner_ == owner) return c.get();
    return nullptr;
}

Controller* World::FindByName(std::string_view name)
{
    for (auto* list : {&controllers_, &spawned_})
        for (auto& c : *list) if (!c->dead_ && c->name_ == name) return c.get();
    return nullptr;
}

std::vector<Controller*> World::Controllers() const
{
    std::vector<Controller*> all;
    for (auto* list : {&controllers_, &spawned_})
        for (auto& c : *list) if (!c->dead_) all.push_back(c.get());
    return all;
}

void World::Flush()
{
    for (auto& c : spawned_) controllers_.push_back(std::move(c));
    spawned_.clear();
    // Destroying runs Exit of tasks, which may destroy other controllers: loop until stable.
    for (bool removed = true; removed;)
    {
        removed = false;
        for (size_t i = 0; i < controllers_.size(); ++i)
            if (controllers_[i]->dead_)
            {
                std::unique_ptr<Controller> dead = std::move(controllers_[i]);
                controllers_.erase(controllers_.begin() + static_cast<std::ptrdiff_t>(i));
                dead.reset();
                removed = true;
                break;
            }
        for (auto& c : spawned_) controllers_.push_back(std::move(c));
        spawned_.clear();
    }
}

void World::Tick(float dt)
{
    if (!std::isfinite(dt) || dt < 0) throw std::invalid_argument("Ai::World::Tick: invalid dt");
    if (ticking_) throw std::logic_error("Ai::World::Tick: recursive tick");
    const auto start = Clock::now();
    stats_ = {};
    Flush();
    struct Guard { World& world; ~Guard() { world.ticking_ = false; } } guard{*this};
    ticking_ = true;
    time_ += dt;
    ++frame_;
    crowdStride_ = dt > 0 ? std::max(1u, static_cast<uint32_t>(budget_.crowdReplanInterval / dt + 1e-3f)) : 1u;

    // 1. Positions; controllers of removed objects die.
    auto stepStart = Clock::now();
    for (auto& c : controllers_) if (!c->dead_ && !c->SyncBody()) c->dead_ = true;

    // 2. Neighbours: bodies, squad soldiers, plain targets.
    neighbours_->Begin();
    float largest = 1;
    for (auto& c : controllers_)
    {
        if (c->dead_) continue;
        if (auto* group = dynamic_cast<Group*>(c.get()))
        {
            for (size_t i = 0; i < group->members_.size(); ++i)
            {
                const auto& m = group->members_[i];
                if (!m.alive) continue;
                neighbours_->Add({m.position, m.radius, m.character, c->serial_, static_cast<uint32_t>(i), c->perception_.team_, m.settled, m.velocity});
                largest = std::max(largest, m.radius);
            }
            stats_.soldiers += static_cast<uint32_t>(group->Size());
            continue;
        }
        if (!c->owner_) continue;
        neighbours_->Add({Flat(c->position_), c->radius_, c->owner_, c->serial_, 0, c->perception_.team_,
                           c->moveStatus_ != MoveStatus::Moving && c->moveStatus_ != MoveStatus::Waiting, Flat(c->velocity_)});
        largest = std::max(largest, c->radius_);
    }
    targets_.erase(std::remove_if(targets_.begin(), targets_.end(), [&](const TargetEntry& t) { return !Resolve(t.id); }), targets_.end());
    for (const auto& target : targets_)
        if (!Find(target.id)) neighbours_->Add({Flat(Resolve(target.id)->GetWorldPosition()), 0.4f, target.id, 0, 0, target.team, true});
    neighbours_->Build(std::max(4.0f, 4 * largest));   // 2x was measured: slower
    stats_.syncMs = Ms(stepStart);

    // 3. Senses.
    stepStart = Clock::now();
    UpdatePerception(dt);
    stats_.perceptionMs = Ms(stepStart);

    // 4. Decisions.
    stepStart = Clock::now();
    for (size_t i = 0; i < controllers_.size(); ++i)
        if (!controllers_[i]->dead_) { controllers_[i]->Think(dt); ++stats_.thinks; }
    stats_.thinkMs = Ms(stepStart);

    // 5. Paths and fields within the budget.
    stepStart = Clock::now();
    if (nav_->IsConfigured())
    {
        paths_->Process(*nav_, budget_.pathNodesPerFrame, stats_);
        fields_->Process(*nav_, budget_.fieldCellsPerFrame, stats_);
    }
    stats_.navigationMs = Ms(stepStart);

    // 6-8. Desired velocity, avoidance, writing to bodies.
    stepStart = Clock::now();
    for (auto& c : controllers_) if (!c->dead_) c->UpdateMovement(dt);
    for (auto& c : controllers_)
    {
        if (c->dead_ || !c->owner_ || glm::length(c->desired_) < 1e-5f) continue;
        // Movers steer around neighbours; standing units are obstacles and are not pushed.
        const glm::vec2 v = Detail::Steer(*neighbours_, Flat(c->position_), Flat(c->desired_), glm::length(c->desired_), c->radius_, largest,
            [&](const SpatialHash::Entry& other) { return other.controller == c->serial_ && other.object == c->owner_; });
        c->desired_ = glm::vec3(v.x, 0, v.y);
    }
    for (auto& c : controllers_) if (!c->dead_) c->WriteBody(dt);
    ResolveOverlaps(largest);
    stats_.movementMs = Ms(stepStart);

    ticking_ = false;
    Flush();
    stats_.controllers = static_cast<uint32_t>(controllers_.size());
    pending_.clear(); // restored controllers had their BeginPlay before this step
    if (debug_->IsVisible()) debug_->Update();
    stats_.totalMs = Ms(start);
}

// Two standing units that ended inside each other (both stopped at one point) drift apart a little
// every frame. Units nobody touches are never moved.
void World::ResolveOverlaps(float largest)
{
    for (auto& c : controllers_)
    {
        if (c->dead_ || !c->owner_ || !c->isCharacter_ || c->moveStatus_ == MoveStatus::Moving || c->moveStatus_ == MoveStatus::Waiting) continue;
        glm::vec2 shift(0);
        neighbours_->Query(Flat(c->position_), c->radius_ + largest, [&](const SpatialHash::Entry& other) {
            if ((other.controller == c->serial_ && other.object == c->owner_) || !other.settled) return;
            const glm::vec2 away = Flat(c->position_) - other.position;
            const float distance = glm::length(away);
            const float overlap = c->radius_ + other.radius - distance;
            if (overlap > 0.02f) shift += (distance > 1e-4f ? away / distance : glm::vec2(1, 0)) * (overlap * 0.5f);
        });
        if (glm::length(shift) < 1e-4f) continue;
        if (nav_->IsConfigured() && !nav_->IsWalkable(Flat(c->position_) + shift, c->radius_)) continue;
        if (GameComponent* body = Resolve(c->owner_))
        {
            body->SetPosition(body->GetLocalPosition() + glm::vec3(shift.x, 0, shift.y));
            c->position_ += glm::vec3(shift.x, 0, shift.y);
        }
    }
}

// Perception
void World::AddTarget(GameComponent& object, Team team)
{
    const ComponentId id = object.GetComponentId();
    if (!id) throw std::logic_error("Ai::World::AddTarget: the object is not in a scene");
    for (auto& target : targets_) if (target.id == id) { target.team = team; return; }
    targets_.push_back({id, team});
}

void World::RemoveTarget(ComponentId object)
{
    targets_.erase(std::remove_if(targets_.begin(), targets_.end(), [&](const TargetEntry& t) { return t.id == object; }), targets_.end());
}

void World::SetAttitude(Team first, Team second, Attitude attitude)
{
    for (const uint16_t key : {PairKey(first, second), PairKey(second, first)})
    {
        auto found = std::find_if(attitudes_.begin(), attitudes_.end(), [&](const auto& a) { return a.first == key; });
        if (found != attitudes_.end()) found->second = attitude;
        else attitudes_.push_back({key, attitude});
    }
}

Attitude World::GetAttitude(Team first, Team second) const
{
    const uint16_t key = PairKey(first, second);
    for (const auto& a : attitudes_) if (a.first == key) return a.second;
    if (first == second) return Attitude::Friendly;
    if (first.id == 0 || second.id == 0) return Attitude::Neutral;
    return Attitude::Hostile;
}

Team World::TeamOf(ComponentId object) const
{
    for (const auto* list : {&controllers_, &spawned_})
        for (const auto& c : *list) if (!c->dead_ && c->owner_ == object) return c->perception_.team_;
    for (const auto& target : targets_) if (target.id == object) return target.team;
    return {};
}

void World::UpdatePerception(float dt)
{
    for (auto& c : controllers_)
    {
        if (c->dead_) continue;
        c->perception_.sinceUpdate_ += dt;
        for (auto& known : c->perception_.known_) if (!known.visibleNow) known.age += dt;
    }
    const size_t count = controllers_.size();
    uint32_t checks = 0;
    for (size_t k = 0; k < count && checks < budget_.sightChecksPerFrame; ++k)
    {
        const size_t index = (perceptionCursor_ + k) % count;
        Controller& c = *controllers_[index];
        if (c.dead_ || !c.perception_.SightEnabled() || c.perception_.sinceUpdate_ + 1e-6f < budget_.perceptionInterval) continue;
        const uint32_t before = stats_.sightChecks;
        UpdatePerceptionOf(c, c.perception_.sinceUpdate_);
        c.perception_.sinceUpdate_ = 0;
        checks += stats_.sightChecks - before + 1;
        perceptionCursor_ = index + 1;
    }
    // Forget what is too old.
    for (auto& c : controllers_)
    {
        if (c->dead_) continue;
        auto& known = c->perception_.known_;
        const size_t before = known.size();
        known.erase(std::remove_if(known.begin(), known.end(), [&](const Perceived& p) {
            return !p.visibleNow && (p.age > c->perception_.memory_ || !Resolve(p.who.id)); }), known.end());
        if (before != known.size() && known.empty() && !c->perception_.targetKey_.empty())
            c->memory_.Clear(Key<ObjectRef>{c->perception_.targetKey_.c_str()});
    }
}

void World::UpdatePerceptionOf(Controller& c, float)
{
    Perception& p = c.perception_;
    const glm::vec2 self = Flat(c.position_);
    const glm::vec2 forward(std::sin(c.yaw_), std::cos(c.yaw_));
    const float halfAngle = glm::radians(std::min(p.fieldOfView_, 360.0f)) * 0.5f;
    std::vector<std::pair<ComponentId, glm::vec2>> visible;
    neighbours_->Query(self, p.sightRadius_, [&](const SpatialHash::Entry& e) {
        if (!e.object || e.object == c.owner_ || e.controller == c.serial_) return;
        if (GetAttitude(p.team_, e.team) != Attitude::Hostile) return;
        const glm::vec2 delta = e.position - self;
        const float distance = glm::length(delta);
        if (p.fieldOfView_ < 360.0f && distance > 1e-3f && std::acos(std::clamp(glm::dot(delta / distance, forward), -1.0f, 1.0f)) > halfAngle) return;
        ++stats_.sightChecks;
        if (nav_->IsConfigured() && !nav_->HasLineOfSight(c.position_, glm::vec3(e.position.x, c.position_.y, e.position.y))) return;
        visible.push_back({e.object, e.position});
    });
    bool spotted = false;
    for (auto& known : p.known_)
    {
        const auto seen = std::find_if(visible.begin(), visible.end(), [&](const auto& v) { return v.first == known.who.id; });
        if (seen == visible.end() && known.visibleNow)
        {
            known.visibleNow = false;
            Stimulus lost{Stimulus::Kind::Sight, known.who, known.lastPosition, 0, true};
            c.OnPerception(lost);
        }
    }
    for (const auto& [id, position] : visible)
    {
        GameComponent* object = Resolve(id);
        const float y = object ? object->GetWorldPosition().y : c.position_.y;
        auto known = std::find_if(p.known_.begin(), p.known_.end(), [&](const Perceived& k) { return k.who.id == id; });
        const bool fresh = known == p.known_.end() || !known->visibleNow;
        if (known == p.known_.end()) { p.known_.push_back({}); known = p.known_.end() - 1; known->who = {id}; }
        known->lastPosition = glm::vec3(position.x, y, position.y);
        known->age = 0;
        known->visibleNow = true;
        known->distance = glm::length(position - self);
        if (fresh)
        {
            spotted = true;
            c.OnPerception({Stimulus::Kind::Sight, {id}, known->lastPosition, 0, false});
        }
    }
    const bool anyVisible = !visible.empty();
    if (spotted) c.SendEvent(Events::Spotted);
    if (p.anyVisible_ && !anyVisible) c.SendEvent(Events::Lost);
    p.anyVisible_ = anyVisible;
    if (const Perceived* best = p.Best())
    {
        if (!p.targetKey_.empty()) c.memory_.Set(Key<ObjectRef>{p.targetKey_.c_str()}, best->who);
        if (!p.lastSeenKey_.empty()) c.memory_.Set(Key<glm::vec3>{p.lastSeenKey_.c_str()}, best->lastPosition);
    }
}

void World::ReportNoise(glm::vec3 position, float loudness, ObjectRef instigator)
{
    const Team source = instigator ? TeamOf(instigator.id) : Team{};
    for (auto& c : controllers_)
    {
        Perception& p = c->perception_;
        if (c->dead_ || p.hearingRadius_ <= 0 || c->owner_ == instigator.id) continue;
        if (instigator && GetAttitude(p.team_, source) == Attitude::Friendly) continue;
        if (glm::length(Flat(position) - Flat(c->position_)) > p.hearingRadius_ * std::max(loudness, 0.0f)) continue;
        if (instigator)
        {
            auto known = std::find_if(p.known_.begin(), p.known_.end(), [&](const Perceived& k) { return k.who == instigator; });
            if (known == p.known_.end()) { p.known_.push_back({}); known = p.known_.end() - 1; known->who = instigator; }
            known->lastPosition = position;
            known->age = 0;
            known->distance = glm::length(Flat(position) - Flat(c->position_));
        }
        if (!p.lastSeenKey_.empty()) c->memory_.Set(Key<glm::vec3>{p.lastSeenKey_.c_str()}, position);
        c->OnPerception({Stimulus::Kind::Hearing, instigator, position, loudness, false});
        c->SendEvent(Events::Heard);
    }
}

void World::ReportDamage(ObjectRef victim, ObjectRef instigator, float amount)
{
    if (damageHandler_) damageHandler_(victim, instigator, amount);
    Controller* c = Find(victim.id);
    if (!c) return;
    glm::vec3 where = c->position_;
    if (GameComponent* source = instigator ? Resolve(instigator.id) : nullptr)
    {
        where = source->GetWorldPosition();
        Perception& p = c->perception_;
        auto known = std::find_if(p.known_.begin(), p.known_.end(), [&](const Perceived& k) { return k.who == instigator; });
        if (known == p.known_.end()) { p.known_.push_back({}); known = p.known_.end() - 1; known->who = instigator; }
        known->lastPosition = where;
        known->age = 0;
        known->distance = glm::length(Flat(where) - Flat(c->position_));
        if (!p.targetKey_.empty() && !c->memory_.Has(Key<ObjectRef>{p.targetKey_.c_str()})) c->memory_.Set(Key<ObjectRef>{p.targetKey_.c_str()}, instigator);
        if (!p.lastSeenKey_.empty()) c->memory_.Set(Key<glm::vec3>{p.lastSeenKey_.c_str()}, where);
    }
    c->OnPerception({Stimulus::Kind::Damage, instigator, where, amount, false});
    c->SendEvent(Events::Damaged);
}

void World::SetDamageHandler(std::function<void(ObjectRef, ObjectRef, float)> handler) { damageHandler_ = std::move(handler); }

void World::Broadcast(Event event, glm::vec3 center, float radius)
{
    for (auto* c : Controllers())
        if (c->owner_ || dynamic_cast<Group*>(c))
            if (glm::length(Flat(c->position_) - Flat(center)) <= radius) c->SendEvent(event);
}

std::vector<SmartObject*> World::SmartObjects() const
{
    std::vector<SmartObject*> objects;
    for (const ComponentId id : smartObjects_)
        if (auto* object = dynamic_cast<SmartObject*>(Resolve(id))) objects.push_back(object);
    return objects;
}

SmartObject* World::FindSmartObject(const std::string& tag, glm::vec3 from, float maxDistance) const
{
    SmartObject* best = nullptr;
    float bestDistance = maxDistance;
    for (SmartObject* object : SmartObjects())
    {
        if (object->tag != tag || object->FreeSlots() <= 0) continue;
        const float distance = glm::length(Flat(object->GetWorldPosition()) - Flat(from));
        if (distance <= bestDistance) { best = object; bestDistance = distance; }
    }
    return best;
}

void World::RegisterSmartObject(SmartObject& object)
{
    const ComponentId id = object.GetComponentId();
    if (id && std::find(smartObjects_.begin(), smartObjects_.end(), id) == smartObjects_.end()) smartObjects_.push_back(id);
}

// Save games
void World::SerializeSaveGame(SaveArchive& archive)
{
    archive.Version(1);
    std::vector<Controller*> saved;
    if (archive.IsWriting())
        for (auto* c : Controllers())
            if (c->owner_ || !c->name_.empty()) saved.push_back(c);
    uint64_t count = saved.size();
    archive.Field("count", count);
    if (count > 100000) throw std::runtime_error("Ai: too many saved controllers");
    if (archive.IsReading()) pending_.clear();
    for (uint64_t i = 0; i < count; ++i)
        archive.Section(("c" + std::to_string(i)).c_str(), [&](SaveArchive& item) {
            Controller* c = archive.IsWriting() ? saved[static_cast<size_t>(i)] : nullptr;
            uint64_t owner = c ? c->owner_ : 0;
            std::string name = c ? c->name_ : std::string();
            std::string state = c ? c->ActiveState() : std::string();
            bool group = c && dynamic_cast<Group*>(c);
            item.Reference("owner", owner);
            item.Field("name", name);
            item.Field("state", state);
            item.Field("group", group);
            if (archive.IsWriting()) { item.Section("controller", [&](SaveArchive& data) { c->SerializeSaveGame(data); }); return; }
            // Read now with remapped references into a stand-in, keep the result for the real controller.
            std::unique_ptr<Controller> standIn = group ? std::make_unique<Group>(*this) : std::make_unique<Controller>(*this);
            nlohmann::json raw = nlohmann::json::object();
            item.Section("controller", [&](SaveArchive& data) { raw = data.Raw(); standIn->SerializeSaveGame(data); });
            nlohmann::json remapped;
            SaveArchive writer(remapped, SaveArchive::Mode::Write);
            standIn->SerializeSaveGame(writer);
            raw.merge_patch(remapped);
            nlohmann::json pending = {{"state", state}, {"controller", raw}};
            pending_.push_back({static_cast<ComponentId>(owner), owner ? std::string() : name, pending.dump()});
        });
}

void World::Restore(Controller& controller)
{
    auto found = std::find_if(pending_.begin(), pending_.end(), [&](const Pending& p) {
        return controller.owner_ ? p.owner == controller.owner_ : (!p.name.empty() && p.name == controller.name_); });
    if (found == pending_.end()) return;
    nlohmann::json data = nlohmann::json::parse(found->json);
    pending_.erase(found);
    SaveArchive reader(data["controller"], SaveArchive::Mode::Read);
    controller.SerializeSaveGame(reader);
    const std::string state = data.value("state", "");
    if (!state.empty())
    {
        Context ctx(*this, controller);
        if (!controller.machine_.Restore(ctx, state)) controller.Log("saved state \"" + state + "\" is not in the machine; starting over");
    }
}

void World::OnSaveGameLoaded(Scene& scene)
{
    // Old bodies are gone; their controllers would die at the next step anyway.
    for (auto& c : controllers_) if (c->owner_ && !scene.FindComponent(c->owner_)) c->dead_ = true;
    if (!ticking_) Flush();
    targets_.clear();
    smartObjects_.clear();
    paths_->Clear();
    fields_->Clear();
    scene_ = &scene;
    BakeScene();
    // Directors and groups outlive the scene: they take their saved state now. Units get theirs
    // when their BeginPlay runs the machine again.
    for (Controller* c : Controllers())
        if (!c->owner_) Restore(*c);
}

// Subsystem
void Subsystem::OnSceneReady(Scene& scene) { world_.Attach(&scene); }
void Subsystem::OnSceneClosing(Scene&) { world_.Detach(); }
void Subsystem::Tick(float dt) { world_.Tick(dt); }
void Subsystem::SerializeSaveGame(SaveArchive& archive)
{
    GameSubsystem::SerializeSaveGame(archive);
    archive.Section("Ai", [&](SaveArchive& data) { world_.SerializeSaveGame(data); });
}
void Subsystem::OnSaveGameLoaded(Scene& scene) { world_.OnSaveGameLoaded(scene); }
void Subsystem::Deinitialize() noexcept
{
    try { world_.Detach(); } catch (...) {}
}

World& Get(Game& game)
{
    auto* subsystem = game.GetSubsystem<Subsystem>();
    if (!subsystem) throw std::logic_error("Ai::Get: AI exists only during gameplay (not in Edit mode, not before Play)");
    return subsystem->GetWorld();
}
} // namespace Ai
