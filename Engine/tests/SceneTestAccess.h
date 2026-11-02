#pragma once
#include <Engine/Scene/Scene.h>
#include <utility>

// Test-only access to engine bookkeeping. Not installed with the game SDK.
struct SceneTestAccess
{
    static SceneSnapshot Snapshot(const Scene& scene) { return scene.GetSnapshot(); }
    static uint64_t Revision(const Scene& scene) { return scene.GetRevision(); }
    static SceneResult Rename(Scene& scene, ComponentId id, std::string name)
    {
        return scene.RenameComponent(id, std::move(name));
    }
};
