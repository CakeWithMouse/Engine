#include <Engine/Scene/Scene.h>
#include <Engine/Components/GameComponents.h>
#include <functional>
#include <limits>
#include <stdexcept>

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
    const auto before = scene.GetSnapshot();
    CHECK(scene.RenameComponent(added.id, "Renamed") == SceneResult::Ok);
    CHECK(!scene.FindComponentByName("Cube") && scene.FindComponentByName("Renamed") == raw);
    CHECK(before.components[0].name == "Cube");
    CHECK(scene.GetSnapshot().components[0].id == added.id && scene.GetRevision() > before.revision);
    CHECK(scene.RemoveComponent(added.id) == SceneResult::Ok && !scene.FindComponent(added.id));
    CHECK(scene.RemoveComponent(added.id) == SceneResult::NotFound);
    CHECK(scene.AddComponent("Cube", duplicate).id > added.id);
    CHECK(scene.SetName("") == SceneResult::EmptyName);
    CHECK(scene.SetName("Level") == SceneResult::Ok && scene.GetSnapshot().name == "Level");
    CHECK(other.GetSnapshot().components.empty());
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
    const auto snapshot = scene.GetSnapshot();
    CHECK(snapshot.components[1].parentId == parent && !snapshot.components[1].inheritParentScale);
    const auto revision = scene.GetRevision();
    transform.scale.x = 0;
    CHECK(scene.SetTransform(child, transform) == SceneResult::InvalidTransform);
    transform.scale.x = 1; transform.position.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(scene.SetTransform(child, transform) == SceneResult::InvalidTransform);
    transform.position.x = 1; transform.rotation = {0, 0, 0, 0};
    CHECK(scene.SetTransform(child, transform) == SceneResult::InvalidTransform);
    CHECK(scene.GetRevision() == revision && ch->GetLocalPosition().x == 1);
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
        CHECK(!scene.FindComponent(created) && scene.GetSnapshot().components.size() == 1);
        auto duplicate = Component();
        CHECK(scene.AddComponent("B", duplicate).result == SceneResult::DuplicateName && duplicate);
        CHECK(scene.RenameComponent(original, "Changed") == SceneResult::Busy);
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
