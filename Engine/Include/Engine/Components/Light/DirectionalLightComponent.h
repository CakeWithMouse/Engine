#pragma once
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/EditorProperties.h>

// One sun per Scene. World rotation turns local -Z; position and scale do not affect lighting.
class DirectionalLightComponent final : public GameComponent, public EditorPropertyProvider
{
public:
    DirectionalLightComponent();
    bool IsRenderable() const override { return false; }
    bool CastsShadow() const override { return false; }
    glm::quat GetLightRotation() const
    {
        auto rotation = GetLocalRotationQuat();
        for (auto* parent = GetParent(); parent; parent = parent->GetParent())
            rotation = parent->GetLocalRotationQuat() * rotation;
        return glm::normalize(rotation);
    }
    glm::vec3 GetDirection() const { return GetLightRotation() * glm::vec3(0, 0, -1); }
    const glm::vec3& GetLightColor() const { return color_; }
    float GetIntensity() const { return intensity_; }
    bool IsEnabled() const { return enabled_; }
    bool HasShadows() const { return shadows_; }
    float GetShadowDistance() const { return shadowDistance_; }
    std::string SaveProjectData() const override;
    void LoadProjectData(const std::string&) override;
    std::string DescribeEditorProperties() const override;
private:
    glm::vec3 color_{1.0f, 0.97f, 0.92f};
    float intensity_ = 1.0f, shadowDistance_ = 150.0f;
    bool enabled_ = true, shadows_ = true;
};
