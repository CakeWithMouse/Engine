#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include "Engine/Components/GameComponents.h"
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <map>
#include <nlohmann/json.hpp>
ENGINE_COMPONENT("ProjectComponent")
class ProjectComponent : public GameComponent
{
public:
    ProjectComponent() = default;
    void BeginPlay() override;
    void Tick(float deltaTime) override;
};

ENGINE_COMPONENT("Bomber Piece")
class BomberPiece final : public GameComponent {};
ENGINE_COMPONENT("Bomber Markers")
class BomberMarkers final : public InstancedMeshComponent {};
ENGINE_COMPONENT("Bomber Sparks")
class BomberSparks final : public ParticleSystemComponent {
public:
    using ParticleSystemComponent::ParticleSystemComponent;
};
ENGINE_COMPONENT("Bomber HUD")
class BomberHud final : public UIComponent {};
ENGINE_COMPONENT("Bomber Skeletal")
class BomberSkeletal final : public SkeletalMeshComponent {};

ENGINE_COMPONENT("BomberReferenceProbe")
class BomberReferenceProbe final : public GameComponent
{
public:
    uint32_t codecVersion = 2;
    float score = 73.25f;
    ComponentId targetId = 0;
    std::string DescribeEditorProperties() const override;
    std::string SaveProjectData() const override;
    void LoadProjectData(const std::string& data) override;
private:
    void RemapProjectReferences(const std::map<ComponentId, ComponentId>& remap) override;
};
