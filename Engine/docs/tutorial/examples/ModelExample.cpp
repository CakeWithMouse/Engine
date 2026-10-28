#include "ModelExample.h"
#include <Engine/Components/GameComponents.h>
#include <Engine/Scene/SceneAssets.h>
#include <stdexcept>

ComponentId SpawnModel(Game& game, const std::string& name)
{
    // Import these files into Content/Models and Content/Textures first.
    auto model = SceneAssets::CreateModel("/Game/Models/CesiumMan.glb");
    model->SetPosition({-2.0f, 0.0f, 0.0f});
    SceneAssets::Apply(*model, model->GetModelAsset(),
                      "/Game/Textures/1.jpg");
    const auto added = game.GetScene().AddComponent(name, model, SceneAssets::ModelType);
    if (added.result != SceneResult::Ok && added.result != SceneResult::Deferred)
        throw std::runtime_error("Cannot spawn model: use a unique object name");
    return added.id;
}
