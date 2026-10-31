#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/EditorProperties.h>

ENGINE_COMPONENT("RotatingCube")
class RotatingCube : public CubeComponent, public EditorPropertyProvider
{
public:
    RotatingCube();
    void Tick(float deltaTime) override;
    std::string SaveProjectData() const override;
    void LoadProjectData(const std::string& data) override;
    std::string DescribeEditorProperties() const override;
private:
    float speed_ = 90.0f;
    bool enabled_ = true;
    float brightness_ = 1.0f;
};