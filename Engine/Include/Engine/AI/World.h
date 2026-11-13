#pragma once
#include <Engine/AI/Controller.h>
#include <Engine/MainGame/GameSubsystem.h>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class Game;
class Scene;

namespace Ai
{
class Group;
class NavGrid;
class Debugger;
class SpatialHash;
class SmartObject;
namespace Detail { class PathService; class FlowFieldCache; }

/** Time spent in each step of the last frame and how much work was done. */
struct Stats
{
    double totalMs = 0, syncMs = 0, perceptionMs = 0, thinkMs = 0, navigationMs = 0, movementMs = 0;   // syncMs: bodies and the neighbour grid
    uint32_t controllers = 0, soldiers = 0, thinks = 0, sightChecks = 0;
    uint32_t pathNodes = 0, pathsFinished = 0, fieldCells = 0, fieldsFinished = 0;
};

/** Per-frame limits: the work beyond them continues in the next frames. */
struct Budget
{
    uint32_t pathNodesPerFrame = 20000;
    uint32_t fieldCellsPerFrame = 25000;    // about 2 ms on a laptop CPU in Release (stage 10 measurement)
    uint32_t sightChecksPerFrame = 2000;
    float perceptionInterval = 0.2f;
    /** A squad soldier chooses its velocity (flow, slot, avoidance) at least this often; between those
     *  frames it keeps going. At 60 FPS that is every fourth frame, spread over the soldiers. */
    float crowdReplanInterval = 1.0f / 15.0f;
};

/**
 * Owner of every controller of one game session. The engine creates it inside a built-in
 * subsystem; games reach it with Ai::Get(game) and never declare a subsystem of their own.
 * Works without a scene too (directors only), which keeps tests free of windows and GPU.
 */
class World
{
public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void Attach(Scene* scene);
    /** Destroys controllers bound to the scene; directors and groups stay. */
    void Detach();
    Scene* GetScene() const { return scene_; }
    /** One AI step: senses, thinking, paths, movement. dt is the gameplay time of the frame. */
    void Tick(float dt);
    float Time() const { return static_cast<float>(time_); }
    /** Number of the current step, from 1. */
    uint64_t Frame() const { return frame_; }
    /** Squad soldiers re-plan every CrowdStride() frames, see Budget::crowdReplanInterval. */
    uint32_t CrowdStride() const { return crowdStride_; }

    Controller& Spawn(GameComponent& owner);
    Controller& Spawn(GameComponent& owner, std::unique_ptr<Controller> controller);
    /** No scene object: mission scripts, battle directors, any plain logic. The name keys save games. */
    Controller& SpawnDirector(std::string name = {});
    Group& CreateGroup(std::string name = {});
    /** Removed at the end of the current step; safe to call from a task. */
    void Destroy(Controller& controller);
    Controller* Find(ComponentId owner);
    Controller* FindByName(std::string_view name);
    std::vector<Controller*> Controllers() const;

    NavGrid& Navigation();
    Debugger& Debug();
    Budget& GetBudget() { return budget_; }
    const Stats& GetStats() const { return stats_; }

    // Perception
    /** Makes an object visible to AI senses, for example the player character. */
    void AddTarget(GameComponent& object, Team team);
    void RemoveTarget(ComponentId object);
    void SetAttitude(Team first, Team second, Attitude attitude);
    Attitude GetAttitude(Team first, Team second) const;
    Team TeamOf(ComponentId object) const;
    void ReportNoise(glm::vec3 position, float loudness, ObjectRef instigator = {});
    void ReportDamage(ObjectRef victim, ObjectRef instigator, float amount);
    /** Game rules for damage: health lives in the game, the AI only reports who hit whom. */
    void SetDamageHandler(std::function<void(ObjectRef victim, ObjectRef instigator, float amount)> handler);
    void Broadcast(Event event, glm::vec3 center, float radius);

    // Smart objects
    std::vector<SmartObject*> SmartObjects() const;
    /** Nearest object with the tag and a free slot within maxDistance; null when none. */
    SmartObject* FindSmartObject(const std::string& tag, glm::vec3 from, float maxDistance) const;
    void RegisterSmartObject(SmartObject& object);

    void SerializeSaveGame(SaveArchive& archive);
    /** Pointers into the old scene are gone: rebuild navigation and drop stale controllers. */
    void OnSaveGameLoaded(Scene& scene);
    /** Seed of the random numbers given to tasks; reproducible runs in tests. */
    void SetSeed(uint32_t seed) { seed_ = seed; }

    // Engine side
    GameComponent* Resolve(ComponentId id) const;
    Detail::PathService& Paths();
    Detail::FlowFieldCache& Fields();
    SpatialHash& Neighbours();
    uint32_t NextSerial() { return ++serial_; }
    uint32_t Seed() const { return seed_; }

private:
    friend class Controller;
    friend class Group;
    friend class SmartObject;
    Controller& Add(std::unique_ptr<Controller> controller);
    void Flush();
    void BakeScene();
    void UpdatePerception(float dt);
    void UpdatePerceptionOf(Controller& controller, float dt);
    void ResolveOverlaps(float largest);
    void Restore(Controller& controller);

    Scene* scene_ = nullptr;
    double time_ = 0;
    uint64_t frame_ = 0;
    uint32_t crowdStride_ = 1;
    uint32_t serial_ = 0, seed_ = 12345;
    bool ticking_ = false;
    std::vector<std::unique_ptr<Controller>> controllers_, spawned_;
    std::unique_ptr<NavGrid> nav_;
    std::unique_ptr<Debugger> debug_;
    std::unique_ptr<SpatialHash> neighbours_;
    std::unique_ptr<Detail::PathService> paths_;
    std::unique_ptr<Detail::FlowFieldCache> fields_;
    Budget budget_;
    Stats stats_;
    size_t perceptionCursor_ = 0;
    struct TargetEntry { ComponentId id; Team team; };
    std::vector<TargetEntry> targets_;
    std::vector<std::pair<uint16_t, Attitude>> attitudes_;
    std::function<void(ObjectRef, ObjectRef, float)> damageHandler_;
    std::vector<ComponentId> smartObjects_;
    // Save games: state waiting for its controller, keyed by owner ID or name.
    struct Pending { ComponentId owner = 0; std::string name; std::string json; };
    std::vector<Pending> pending_;
};

/** The built-in subsystem that owns the World; GameSession creates it for every game. */
class Subsystem final : public GameSubsystem
{
public:
    Subsystem() { bTickEnabled = true; }
    World& GetWorld() { return world_; }
    void OnSceneReady(Scene& scene) override;
    void OnSceneClosing(Scene& scene) override;
    void Tick(float dt) override;
    void SerializeSaveGame(SaveArchive& archive) override;
    void OnSaveGameLoaded(Scene& scene) override;
    void Deinitialize() noexcept override;

private:
    World world_;
};

/** The AI of this game session. Throws when called outside gameplay (Edit mode, before Play). */
World& Get(Game& game);
} // namespace Ai
