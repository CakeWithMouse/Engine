#include "GeneratedComponents.h"
#include <Engine/Project/ProjectModule.h>
static ComponentTypeCatalog ProjectComponentTypes() {
    static const auto types = CombineProjectTypes(EngineGenerated::Project::GetComponentTypes());
    return {types.data(), types.size()};
}
static std::unique_ptr<Game> ProjectGameFactory() { return SelectProjectGame(EngineGenerated::Project::GetGameTypes()); }
#define SCENE_MODULE_CREATE_GAME ProjectGameFactory
#include <Engine/Project/SceneModule.inl>