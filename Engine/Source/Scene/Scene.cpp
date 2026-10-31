#include <Engine/Scene/Scene.h>
#include <Engine/Components/Character.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/Light/DirectionalLightComponent.h>
#include <atomic>
#include <cmath>
#include <stdexcept>

namespace
{
    std::atomic<SceneId> nextSceneId{1};
    bool Finite(const glm::vec3& v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }
}

Scene::Scene(std::string name) : id_(nextSceneId.fetch_add(1)), name_(std::move(name))
{
    if (name_.empty()) throw std::invalid_argument("Scene name must not be empty");
}
Scene::~Scene() = default;

SceneResult Scene::SetName(std::string name)
{
    if (name.empty()) return SceneResult::EmptyName;
    if (name_ != name) { name_ = std::move(name); ++revision_; }
    return SceneResult::Ok;
}

SceneResult Scene::ValidateAddition(std::string_view name, const GameComponent* component) const
{
    if (!component) return SceneResult::NullComponent;
    if (name.empty()) return SceneResult::EmptyName;
    if (components_.count(name) || pendingAdds_.count(name)) return SceneResult::DuplicateName;
    if (component->RegisteredScene) return SceneResult::AlreadyOwned;
    if (dynamic_cast<const DirectionalLightComponent*>(component))
    {
        for (const auto* entries : {&components_, &pendingAdds_})
            for (const auto& entry : *entries)
                if (dynamic_cast<const DirectionalLightComponent*>(entry.second.get()))
                    return SceneResult::DuplicateSingleton;
    }
    const auto* parent = component->GetParent();
    if (parent && (parent->RegisteredScene != this || !byId_.count(parent->SceneComponentId)))
        return SceneResult::InvalidParent;
    return SceneResult::Ok;
}

AddComponentResult Scene::AddComponent(std::string name, std::unique_ptr<GameComponent>& component, std::string typeName)
{
    const auto result = ValidateAddition(name, component.get());
    if (result != SceneResult::Ok) return {result, 0};
    auto& entries = updateDepth_ ? pendingAdds_ : components_;
    auto& ids = updateDepth_ ? pendingIds_ : byId_;
    const ComponentId id = nextId_++;
    // Allocate both indices before transferring ownership.
    std::string componentName = name;
    auto entry = entries.emplace(std::move(name), nullptr).first;
    try { ids.emplace(id, component.get()); }
    catch (...) { entries.erase(entry); throw; }
    component->RegisteredScene = this;
    component->SceneComponentId = id;
    component->SceneName = std::move(componentName);
    component->SceneTypeName = std::move(typeName);
    entry->second = std::move(component);
    if (!updateDepth_) ++revision_;
    return {updateDepth_ ? SceneResult::Deferred : SceneResult::Ok, id};
}

GameComponent* Scene::FindComponent(ComponentId id)
{
    auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}
const GameComponent* Scene::FindComponent(ComponentId id) const
{
    auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}
GameComponent* Scene::FindComponentByName(std::string_view name)
{
    auto it = components_.find(name);
    return it == components_.end() ? nullptr : it->second.get();
}
const GameComponent* Scene::FindComponentByName(std::string_view name) const
{
    auto it = components_.find(name);
    return it == components_.end() ? nullptr : it->second.get();
}

SceneResult Scene::RemoveComponent(ComponentId id)
{
    if (!byId_.count(id) && !pendingIds_.count(id)) return SceneResult::NotFound;
    if (updateDepth_) { pendingRemoves_.insert(id); return SceneResult::Deferred; }
    RemoveNow(id);
    return SceneResult::Ok;
}
void Scene::RemoveNow(ComponentId id) noexcept
{
    auto it = byId_.find(id);
    if (it == byId_.end()) return;
    auto* removed = it->second;
    for (auto& entry : components_) entry.second->ClearParentReferences(removed);
    for (auto& entry : components_) if (auto* settings=dynamic_cast<PlaySettings*>(entry.second.get())) {
        if(settings->character==id)settings->character=0;
        if(settings->camera==id)settings->camera=0;
    }
    auto entry = components_.find(removed->SceneName);
    byId_.erase(it);
    components_.erase(entry);
    ++revision_;
}
void Scene::Clear() noexcept
{
    pendingRemoves_.clear();
    pendingIds_.clear(); pendingAdds_.clear();
    byId_.clear(); components_.clear();
    ++revision_;
}

SceneResult Scene::RenameComponent(ComponentId id, std::string name)
{
    auto* component = FindComponent(id);
    if (!component) return SceneResult::NotFound;
    if (name.empty()) return SceneResult::EmptyName;
    if (name == component->SceneName) return SceneResult::Ok;
    if (components_.count(name) || pendingAdds_.count(name)) return SceneResult::DuplicateName;
    if (updateDepth_) return SceneResult::Busy; // Preserve the current traversal order.
    std::string componentName = name;
    auto node = components_.extract(component->SceneName);
    node.key() = std::move(name);
    component->SceneName = std::move(componentName);
    components_.insert(std::move(node));
    ++revision_;
    return SceneResult::Ok;
}

SceneResult Scene::SetTransform(ComponentId id, const SceneTransform& transform)
{
    auto* component = FindComponent(id);
    if (!component) return SceneResult::NotFound;
    const auto& q = transform.rotation;
    const float length2 = glm::dot(q, q);
    if (!Finite(transform.position) || !Finite(transform.scale) ||
        !std::isfinite(length2) || length2 < 1e-12f ||
        std::abs(transform.scale.x) < 1e-6f || std::abs(transform.scale.y) < 1e-6f ||
        std::abs(transform.scale.z) < 1e-6f) return SceneResult::InvalidTransform;
    component->SetPosition(transform.position);
    component->SetScale(transform.scale);
    component->SetRotationQuat(q);
    ++revision_;
    return SceneResult::Ok;
}

SceneResult Scene::SetParent(ComponentId childId, ComponentId parentId, bool inheritScale)
{
    auto* child = FindComponent(childId);
    if (!child) return SceneResult::NotFound;
    auto* parent = FindComponent(parentId);
    if (!parent) return SceneResult::InvalidParent;
    const bool ok = inheritScale ? child->SetParent(parent) : child->SetParentWithoutScale(parent);
    if (!ok) return SceneResult::ParentCycle;
    ++revision_;
    return SceneResult::Ok;
}
SceneResult Scene::ClearParent(ComponentId id)
{
    auto* child = FindComponent(id);
    if (!child) return SceneResult::NotFound;
    child->SetParent(nullptr);
    ++revision_;
    return SceneResult::Ok;
}

SceneSnapshot Scene::GetSnapshot() const
{
    SceneSnapshot result{id_, name_, revision_, {}};
    result.documentVersion = documentVersion_;
    result.components.reserve(byId_.size());
    for (const auto& entry : byId_)
    {
        const auto& c = *entry.second;
        result.components.push_back({entry.first, c.SceneName,
            c.GetParent() ? c.GetParent()->SceneComponentId : 0, c.bApplyParentScale,
            {c.GetLocalPosition(), c.bUseQuaternionRotation ? c.LocalRotationQuaternion :
                glm::quat(glm::radians(c.ComponentRotation)), c.GetScale()}, c.SceneTypeName,
                c.GetModelAsset(), c.GetTexturePath(), c.GetMaterialAsset()});
        if (const auto* light = dynamic_cast<const PointLightComponent*>(&c))
            result.components.back().pointLight = {light->GetLightColor(), light->GetIntensity(), light->GetRadius(), light->IsEnabled()};
        result.components.back().projectData = c.SaveProjectData();
        if (!result.components.back().projectData.empty()) result.documentVersion = 3;
    }
    return result;
}

bool SceneComponentSnapshot::PointLight::IsValid() const noexcept
{
    return Finite(color) && color.x >= 0 && color.x <= 1 && color.y >= 0 && color.y <= 1 &&
        color.z >= 0 && color.z <= 1 && std::isfinite(intensity) && intensity >= 0 &&
        std::isfinite(radius) && radius > 0;
}

void Scene::SetDocumentVersion(uint32_t version)
{
    if (version < 1 || version > 3) throw std::invalid_argument("Unsupported scene format");
    documentVersion_ = version;
}

SceneResult Scene::SetPointLight(ComponentId id, const SceneComponentSnapshot::PointLight& light)
{
    if (IsUpdating()) return SceneResult::Busy;
    auto* object = FindComponent(id);
    if (!object) return SceneResult::NotFound;
    auto* component = dynamic_cast<PointLightComponent*>(object);
    if (!component || !light.IsValid()) return SceneResult::InvalidTransform;
    component->SetLightColor(light.color);
    component->SetIntensity(light.intensity);
    component->SetRadius(light.radius);
    component->SetEnabled(light.enabled);
    ++revision_;
    return SceneResult::Ok;
}

SceneResult Scene::SetTextureAsset(ComponentId id, const std::string& path)
{
    if (IsUpdating()) return SceneResult::Busy;
    auto* object = FindComponent(id);
    if (!object) return SceneResult::NotFound;
    if (!object->TrySetTexture(path)) throw std::runtime_error("Cannot load texture: " + path);
    ++revision_;
    return SceneResult::Ok;
}

SceneResult Scene::SetMaterialAsset(ComponentId id, const std::string& path)
{
    if (IsUpdating()) return SceneResult::Busy;
    auto* object = FindComponent(id);
    if (!object) return SceneResult::NotFound;
    if (!object->SetMaterialAsset(path)) throw std::runtime_error("This object cannot take a material");
    ++revision_;
    return SceneResult::Ok;
}

Scene::UpdateScope::UpdateScope(Scene& scene) : scene_(scene) { ++scene_.updateDepth_; }
Scene::UpdateScope::~UpdateScope() { scene_.EndUpdate(); }
void Scene::EndUpdate() noexcept
{
    if (--updateDepth_) return;
    const bool added = !pendingAdds_.empty();
    components_.merge(pendingAdds_);
    byId_.merge(pendingIds_);
    if (added) ++revision_;
    for (auto id : pendingRemoves_) RemoveNow(id);
    pendingRemoves_.clear();
}
void Scene::Tick(float dt, bool pauseNonSkybox)
{
    if (ticking_) throw std::logic_error("Recursive Scene::Tick");
    auto scope = BeginUpdate();
    ticking_ = true;
    try
    {
        for (auto& entry : components_)
            entry.second->Tick(pauseNonSkybox && !entry.second->IsSkybox() ? 0.0f : dt);
    }
    catch (...) { ticking_ = false; throw; }
    ticking_ = false;
}
