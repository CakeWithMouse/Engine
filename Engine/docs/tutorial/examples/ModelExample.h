#pragma once
#include <Engine/Scene/Scene.h>
#include <string>
class Game;
class GameComponent;

ComponentId SpawnModel(Game& game, const std::string& name,
                       const std::string& modelPath);
void AssignMaterial(GameComponent& object, const std::string& materialPath);
void AssignTexture(GameComponent& object, const std::string& texturePath);
