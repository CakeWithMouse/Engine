#include <Engine/Scene/SceneCommands.h>
#include <cmath>
#include <stdexcept>

namespace
{
uint32_t Status(SceneResult result)
{
    switch (result)
    {
    case SceneResult::Ok: return EngineSceneOk;
    case SceneResult::NotFound: return EngineSceneNotFound;
    case SceneResult::DuplicateName: return EngineSceneDuplicateName;
    case SceneResult::Busy: return EngineSceneBusy;
    case SceneResult::EmptyName: case SceneResult::InvalidTransform: return EngineSceneInvalidArgument;
    default: return EngineSceneFailure;
    }
}
SceneTransform Decode(const EngineSceneTransform& t)
{
    return {{t.position[0], t.position[1], t.position[2]},
        {t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]},
        {t.scale[0], t.scale[1], t.scale[2]}};
}
bool Valid(const SceneTransform& t)
{
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(t.position[i]) || !std::isfinite(t.scale[i]) || std::abs(t.scale[i]) < 1e-6f) return false;
    const auto length2 = glm::dot(t.rotation, t.rotation);
    return std::isfinite(length2) && length2 >= 1e-12f;
}
EngineSceneTransform Encode(const SceneTransform& t)
{
    return {{t.position.x, t.position.y, t.position.z},
        {t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w}, {t.scale.x, t.scale.y, t.scale.z}};
}
}
SceneCommands::SceneCommands(Scene& scene, ComponentTypeCatalog catalog, bool* sharedBusy)
    : scene_(&scene), catalog_(catalog), thread_(std::this_thread::get_id()), sharedBusy_(sharedBusy)
{
    if (catalog.size && !catalog.data) throw std::invalid_argument("Null component catalog");
    for (size_t i = 0; i < catalog.size; ++i)
    {
        const auto& type = catalog.data[i];
        if (!type.typeName || !*type.typeName || !type.displayName || !type.create)
            throw std::invalid_argument("Invalid component descriptor");
        for (size_t j = 0; j < i; ++j)
            if (std::string_view(type.typeName) == catalog.data[j].typeName)
                throw std::invalid_argument("Duplicate component type");
    }
}
EngineSceneApi SceneCommands::GetApi() noexcept
{
    return {ENGINE_SCENE_API_VERSION, this, ListTypes, ReadScene, CreateObject,
        RemoveObject, RenameObject, SetTransform};
}
uint32_t SceneCommands::ListTypes(void* context, EngineSceneTypeVisitor visit, void* user) noexcept
{
    if (!visit) return EngineSceneInvalidArgument;
    return Invoke(context, [&](SceneCommands& self) -> uint32_t {
        for (size_t i = 0; i < self.catalog_.size; ++i)
        {
            const auto& t = self.catalog_.data[i];
            const EngineSceneTypeInfo info{t.typeName, t.displayName, t.visualDefaults.geometryAsset, t.visualDefaults.materialAsset};
            visit(user, &info);
        }
        return EngineSceneOk;
    });
}
uint32_t SceneCommands::ReadScene(void* context, EngineSceneInfo* info, EngineSceneObjectVisitor visit, void* user) noexcept
{
    if (!info || !visit) return EngineSceneInvalidArgument;
    *info = {};
    return Invoke(context, [&](SceneCommands& self) -> uint32_t {
        const auto snapshot = self.scene_->GetSnapshot();
        *info = {snapshot.id, snapshot.revision, snapshot.components.size()};
        for (const auto& object : snapshot.components)
        {
            const EngineSceneObjectInfo value{object.id, object.name.c_str(), object.typeName.c_str(),
                object.parentId, object.inheritParentScale ? 1u : 0u, Encode(object.transform),
                object.modelAsset.c_str(), object.textureAsset.c_str()};
            visit(user, &value);
        }
        return EngineSceneOk;
    });
}
uint32_t SceneCommands::CreateObject(void* context, const char* typeName, const char* name,
    const EngineSceneTransform* transform, uint64_t* id) noexcept
{
    if (!id) return EngineSceneInvalidArgument;
    *id = 0;
    if (!typeName || !*typeName || !name || !*name) return EngineSceneInvalidArgument;
    return Invoke(context, [&](SceneCommands& self) -> uint32_t {
        const auto* type = self.catalog_.Find(typeName);
        if (self.readOnly_) return EngineSceneReadOnly;
        if (!type) return EngineSceneUnknownType;
        if (self.scene_->FindComponentByName(name)) return EngineSceneDuplicateName;
        SceneTransform initial;
        if (transform) { initial = Decode(*transform); if (!Valid(initial)) return EngineSceneInvalidArgument; }
        auto object = type->create(); // Constructor failures are caught BEFORE touching Scene.
        if (!object) return EngineSceneFailure;
        if (transform)
        {
            object->SetPosition(initial.position);
            object->SetRotationQuat(initial.rotation);
            object->SetScale(initial.scale);
        }
        const auto added = self.scene_->AddComponent(name, object, type->typeName);
        if (added.result == SceneResult::Ok) *id = added.id;
        return Status(added.result);
    });
}
uint32_t SceneCommands::RemoveObject(void* context, uint64_t id) noexcept
{
    return Invoke(context, [&](SceneCommands& self) { return self.readOnly_ ? EngineSceneReadOnly : Status(self.scene_->RemoveComponent(id)); });
}
uint32_t SceneCommands::RenameObject(void* context, uint64_t id, const char* name) noexcept
{
    if (!name || !*name) return EngineSceneInvalidArgument;
    return Invoke(context, [&](SceneCommands& self) { return self.readOnly_ ? EngineSceneReadOnly : Status(self.scene_->RenameComponent(id, name)); });
}
uint32_t SceneCommands::SetTransform(void* context, uint64_t id, const EngineSceneTransform* transform) noexcept
{
    if (!transform) return EngineSceneInvalidArgument;
    return Invoke(context, [&](SceneCommands& self) { return self.readOnly_ ? EngineSceneReadOnly : Status(self.scene_->SetTransform(id, Decode(*transform))); });
}
