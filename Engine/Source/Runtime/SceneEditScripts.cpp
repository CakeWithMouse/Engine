#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Scene/SceneAssets.h>
#include <Engine/Script/ScriptRuntime.h>
#include "../Resources/VfsRead.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace
{
void Visit(EngineScriptProblemVisitor visitor, void* user, uint64_t objectId, const std::string& objectName, const std::string& script,
           const Script::Problem& problem, bool runtime)
{
    const EngineScriptProblem item{objectId, objectName.c_str(), script.c_str(), problem.node, problem.message.c_str(), runtime ? 1u : 0u};
    visitor(user, &item);
}
} // namespace

EngineScriptApi SceneEditSession::GetScriptApi() noexcept
{
    EngineScriptApi api{};
    api.version = 1;
    api.context = this;
    api.describe = [](void* context, EngineScriptTextVisitor visitor, void* user) -> uint32_t {
        if (!visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession&) -> uint32_t {
            const std::string catalog = Script::CatalogJson();
            visitor(user, catalog.c_str());
            return EngineSceneOk;
        });
    };
    api.get = [](void* context, uint64_t id, EngineScriptTextVisitor visitor, void* user) -> uint32_t {
        if (!visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            const auto* object = s.Active().GetScene().FindComponent(id); // same scene as readActiveScene
            if (!object) return EngineSceneNotFound;
            visitor(user, object->GetScriptAsset().c_str());
            return EngineSceneOk;
        });
    };
    api.set = [](void* context, uint64_t id, const char* path, char* error, uint32_t capacity) -> uint32_t {
        if (!path || (capacity && !error)) return EngineSceneInvalidArgument;
        if (capacity) error[0] = 0;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            if (s.state_ != EngineEditing) return EngineSceneReadOnly;
            auto& scene = s.edit_->GetScene();
            if (!scene.FindComponent(id)) return EngineSceneNotFound;
            try
            {
                SceneAssets::ValidateScript(path);
            }
            catch (const std::exception& e)
            {
                if (capacity) std::snprintf(error, capacity, "%s", e.what());
                return EngineSceneInvalidArgument;
            }
            return scene.SetScriptAsset(id, path) == SceneResult::Ok ? EngineSceneOk : EngineSceneFailure;
        });
    };
    api.check = [](void* context, const char* path, EngineScriptProblemVisitor visitor, void* user) -> uint32_t {
        if (!path || !visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession&) -> uint32_t {
            const std::string script = path;
            if (!SceneAssets::ValidPath(script) || script.empty()) return EngineSceneInvalidArgument;
            const std::string physical = VfsRead::Active().Resolve(script);
            std::ifstream input(std::filesystem::u8path(physical.empty() ? script : physical), std::ios::binary);
            if (!input)
            {
                Visit(visitor, user, 0, {}, script, {0, "cannot read the file"}, false);
                return EngineSceneOk;
            }
            std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            if (text.compare(0, 3, "\xef\xbb\xbf") == 0) text.erase(0, 3);
            for (const auto& problem : Script::Compile(text).problems) Visit(visitor, user, 0, {}, script, problem, false);
            return EngineSceneOk;
        });
    };
    api.readProblems = [](void* context, EngineScriptProblemVisitor visitor, void* user) -> uint32_t {
        if (!visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            if (!s.play_) return EngineSceneOk;
            Game& game = *s.play_;
            for (const auto& entry : game.GetScene().GetComponents())
            {
                const GameComponent& object = *entry.second; // keyed by object name
                const std::string& script = object.GetScriptAsset();
                if (script.empty()) continue;
                // The Play copy has its own ids; the editor knows the edit scene object of the same name.
                const auto* edited = s.edit_->GetScene().FindComponentByName(entry.first);
                const uint64_t id = edited ? edited->GetComponentId() : 0;
                if (const auto* instance = object.GetScriptInstance())
                {
                    if (instance->Failed()) Visit(visitor, user, id, entry.first, script, instance->Error(), true);
                    continue;
                }
                for (const auto& problem : game.GetScriptProblems(script)) Visit(visitor, user, id, entry.first, script, problem, false);
            }
            return EngineSceneOk;
        });
    };
    return api;
}
