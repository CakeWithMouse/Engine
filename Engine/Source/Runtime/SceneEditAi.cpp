// Engine.Scene.AiMachine: .fsm state machines for the AI editor (catalog, check, Play debugger).
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/AI/Ai.h>
#include <Engine/Scene/SceneAssets.h>

#include <cstdio>
#include <string>
#include <vector>

namespace
{
struct Unit
{
    std::string name;
    const AiCharacter* character = nullptr;
    const Ai::Controller* ai = nullptr;
    uint64_t objectId = 0; // the edit-scene object of the same name
};

// AI characters of the Play scene, in scene order. machine: "" — every one.
std::vector<Unit> Units(Game& play, Scene& edit, const std::string& machine)
{
    std::vector<Unit> units;
    for (const auto& entry : play.GetScene().GetComponents())
    {
        const auto* character = dynamic_cast<const AiCharacter*>(&*entry.second);
        const Ai::Controller* ai = character ? character->GetAi() : nullptr;
        if (!ai || (!machine.empty() && character->stateMachine != machine)) continue;
        const auto* edited = edit.FindComponentByName(entry.first);
        units.push_back({entry.first, character, ai, edited ? edited->GetComponentId() : 0});
    }
    return units;
}

bool FindUnit(Game& play, Scene& edit, uint64_t objectId, Unit& found)
{
    for (const Unit& unit : Units(play, edit, {}))
        if (!objectId || unit.objectId == objectId)
        {
            found = unit;
            return true;
        }
    return false;
}

// The .fsm the character runs, if its machine came from one: gives node ids of the states.
std::shared_ptr<const Ai::MachineAsset> AssetOf(const Unit& unit)
{
    if (!Ai::IsMachineFile(unit.character->stateMachine)) return nullptr;
    const auto loaded = Ai::LoadMachineFile(unit.character->stateMachine);
    return loaded.asset && loaded.asset->machine == unit.ai->Machine() ? loaded.asset : nullptr;
}

std::string Number(float value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%g", value);
    return text;
}

std::string ValueText(Game& play, const Ai::Blackboard::Value& value)
{
    return std::visit([&](const auto& typed) -> std::string {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, bool>) return typed ? "true" : "false";
        else if constexpr (std::is_same_v<T, int>) return std::to_string(typed);
        else if constexpr (std::is_same_v<T, float>) return Number(typed);
        else if constexpr (std::is_same_v<T, glm::vec3>) return "(" + Number(typed.x) + ", " + Number(typed.y) + ", " + Number(typed.z) + ")";
        else if constexpr (std::is_same_v<T, std::string>) return typed;
        else
        {
            if (!typed) return "none";
            for (const auto& entry : play.GetScene().GetComponents())
                if (entry.second->GetComponentId() == typed.id) return entry.first;
            return "removed #" + std::to_string(typed.id);
        }
    }, value);
}
} // namespace

EngineAiMachineApi SceneEditSession::GetAiMachineApi() noexcept
{
    EngineAiMachineApi api{};
    api.version = 1;
    api.context = this;
    api.describe = [](void* context, EngineAiTextVisitor visitor, void* user) -> uint32_t {
        if (!visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession&) -> uint32_t {
            const std::string catalog = Ai::MachineCatalogJson();
            visitor(user, catalog.c_str());
            return EngineSceneOk;
        });
    };
    api.check = [](void* context, const char* path, EngineAiProblemVisitor visitor, void* user) -> uint32_t {
        if (!path || !visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession&) -> uint32_t {
            const std::string machine = path;
            if (machine.empty() || !SceneAssets::ValidPath(machine) || !Ai::IsMachineFile(machine)) return EngineSceneInvalidArgument;
            for (const auto& problem : Ai::LoadMachineFile(machine).problems)
            {
                const EngineAiProblem item{problem.node, problem.message.c_str()};
                visitor(user, &item);
            }
            return EngineSceneOk;
        });
    };
    api.listUnits = [](void* context, const char* machine, EngineAiUnitVisitor visitor, void* user) -> uint32_t {
        if (!machine || !visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            if (!s.play_) return EngineSceneOk;
            for (const Unit& unit : Units(*s.play_, s.edit_->GetScene(), machine))
            {
                const std::string active = unit.ai->ActiveState();
                const EngineAiUnit item{unit.objectId, unit.name.c_str(), unit.character->stateMachine.c_str(), active.c_str(), unit.ai->IsRunning() ? 1u : 0u};
                visitor(user, &item);
            }
            return EngineSceneOk;
        });
    };
    api.readStates = [](void* context, uint64_t objectId, EngineAiStateVisitor visitor, void* user) -> uint32_t {
        if (!visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            Unit unit;
            if (!s.play_ || !FindUnit(*s.play_, s.edit_->GetScene(), objectId, unit)) return EngineSceneNotFound;
            const auto asset = AssetOf(unit);
            const std::string active = unit.ai->ActiveState();
            size_t begin = 0;
            while (begin < active.size())
            {
                const size_t end = std::min(active.find('/', begin), active.size());
                const std::string name = active.substr(begin, end - begin), path = active.substr(0, end);
                int node = 0;
                if (asset)
                    if (const auto found = asset->stateNodes.find(path); found != asset->stateNodes.end()) node = found->second;
                const EngineAiState item{node, name.c_str(), path.c_str()};
                visitor(user, &item);
                begin = end + 1;
            }
            return EngineSceneOk;
        });
    };
    api.readMemory = [](void* context, uint64_t objectId, EngineAiMemoryVisitor visitor, void* user) -> uint32_t {
        if (!visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            Unit unit;
            if (!s.play_ || !FindUnit(*s.play_, s.edit_->GetScene(), objectId, unit)) return EngineSceneNotFound;
            for (const auto& [key, value] : unit.ai->Memory().Values())
            {
                const std::string text = ValueText(*s.play_, value);
                const EngineAiMemoryValue item{key.c_str(), Ai::ToString(static_cast<Ai::VariableType>(value.index())), text.c_str()};
                visitor(user, &item);
            }
            return EngineSceneOk;
        });
    };
    api.readHistory = [](void* context, uint64_t objectId, EngineAiTextVisitor visitor, void* user) -> uint32_t {
        if (!visitor) return EngineSceneInvalidArgument;
        return Invoke(context, [&](SceneEditSession& s) -> uint32_t {
            Unit unit;
            if (!s.play_ || !FindUnit(*s.play_, s.edit_->GetScene(), objectId, unit)) return EngineSceneNotFound;
            for (const std::string& line : unit.ai->History()) visitor(user, line.c_str());
            return EngineSceneOk;
        });
    };
    return api;
}
