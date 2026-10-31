#include "ModelExample.h"
#include <Engine/Components/GameComponents.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Scene/SceneAssets.h>
#include <stdexcept>

ComponentId SpawnModel(Game& game, const std::string& name,
                       const std::string& modelPath) {
    auto model = SceneAssets::CreateModel(modelPath);
    model->SetPosition({-2.0f, 0.0f, 0.0f});
    const auto added = game.GetScene().AddComponent(
        name, model, SceneAssets::ModelType);
    if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
        throw std::runtime_error("Cannot spawn model: " + name);
    return added.id;
}

void AssignMaterial(GameComponent& object, const std::string& materialPath) {
    SceneAssets::ValidateMaterial(materialPath);
    if (!object.SetMaterialAsset(materialPath))
        throw std::runtime_error("Material unsupported");
}

void AssignTexture(GameComponent& object, const std::string& texturePath) {
    if (!object.TrySetTexture(texturePath))
        throw std::runtime_error("Cannot set texture: " + texturePath);
}
