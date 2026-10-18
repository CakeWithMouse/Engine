#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

class GameComponent;
class Game;
using SceneId = uint64_t;
using ComponentId = uint64_t;
// Zero is invalid. IDs are process-local, not persistent asset identifiers.

enum class SceneResult
{
    Ok, Deferred, NullComponent, EmptyName, DuplicateName, AlreadyOwned,
    NotFound, InvalidParent, ParentCycle, InvalidTransform, Busy
};
struct AddComponentResult
{
    SceneResult result;
    ComponentId id = 0;
};
struct SceneTransform
{
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};
struct SceneComponentSnapshot
{
    ComponentId id = 0;
    std::string name;
    ComponentId parentId = 0;
    bool inheritParentScale = true;
    SceneTransform transform;
    std::string typeName; // copied qualified catalog key; empty for legacy additions
};
struct SceneSnapshot
{
    SceneId id = 0;
    std::string name;
    uint64_t revision = 0;
    std::vector<SceneComponentSnapshot> components;
};

// Engine-thread API. No window/device is needed for storage and editing.
// See docs/SCENE-EDITOR-API.md for ownership, update boundaries and DLL limitations.
class Scene final
{
public:
    explicit Scene(std::string name = "Scene");
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;

    SceneId GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    SceneResult SetName(std::string name);
    uint64_t GetRevision() const noexcept { return revision_; }

    // Ok/Deferred transfer ownership and clear component; all failures retain it.
    // Deferred additions have a reserved ID/name but are not visible until EndUpdate.
    AddComponentResult AddComponent(std::string name, std::unique_ptr<GameComponent>& component, std::string typeName = {});
    bool IsUpdating() const noexcept { return updateDepth_ != 0; }
    SceneResult RemoveComponent(ComponentId id);
    GameComponent* FindComponent(ComponentId id);
    const GameComponent* FindComponent(ComponentId id) const;
    GameComponent* FindComponentByName(std::string_view name);
    const GameComponent* FindComponentByName(std::string_view name) const;
    SceneResult RenameComponent(ComponentId id, std::string name);
    SceneResult SetTransform(ComponentId id, const SceneTransform& transform);
    SceneResult SetParent(ComponentId child, ComponentId parent, bool inheritScale = true);
    SceneResult ClearParent(ComponentId child);
    SceneSnapshot GetSnapshot() const;

    // Covers custom game update order too. The outermost scope commits pending edits,
    // including when unwinding an exception. Scopes must not outlive their Scene.
    class UpdateScope
    {
    public:
        ~UpdateScope();
        UpdateScope(const UpdateScope&) = delete;
        UpdateScope& operator=(const UpdateScope&) = delete;
    private:
        friend class Scene;
        explicit UpdateScope(Scene& scene);
        Scene& scene_;
    };
    UpdateScope BeginUpdate() { return UpdateScope(*this); }
    void Tick(float deltaTime, bool pauseNonSkybox = false);

    // Borrowed engine-only views; do not keep iterators across mutations.
    using ComponentMap = std::map<std::string, std::unique_ptr<GameComponent>, std::less<>>;
    const ComponentMap& GetComponents() const noexcept { return components_; }
    const std::map<ComponentId, GameComponent*>& GetComponentsById() const noexcept { return byId_; }

private:
    friend class Game;
    SceneResult ValidateAddition(std::string_view name, const GameComponent* component) const;
    void EndUpdate() noexcept;
    void RemoveNow(ComponentId id) noexcept;
    void Clear() noexcept;
    SceneId id_;
    std::string name_;
    uint64_t revision_ = 0;
    ComponentId nextId_ = 1;
    ComponentMap components_, pendingAdds_;
    std::map<ComponentId, GameComponent*> byId_, pendingIds_;
    std::set<ComponentId> pendingRemoves_;
    unsigned updateDepth_ = 0;
    bool ticking_ = false;
};
