#pragma once
#include <Engine/AI/Ai.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// CHECK works in Release too, unlike assert.
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string(#condition) + "  (" + __FILE__ + ":" + std::to_string(__LINE__) + ")"); } while (false)

template<class F> bool Throws(F&& f, const char* fragment = nullptr)
{
    try { f(); } catch (const std::exception& e) { return !fragment || std::string(e.what()).find(fragment) != std::string::npos; }
    return false;
}

// Adds an object to the scene of a game that has not started yet.
template<class T> T& AddToScene(Game& game, const std::string& name, glm::vec3 position = glm::vec3(0))
{
    std::unique_ptr<GameComponent> object = std::make_unique<T>();
    T& typed = static_cast<T&>(*object);
    typed.SetPosition(position);
    const auto result = game.GetScene().AddComponent(name, object);
    if (result.result != SceneResult::Ok && result.result != SceneResult::Deferred) throw std::runtime_error("AddComponent failed: " + name);
    return typed;
}

inline void Step(Game& game, float seconds, float dt = 0.05f)
{
    for (float t = 0; t < seconds - 1e-4f; t += dt) game.StepProjectSimulation(dt);
}

inline glm::vec2 FlatOf(glm::vec3 v) { return {v.x, v.z}; }
