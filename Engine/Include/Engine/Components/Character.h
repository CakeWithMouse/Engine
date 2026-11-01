#pragma once
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/PropertyMacros.h>

struct CameraProperties {
    bool orthographic=false;
    float fov=60, nearClip=0.1f, farClip=10000, orthoHeight=10;
    int viewPlane=0; // transform, XY, XZ, YZ
    template<class Self> static void Visit(Self& self, EngineProperties::Visitor& v) {
        v.Enum("orthographic",self.orthographic,"Проекция",{{false,"Перспектива"},{true,"Ортографическая (2D)"}});
        v.Field("fov",self.fov,"Угол обзора (градусы)",1,179);
        v.Field("nearClip",self.nearClip,"Ближняя граница",0.001,100000);
        v.Field("farClip",self.farClip,"Дальняя граница",0.002,1000000);
        v.Field("orthoHeight",self.orthoHeight,"Высота 2D-вида",0.001,1000000);
        v.Enum("viewPlane",self.viewPlane,"Плоскость просмотра",{{0,"По повороту камеры"},{1,"XY — спереди"},{2,"XZ — сверху"},{3,"YZ — сбоку"}});
    }
    void Validate() const { if(farClip<=nearClip)throw std::invalid_argument("Far clip must be greater than near clip"); }
    void ApplyProjection(Player&) const;
    glm::vec3 Forward() const;
    glm::vec3 Up() const;
};

// A scene object, not an input receiver. AI and controllers use the same Move API.
class Character : public GameComponent
{
    float moveSpeed_ = 5;
    bool needsLegacySettings_ = true;
    template<class Self> static void Visit(Self& self, EngineProperties::Visitor& v) {
        v.Field("moveSpeed",self.moveSpeed_,"Скорость движения",0,10000);
        v.Field("playerControlled",self.playerControlled,"Игрок");
        v.Field("standardControl",self.standardControl,"Стандартное управление");
        v.Field("camera",self.camera,"Камера (0 — слежение)",0,1e12);
        v.Field("cameraDistance",self.cameraDistance,"Дистанция камеры",0,10000);
        v.Field("cameraHeight",self.cameraHeight,"Высота камеры",-10000,10000);
        v.Field("lookSensitivity",self.lookSensitivity,"Чувствительность камеры",0,1);
        CameraProperties::Visit(self.cameraSettings,v);
    }
public:
    Character();
    void SetPlayerControlled(bool enabled) {playerControlled=enabled;needsLegacySettings_=false;}
    bool playerControlled=false, standardControl=true;
    ComponentId camera=0;
    float cameraDistance=5, cameraHeight=2, lookSensitivity=0.005f;
    CameraProperties cameraSettings;
    // @engine-internal-begin
private:
    void RemapProjectReferences(const std::map<ComponentId,ComponentId>&) override;
public:
    // @engine-internal-end
    void ImportLegacySettings(const class PlaySettings&);
    float GetMoveSpeed() const { return moveSpeed_; }
    void SetMoveSpeed(float);
    std::string DescribeEditorProperties() const override;
    virtual void Move(const glm::vec3& worldDirection, float seconds);
    std::string SaveProjectData() const override;
    void LoadProjectData(const std::string&) override;
};

class PlayerCharacter : public Character { public: PlayerCharacter() { SetPlayerControlled(true); } };

class SceneCamera : public GameComponent, public CameraProperties
{
public:
    bool IsRenderable() const override { return false; }
    bool CastsShadow() const override { return false; }
    ENGINE_PROPERTIES(GameComponent,
        ENGINE_ENUM_PROPERTY_FIELD(orthographic, "Проекция", {false,"Перспектива"}, {true,"Ортографическая (2D)"})
        ENGINE_PROPERTY_FIELD(fov, "Угол обзора (градусы)", 1, 179)
        ENGINE_PROPERTY_FIELD(nearClip, "Ближняя граница", 0.001, 100000)
        ENGINE_PROPERTY_FIELD(farClip, "Дальняя граница", 0.002, 1000000)
        ENGINE_PROPERTY_FIELD(orthoHeight, "Высота 2D-вида", 0.001, 1000000)
        ENGINE_ENUM_PROPERTY_FIELD(viewPlane, "Плоскость просмотра", {0,"По повороту камеры"}, {1,"XY — спереди"}, {2,"XZ — сверху"}, {3,"YZ — сбоку"})
    )
    void OnPropertiesChanged() { Validate(); }
    virtual void Apply(Player& camera); // world transform; forward is local -Z
};

// Legacy scene record. New projects configure Character / PlayerCharacter properties.
// Character=0 means no possession; Camera=0 explicitly selects the follow camera.
class PlaySettings : public GameComponent
{
public:
    std::string DescribeEditorProperties() const override;
    ComponentId character = 0, camera = 0;
    bool standardControl = true;
    float cameraDistance = 5, cameraHeight = 2, lookSensitivity = 0.005f;
    bool IsRenderable() const override { return false; }
    bool CastsShadow() const override { return false; }
    std::string SaveProjectData() const override;
    void LoadProjectData(const std::string&) override;
    // @engine-internal-begin
private:
    void RemapProjectReferences(const std::map<ComponentId,ComponentId>&) override;
public:
    // @engine-internal-end
};

// Reads legacy settings once; modern property values always take precedence.
void MigrateLegacyPlaySettings(Scene& scene);
