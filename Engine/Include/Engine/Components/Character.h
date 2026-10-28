#pragma once
#include <Engine/Components/SpecificComponents/CubeComponent.h>

// A scene object, not an input receiver. AI and controllers use the same Move API.
class Character : public CubeComponent
{
    float moveSpeed_ = 5;
public:
    Character();
    float GetMoveSpeed() const { return moveSpeed_; }
    void SetMoveSpeed(float);
    virtual void Move(const glm::vec3& worldDirection, float seconds);
    std::string SaveProjectData() const override;
    void LoadProjectData(const std::string&) override;
};

class SceneCamera : public GameComponent
{
public:
    bool IsRenderable() const override { return false; }
    bool CastsShadow() const override { return false; }
    virtual void Apply(Player& camera); // world transform; forward is local -Z
};

// One optional settings object per scene. IDs are remapped on load, Play and duplicate.
// Character=0 means no possession; Camera=0 explicitly selects the follow camera.
class PlaySettings : public GameComponent
{
public:
    ComponentId character = 0, camera = 0;
    bool standardControl = true;
    float cameraDistance = 5, cameraHeight = 2, lookSensitivity = 0.005f;
    bool IsRenderable() const override { return false; }
    bool CastsShadow() const override { return false; }
    std::string SaveProjectData() const override;
    void LoadProjectData(const std::string&) override;
    void RemapProjectReferences(const std::map<ComponentId,ComponentId>&) override;
};
