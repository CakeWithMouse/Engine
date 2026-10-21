#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/SkeletalAnimation.h>
#include <Engine/Render/SkeletalLoader.h>
#include <iostream>

SkeletalMeshComponent::SkeletalMeshComponent()
    : GameComponent()
{
}

SkeletalMeshComponent::SkeletalMeshComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
    : GameComponent(pos, rot, scale)
{
}

SkeletalMeshComponent::~SkeletalMeshComponent() = default;

bool SkeletalMeshComponent::LoadFromVfs(std::string_view vpath)
{
    assetPath = std::string(vpath);
    SkeletalLoadResult loadResult = LoadSkeletalModelFromVfs(vpath);
    if (!loadResult.Success || !loadResult.Model)
    {
        std::cerr << "SkeletalMeshComponent: failed to load " << vpath << ": " << loadResult.ErrorMessage << std::endl;
        return false;
    }

    return SetModel(loadResult.Model);
}

bool SkeletalMeshComponent::SetModel(std::shared_ptr<SkeletalModel> model)
{
    if (!model)
    {
        return false;
    }

    skeletalModel = std::move(model);
    animationTime = 0.0f;

    const size_t boneCount = skeletalModel->ModelSkeleton.Bones.size();
    bonePalette.resize(boneCount);

    if (boneCount > 0)
    {
        EvaluateSkeletalAnimation(*skeletalModel, currentClipIndex, animationTime, bonePalette, false);
    }

    if (GamePtr && GamePtr->GetDevice())
    {
        CreateBuffers(GamePtr->GetDevice());
    }

    return true;
}

void SkeletalMeshComponent::Tick(float deltaTime)
{
    GameComponent::Tick(deltaTime);

    if (!skeletalModel || skeletalModel->ModelSkeleton.Bones.empty())
    {
        return;
    }

    if (isPlaying)
    {
        animationTime += deltaTime * playbackSpeed;
    }

    // Evaluates bone palette in-place without dynamic heap reallocations
    EvaluateSkeletalAnimation(*skeletalModel, currentClipIndex, animationTime, bonePalette, false);
}

void SkeletalMeshComponent::CreateBuffers(ID3D11Device* /*device*/)
{
    // GPU buffer initialization will be completed in Stage 4
}

void SkeletalMeshComponent::Render(ID3D11DeviceContext* /*context*/)
{
    // GPU draw calls will be completed in Stage 4
}
