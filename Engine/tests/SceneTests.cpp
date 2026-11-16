#include "SceneTestAccess.h"
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/Components/ComponentType.h>
#include <Engine/Components/GameComponents.h>
#include "../ThirdParty/nlohmann/json.hpp"
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <type_traits>

// Access remains private even when the test-only inspector is included.
#define PRIVATE_SCENE_METHOD(Name) \
template<class T, class = void> struct HasPublic##Name : std::false_type {}; \
template<class T> struct HasPublic##Name<T, std::void_t<decltype(&T::Name)>> : std::true_type {}; \
static_assert(!HasPublic##Name<Scene>::value);
PRIVATE_SCENE_METHOD(RenameComponent)
PRIVATE_SCENE_METHOD(GetSnapshot)
PRIVATE_SCENE_METHOD(GetRevision)
#undef PRIVATE_SCENE_METHOD

#define CHECK(value) do { if (!(value)) throw std::runtime_error(#value); } while (false)
namespace
{
    std::unique_ptr<GameComponent> Component() { return std::make_unique<GameComponent>(); }
    class CallbackComponent final : public GameComponent
    {
    public:
        std::function<void()> callback;
        int* deaths = nullptr;
        ~CallbackComponent() { if (deaths) ++*deaths; }
        void Tick(float) override { if (callback) callback(); }
    };
}

void SceneOwnershipTests()
{
    Scene scene("First"), other("Second"); // No Game, window or GPU.
    CHECK(scene.GetId() != other.GetId());
    auto component = Component();
    auto* raw = component.get();
    CHECK(scene.AddComponent("", component).result == SceneResult::EmptyName && component);
    const auto added = scene.AddComponent("Cube", component);
    CHECK(added.result == SceneResult::Ok && !component && added.id != 0);
    CHECK(raw->GetGame() == nullptr && raw->GetScene() == &scene);
    CHECK(raw->GetComponentId() == added.id && scene.FindComponent(added.id) == raw);
    auto duplicate = Component();
    CHECK(scene.AddComponent("Cube", duplicate).result == SceneResult::DuplicateName && duplicate);
    std::unique_ptr<GameComponent> alias(raw); // Deliberately forge ownership to test rejection.
    const auto rejected = other.AddComponent("Alias", alias);
    alias.release();
    CHECK(rejected.result == SceneResult::AlreadyOwned && raw->GetScene() == &scene);
    const auto before = SceneTestAccess::Snapshot(scene);
    CHECK(SceneTestAccess::Rename(scene, added.id, "Renamed") == SceneResult::Ok);
    CHECK(!scene.FindComponentByName("Cube") && scene.FindComponentByName("Renamed") == raw);
    CHECK(before.components[0].name == "Cube");
    CHECK(SceneTestAccess::Snapshot(scene).components[0].id == added.id && SceneTestAccess::Revision(scene) > before.revision);
    CHECK(scene.RemoveComponent(added.id) == SceneResult::Ok && !scene.FindComponent(added.id));
    CHECK(scene.RemoveComponent(added.id) == SceneResult::NotFound);
    CHECK(scene.AddComponent("Cube", duplicate).id > added.id);
    CHECK(scene.SetName("") == SceneResult::EmptyName);
    CHECK(scene.SetName("Level") == SceneResult::Ok && SceneTestAccess::Snapshot(scene).name == "Level");
    CHECK(SceneTestAccess::Snapshot(other).components.empty());
}

void SceneHierarchyTests()
{
    Scene scene, other;
    auto a = Component(), b = Component(), c = Component();
    const auto parent = scene.AddComponent("Parent", a).id;
    const auto child = scene.AddComponent("Child", b).id;
    const auto foreign = other.AddComponent("Foreign", c).id;
    auto* p = scene.FindComponent(parent);
    auto* ch = scene.FindComponent(child);
    CHECK(scene.SetParent(child, parent, false) == SceneResult::Ok);
    CHECK(scene.SetParent(parent, child) == SceneResult::ParentCycle);
    CHECK(!ch->SetParent(other.FindComponent(foreign)));
    CHECK(scene.SetParent(child, 999) == SceneResult::InvalidParent);
    SceneTransform transform;
    transform.position = {10, 0, 0}; transform.scale = {2, 2, 2};
    transform.rotation = {2, 0, 0, 0};
    CHECK(scene.SetTransform(parent, transform) == SceneResult::Ok);
    transform.position = {1, 0, 0}; transform.scale = {1, 1, 1};
    CHECK(scene.SetTransform(child, transform) == SceneResult::Ok);
    CHECK(std::abs(ch->GetWorldPosition().x - 11) < 0.0001f);
    const auto snapshot = SceneTestAccess::Snapshot(scene);
    CHECK(snapshot.components[1].parentId == parent && !snapshot.components[1].inheritParentScale);
    const auto revision = SceneTestAccess::Revision(scene);
    transform.scale.x = 0;
    CHECK(scene.SetTransform(child, transform) == SceneResult::InvalidTransform);
    transform.scale.x = 1; transform.position.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(scene.SetTransform(child, transform) == SceneResult::InvalidTransform);
    transform.position.x = 1; transform.rotation = {0, 0, 0, 0};
    CHECK(scene.SetTransform(child, transform) == SceneResult::InvalidTransform);
    CHECK(SceneTestAccess::Revision(scene) == revision && ch->GetLocalPosition().x == 1);
    auto* instance = ch->CreateInstance({0, 0, 0}, {0, 0, 0}, {1, 1, 1}, {1, 1, 1, 1});
    CHECK(instance->SetParent(p));
    CHECK(scene.RemoveComponent(parent) == SceneResult::Ok);
    CHECK(!ch->GetParent() && !instance->GetParent());
    CHECK(scene.ClearParent(child) == SceneResult::Ok && snapshot.components[1].parentId == parent);
    auto unownedParent = Component(), unownedChild = Component();
    CHECK(unownedChild->SetParent(unownedParent.get()));
    CHECK(scene.AddComponent("Invalid", unownedChild).result == SceneResult::InvalidParent && unownedChild);
}

void SceneDeferredTests()
{
    Scene scene;
    int deaths = 0, laterTicks = 0;
    std::unique_ptr<GameComponent> updater = std::make_unique<CallbackComponent>();
    auto* action = static_cast<CallbackComponent*>(updater.get());
    const auto original = scene.AddComponent("A", updater).id;
    ComponentId created = 0;
    action->deaths = &deaths;
    action->callback = [&] {
        std::unique_ptr<GameComponent> next = std::make_unique<CallbackComponent>();
        static_cast<CallbackComponent*>(next.get())->callback = [&] { ++laterTicks; };
        const auto added = scene.AddComponent("B", next);
        CHECK(added.result == SceneResult::Deferred && !next);
        created = added.id;
        CHECK(!scene.FindComponent(created) && SceneTestAccess::Snapshot(scene).components.size() == 1);
        auto duplicate = Component();
        CHECK(scene.AddComponent("B", duplicate).result == SceneResult::DuplicateName && duplicate);
        CHECK(SceneTestAccess::Rename(scene, original, "Changed") == SceneResult::Busy);
        CHECK(scene.RemoveComponent(original) == SceneResult::Deferred);
        CHECK(scene.FindComponent(original) && deaths == 0);
    };
    {
        auto customUpdate = scene.BeginUpdate();
        scene.Tick(0.016f);
        CHECK(!scene.FindComponent(created) && scene.FindComponent(original));
    }
    CHECK(deaths == 1 && !scene.FindComponent(original) && scene.FindComponent(created));
    CHECK(laterTicks == 0);
    scene.Tick(0.016f);
    CHECK(laterTicks == 1);
    ComponentId cancelled = 0;
    try
    {
        auto scope = scene.BeginUpdate();
        std::unique_ptr<GameComponent> pending = std::make_unique<CallbackComponent>();
        static_cast<CallbackComponent*>(pending.get())->deaths = &deaths;
        cancelled = scene.AddComponent("Cancelled", pending).id;
        CHECK(scene.RemoveComponent(cancelled) == SceneResult::Deferred);
        throw std::runtime_error("test unwind");
    }
    catch (const std::runtime_error& error) { CHECK(std::string(error.what()) == "test unwind"); }
    CHECK(deaths == 2 && !scene.FindComponent(cancelled));
    auto finalComponent = Component();
    CHECK(scene.AddComponent("Final", finalComponent).id > cancelled);
}

void PhysicsObjectSceneTests()
{
    // Per-object rigid body settings: any component accepts them, the snapshot
    // carries them, invalid values are rejected without touching stored state.
    Scene scene;
    auto component = Component();
    const auto id = scene.AddComponent("Crate", component, "Test.Body").id;
    SceneComponentSnapshot::PhysicsBody body;
    body.enabled = true;
    body.bodyType = PhysicsBodyType::Kinematic;
    body.shapeType = PhysicsShapeType::Capsule;
    body.friction = 0.25f;
    body.restitution = 0.5f;
    body.density = 500.0f;
    CHECK(scene.SetPhysicsBody(id, body) == SceneResult::Ok);
    auto snapshot = SceneTestAccess::Snapshot(scene);
    CHECK(snapshot.components.size() == 1 && snapshot.components[0].physics.has_value());
    CHECK(snapshot.components[0].physics->enabled);
    CHECK(snapshot.components[0].physics->bodyType == PhysicsBodyType::Kinematic);
    CHECK(snapshot.components[0].physics->shapeType == PhysicsShapeType::Capsule);
    CHECK(std::abs(snapshot.components[0].physics->friction - 0.25f) < 1e-6f);
    CHECK(std::abs(snapshot.components[0].physics->density - 500.0f) < 1e-6f);
    body.density = -1.0f;
    CHECK(scene.SetPhysicsBody(id, body) == SceneResult::InvalidTransform);
    CHECK(SceneTestAccess::Snapshot(scene).components[0].physics->density == 500.0f);
    CHECK(scene.SetPhysicsBody(999, body) == SceneResult::NotFound);
    auto plain = Component();
    CHECK(scene.AddComponent("Plain", plain, "Test.Body").id != 0);
    CHECK(!SceneTestAccess::Snapshot(scene).components[1].physics.has_value());

    // Scene documents round-trip the physics block; malformed blocks are rejected.
    const auto text = SceneDocument::Serialize(SceneTestAccess::Snapshot(scene));
    const auto parsed = SceneDocument::Parse(text);
    CHECK(parsed.components[0].physics.has_value());
    CHECK(parsed.components[0].physics->bodyType == PhysicsBodyType::Kinematic);
    CHECK(parsed.components[0].physics->shapeType == PhysicsShapeType::Capsule);
    CHECK(std::abs(parsed.components[0].physics->restitution - 0.5f) < 1e-6f);
    CHECK(SceneDocument::Serialize(parsed) == text);

    auto broken = nlohmann::json::parse(text);
    broken["objects"][0]["physics"]["body"] = "floating";
    bool rejected = false;
    try { (void)SceneDocument::Parse(broken.dump()); } catch (const SceneDocumentError&) { rejected = true; }
    CHECK(rejected);
    broken = nlohmann::json::parse(text);
    broken["objects"][0]["physics"] = {{"enabled", true}};
    rejected = false;
    try { (void)SceneDocument::Parse(broken.dump()); } catch (const SceneDocumentError&) { rejected = true; }
    CHECK(rejected);

    // Restore reapplies physics to the recreated component.
    const ComponentType types[] = {{"Test.Body", "Body", EngineComponentDetail::Create<GameComponent>}};
    const ComponentTypeCatalog catalog{types, sizeof(types) / sizeof(types[0])};
    Scene restored;
    SceneDocument::Restore(parsed, catalog, restored);
    const auto* recreated = restored.FindComponentByName("Crate");
    CHECK(recreated && recreated->IsPhysicsEnabled());
    CHECK(recreated->GetPhysicsBodyType() == PhysicsBodyType::Kinematic);
    CHECK(recreated->GetPhysicsShapeType() == PhysicsShapeType::Capsule);
    CHECK(std::abs(recreated->GetPhysicsRestitution() - 0.5f) < 1e-6f);
    const auto* untouched = restored.FindComponentByName("Plain");
    CHECK(untouched && !untouched->HasPhysicsSettings());
}
