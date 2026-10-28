#include "RotatingCube.h"
#include "ProjectGame.h"
#include <Engine/Runtime/SceneEditSession.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#if __has_include("InputCube.h")
#include "InputCube.h"
#define TEST_INPUT_CUBE
#endif

static void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
int main() {
    try {
        RotatingCube cube;
        cube.SetPosition({2, 1, 0});
        cube.SetRotation({15, 0, 25});
        cube.Tick(1.0f);
        Check(std::abs(cube.GetRotation().y - 90.0f) < 0.001f, "90 degrees per second");
        Check(cube.GetLocalPosition() == glm::vec3(2, 1, 0), "rotation must not move cube");
        Check(cube.GetRotation().x == 15 && cube.GetRotation().z == 25, "other axes preserved");
        RotatingCube smallSteps;
        for (int frame = 0; frame < 60; ++frame) smallSteps.Tick(1.0f / 60.0f);
        Check(std::abs(smallSteps.GetRotation().y - cube.GetRotation().y) < 0.001f, "frame-rate independent rotation");
        smallSteps.Tick(3.0f);
        Check(std::abs(smallSteps.GetRotation().y) < 0.001f, "full turn wraps");
        ProjectGame game;
        game.AfterInitialize();
        Check(game.GetScene().GetComponents().size() == 1, "starter spawns cube");
        game.AfterInitialize();
        Check(game.GetScene().GetComponents().size() == 1, "saved cube is not duplicated");
        auto* spawned = dynamic_cast<RotatingCube*>(game.GetScene().FindComponentByName("RotatingCube"));
        Check(spawned && spawned->GetLocalPosition() == glm::vec3(2, 1, 0), "spawn type and position");
        ComponentType types[] = {{"RotatingCube", "RotatingCube", EngineComponentDetail::Create<RotatingCube>}};
        SceneEditSession edit({types, 1});
        auto api = edit.GetApi(); uint64_t id = 0;
        Check(api.scene.createObject(api.scene.context, "RotatingCube", "Cube", nullptr, &id) == EngineSceneOk, "editor catalog spawn");
        auto angle = [&]() {
            float result = -1;
            EngineSceneInfo info{};
            api.readActiveScene(api.context, &info, [](void* p, const EngineSceneObjectInfo* value) {
                // Quaternion y is non-zero only after rotation around Y.
                *static_cast<float*>(p) = value->transform.rotation[1];
            }, &result);
            return result;
        };
        Check(api.frame(api.context, 0.05) == EngineSceneOk, "edit frame");
        Check(angle() == 0, "Edit must not rotate");
        Check(api.control(api.context, EnginePlay) == EngineSceneOk, "Play");
        Check(api.frame(api.context, 0.05) == EngineSceneOk, "play frame");
        Check(std::abs(angle()) > 0.01f, "Play rotates cube");
        const auto paused = angle();
        api.control(api.context, EnginePause); api.frame(api.context, 0.05);
        Check(angle() == paused, "Pause freezes rotation");
        api.control(api.context, EngineStop);
        Check(angle() == 0, "Stop restores edit rotation");
#ifdef TEST_INPUT_CUBE
        {
            Game inputGame;
            auto inputCube = std::make_unique<InputCube>();
            inputCube->SetGame(&inputGame); inputCube->BeginPlay();
            auto& input = inputGame.GetInput();
            inputCube->Tick(0.1f);
            const auto before = inputCube->GetRotation().y;
            input.SetKey(Keys::R, true); input.Tick(0.1f); inputCube->Tick(0.1f);
            Check(inputCube->GetRotation().y == before, "R stops rotation");
            input.Tick(0.1f); inputCube->Tick(0.1f);
            Check(inputCube->GetRotation().y == before, "holding R does not toggle again");
            input.SetKey(Keys::R, false); input.Tick(0.01f);
            input.SetKey(Keys::R, true); input.Tick(0.01f); inputCube->Tick(0.1f);
            Check(inputCube->GetRotation().y > before, "R resumes rotation");
            input.SetKey(Keys::N, true); input.Tick(0.01f);
            Check(inputGame.GetScene().GetComponents().size() == 1, "N spawns one cube");
            input.Tick(0.01f);
            Check(inputGame.GetScene().GetComponents().size() == 1, "held N does not repeat spawn");
            inputCube.reset();
            input.SetKey(Keys::N, false); input.Tick(0.01f);
            input.SetKey(Keys::N, true); input.Tick(0.01f);
            Check(inputGame.GetScene().GetComponents().size() == 1, "destroyed owner unsubscribes");
        }
        std::cout << "Input example: R toggle, N spawn, held keys and lifetime passed\n";
#endif
        std::cout << "Starter cube: rotation, spawn, Edit/Play/Pause/Stop passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
