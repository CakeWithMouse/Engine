#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Scene/SceneAssets.h>
#include <Engine/Script/ScriptRuntime.h>
#include <nlohmann/json.hpp>
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
    api.getVariables = [](void* context, uint64_t id, EngineScriptTextVisitor visitor, void* user) -> uint32_t {
        if (!visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            const auto* object = s.Active().GetScene().FindComponent(id);
            if (!object) return EngineSceneNotFound;
            visitor(user, object->GetScriptVariables().c_str());
            return EngineSceneOk;
        });
    };
    api.setVariables = [](void* context, uint64_t id, const char* json, char* error, uint32_t capacity) -> uint32_t {
        if (!json || (capacity && !error)) return EngineSceneInvalidArgument;
        if (capacity) error[0] = 0;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            if (s.state_ != EngineEditing) return EngineSceneReadOnly;
            auto& scene = s.edit_->GetScene();
            if (!scene.FindComponent(id)) return EngineSceneNotFound;
            std::string text = json;
            if (!text.empty())
            {
                // The same rules as the scene file: an object of literals; {} clears.
                const auto values = nlohmann::json::parse(text, nullptr, false);
                bool valid = values.is_object() && text.size() <= 65536;
                if (valid)
                    for (const auto& value : values)
                        valid = valid && (value.is_boolean() || value.is_number() || value.is_string() || value.is_array());
                if (!valid)
                {
                    if (capacity) std::snprintf(error, capacity, "%s", "Script variables must be a JSON object of values");
                    return EngineSceneInvalidArgument;
                }
                text = values.empty() ? std::string() : values.dump();
            }
            return scene.SetScriptVariables(id, text) == SceneResult::Ok ? EngineSceneOk : EngineSceneFailure;
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

namespace
{
// The Play object running asset that the trace shows: objectId (edit-scene id), or the first one for 0.
const Script::Instance* TracedInstance(Game& play, Scene& edit, const char* asset, uint64_t objectId)
{
    for (const auto& entry : play.GetScene().GetComponents())
    {
        const GameComponent& object = *entry.second;
        const auto* instance = object.GetScriptInstance();
        if (!instance || object.GetScriptAsset() != asset) continue;
        if (objectId)
        {
            const auto* edited = edit.FindComponentByName(entry.first);
            if (!edited || edited->GetComponentId() != objectId) continue;
        }
        return instance;
    }
    return nullptr;
}
} // namespace

EngineGraphTraceApi SceneEditSession::GetGraphTraceApi() noexcept
{
    EngineGraphTraceApi api{};
    api.version = 1;
    api.context = this;
    api.listObjects = [](void* context, const char* asset, EngineGraphTraceObjectVisitor visitor, void* user) -> uint32_t {
        if (!asset || !visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            if (!s.play_) return EngineSceneOk;
            for (const auto& entry : s.play_->GetScene().GetComponents())
            {
                const GameComponent& object = *entry.second;
                const auto* instance = object.GetScriptInstance();
                if (!instance || object.GetScriptAsset() != asset) continue;
                const auto* edited = s.edit_->GetScene().FindComponentByName(entry.first);
                const EngineGraphTraceObject item{edited ? edited->GetComponentId() : 0, entry.first.c_str(), instance->Failed() ? 1u : 0u};
                visitor(user, &item);
            }
            return EngineSceneOk;
        });
    };
    api.read = [](void* context, const char* asset, uint64_t objectId, EngineGraphTraceNodeVisitor visitor, void* user) -> uint32_t {
        if (!asset || !visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            if (!s.play_) return EngineSceneNotFound;
            if (const auto* instance = TracedInstance(*s.play_, s.edit_->GetScene(), asset, objectId))
            {
                const int failedNode = instance->Failed() ? instance->Error().node : 0;
                bool failedSeen = false;
                for (const auto& trace : instance->Trace())
                {
                    const bool failed = trace.node == failedNode;
                    failedSeen = failedSeen || failed;
                    const EngineGraphTraceNode item{trace.node, failed ? uint32_t(EngineGraphNodeFailed) : uint32_t(EngineGraphNodeRan),
                                                    trace.framesAgo, trace.count};
                    visitor(user, &item);
                }
                if (failedNode && !failedSeen)
                {
                    const EngineGraphTraceNode item{failedNode, EngineGraphNodeFailed, 0, 0};
                    visitor(user, &item);
                }
                return EngineSceneOk;
            }
            return EngineSceneNotFound;
        });
    };
    api.readValues = [](void* context, const char* asset, uint64_t objectId, EngineGraphTraceValueVisitor visitor, void* user) -> uint32_t {
        if (!asset || !visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            if (!s.play_) return EngineSceneNotFound;
            const auto* instance = TracedInstance(*s.play_, s.edit_->GetScene(), asset, objectId);
            if (!instance) return EngineSceneNotFound;
            for (const auto& value : instance->Values())
            {
                const EngineGraphTraceValue item{value.node, value.pin.c_str(), value.text.c_str()};
                visitor(user, &item);
            }
            return EngineSceneOk;
        });
    };
    return api;
}
