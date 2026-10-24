#include "GeneratedComponents.h"
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <cstdio>
#include <memory>

static ComponentTypeCatalog ProjectComponentTypes() { return EngineGenerated::RuntimeExample::GetComponentTypes(); }

static void StartSkeletalDemo(Game& game)
{
#if defined(ENGINE_ENABLE_ASSIMP)
    auto skeletal = std::make_unique<SkeletalMeshComponent>();
    if (skeletal->LoadFromVfs("/Game/Models/CesiumMan.glb"))
    {
        // Walks towards camera along +Z
        skeletal->SetModelYawDegrees(0.0f);
        skeletal->SetPosition({0.0f, -1.0f, 0.0f});
        std::printf("[RuntimeExample] SkeletalMesh loaded: bones=%zu clip=%.2fs\n",
                    skeletal->GetModel()->ModelSkeleton.Bones.size(),
                    skeletal->GetModel()->Clips.empty() ? 0.0f : skeletal->GetModel()->Clips[0].Duration);
        const auto reg = game.RegisterComponent("CesiumMan", skeletal.get(), "SkinnedMesh.hlsl", "SkinnedMesh.hlsl");
        if (reg == RegisterResult::Ok)
        {
            skeletal.release();
        }
    }
    else
    {
        std::fprintf(stderr, "[RuntimeExample] SkeletalMesh failed to load CesiumMan.glb\n");
        std::fflush(stderr);
    }
#else
    (void)game;
    std::printf("[RuntimeExample] SkeletalMesh skipped: Assimp not enabled\n");
    std::fflush(stdout);
#endif
}

#define SCENE_MODULE_START_SCENE StartSkeletalDemo
#include "../../Common/SceneModule.inl"
#undef SCENE_MODULE_START_SCENE
