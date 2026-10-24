#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Scene/SceneAssets.h>
#include "../Resources/VfsRead.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
template<class F> uint32_t AssetCall(char* error, uint32_t capacity, F&& action)
{
    if (capacity && !error) return EngineSceneInvalidArgument;
    if (capacity) error[0] = 0;
    try { return action(); }
    catch (const std::exception& e) { if (capacity) std::snprintf(error, capacity, "%s", e.what()); }
    catch (...) { if (capacity) std::snprintf(error, capacity, "Asset operation failed"); }
    return EngineSceneFailure;
}
bool Text(const char* value)
{
    return value && *value && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, nullptr, 0);
}
}
SceneEditSession::~SceneEditSession()
{
    play_.reset(); edit_.reset();
    if (ownsGameMount_)
    {
        auto& vfs = VfsRead::GetVFS();
        if (vfs.Resolve("/Game") == contentRoot_)
        {
            if (previousGameRoot_.empty()) vfs.Unmount("/Game");
            else vfs.Mount("/Game", previousGameRoot_);
        }
    }
}
void SceneEditSession::AddAssetApi(EngineEditSessionApi& api) noexcept
{
    api.setContentRoot = [](void* p, const char* root, char* error, uint32_t capacity) -> uint32_t {
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            if (s.state_ != EngineEditing) return EngineSceneReadOnly;
            return AssetCall(error, capacity, [&]() -> uint32_t {
                if (!Text(root)) return EngineSceneInvalidArgument;
                auto path = std::filesystem::u8path(root);
                if (!path.is_absolute() || !std::filesystem::is_directory(path)) return EngineSceneInvalidArgument;
                path = std::filesystem::canonical(path);
                if (!s.contentRoot_.empty()) return path == s.contentRoot_ ? EngineSceneOk : EngineSceneBusy;
                auto& vfs = VfsRead::GetVFS();
                if (auto old = vfs.Resolve("/Game")) s.previousGameRoot_ = *old;
                vfs.Mount("/Game", path);
                s.contentRoot_ = path; s.ownsGameMount_ = true;
                return EngineSceneOk;
            });
        });
    };
    api.importAsset = [](void* p, const char* source, uint32_t kind, char* path, uint32_t pathCapacity,
        char* error, uint32_t capacity) -> uint32_t {
        if (path && pathCapacity) path[0] = 0;
        if (!path || pathCapacity < 4096 || kind > 1 || !Text(source)) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            if (s.state_ != EngineEditing) return EngineSceneReadOnly;
            return AssetCall(error, capacity, [&]() -> uint32_t {
                const auto result = SceneAssets::Import(s.contentRoot_, std::filesystem::u8path(source), static_cast<SceneAssets::Kind>(kind));
                std::memcpy(path, result.c_str(), result.size() + 1);
                return EngineSceneOk;
            });
        });
    };
    api.createModel = [](void* p, const char* path, const char* name, const EngineSceneTransform* transform,
        uint64_t* id, char* error, uint32_t capacity) -> uint32_t {
        if (!id) return EngineSceneInvalidArgument;
        *id = 0;
        if (!Text(path) || !Text(name)) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            if (s.state_ != EngineEditing) return EngineSceneReadOnly;
            return AssetCall(error, capacity, [&]() -> uint32_t {
                auto& scene = s.edit_->GetScene();
                if (scene.FindComponentByName(name)) return EngineSceneDuplicateName;
                SceneTransform t;
                if (transform)
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        t.position[i] = transform->position[i]; t.scale[i] = transform->scale[i];
                        if (!std::isfinite(t.position[i]) || !std::isfinite(t.scale[i]) || std::abs(t.scale[i]) < 1e-6f)
                            return EngineSceneInvalidArgument;
                    }
                    t.rotation = {transform->rotation[3], transform->rotation[0], transform->rotation[1], transform->rotation[2]};
                    const auto length = glm::dot(t.rotation, t.rotation);
                    if (!std::isfinite(length) || length < 1e-12f) return EngineSceneInvalidArgument;
                }
                auto object = SceneAssets::CreateModel(path);
                object->SetPosition(t.position); object->SetScale(t.scale); object->SetRotationQuat(glm::normalize(t.rotation));
                const auto added = scene.AddComponent(name, object, SceneAssets::ModelType);
                if (added.result != SceneResult::Ok) return EngineSceneFailure;
                *id = added.id;
                return EngineSceneOk;
            });
        });
    };
    api.setTexture = [](void* p, uint64_t id, const char* path, char* error, uint32_t capacity) -> uint32_t {
        if (!path) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            if (s.state_ != EngineEditing) return EngineSceneReadOnly;
            return AssetCall(error, capacity, [&]() -> uint32_t {
                auto& scene = s.edit_->GetScene();
                if (!scene.FindComponent(id)) return EngineSceneNotFound;
                SceneAssets::ValidateTexture(path);
                return scene.SetTextureAsset(id, path) == SceneResult::Ok ? EngineSceneOk : EngineSceneFailure;
            });
        });
    };
}
