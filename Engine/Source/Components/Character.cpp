#include <Engine/Components/Character.h>
#include "../../ThirdParty/nlohmann/json.hpp"
#include <cmath>
#include <stdexcept>
using Json=nlohmann::json;
Character::Character()
{
    // Keep the existing transform and SDK class layout; only the default mesh changes.
    // A capped unit cylinder along Y, with the same bounds as the former unit cube.
    std::vector<DirectX::XMFLOAT4> vertices;
    std::vector<int> triangles;
    constexpr int segments = 32;
    vertices.reserve(segments * 24);
    triangles.reserve(segments * 12);
    const auto triangle = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        const auto normal = glm::normalize(glm::cross(b - a, c - a));
        const float shade = 0.55f + 0.45f * std::abs(glm::dot(normal, glm::normalize(glm::vec3(1, 2, 3))));
        for (auto p : {a, b, c}) {
            triangles.push_back(static_cast<int>(vertices.size() / 2));
            vertices.emplace_back(p.x, p.y, p.z, 1.f);
            vertices.emplace_back(shade, shade, shade, 1.f);
        }
    };
    for (int i = 0; i < segments; ++i) {
        const float a = i * glm::two_pi<float>() / segments, b = (i + 1) * glm::two_pi<float>() / segments;
        const glm::vec3 p(0.5f * std::cos(a), -0.5f, 0.5f * std::sin(a));
        const glm::vec3 q(0.5f * std::cos(b), -0.5f, 0.5f * std::sin(b));
        const auto r = p + glm::vec3(0, 1, 0), s = q + glm::vec3(0, 1, 0);
        triangle(p, r, s); triangle(p, s, q);
        triangle({0, -0.5f, 0}, p, q); triangle({0, 0.5f, 0}, s, r);
    }
    InitPoints(vertices.data(), static_cast<int>(vertices.size()), triangles.data(), static_cast<int>(triangles.size()));
    SetScale({0.6f, 1.8f, 0.6f});
    SetColor({0.2f, 0.65f, 1, 1});
}
void Character::SetMoveSpeed(float v){if(!std::isfinite(v)||v<0||v>10000)throw std::invalid_argument("Invalid character speed");moveSpeed_=v;}
void Character::Move(const glm::vec3& direction,float seconds)
{
    if(!std::isfinite(seconds)||seconds<0)return;
    auto delta=direction;auto length=glm::length(delta);if(!std::isfinite(length)||length==0)return;
    if(length>1)delta/=length;
    delta*=moveSpeed_*seconds;
    // Convert world displacement to local displacement for parented characters.
    if(Parent){auto m=Parent->GetWorldMatrix();glm::mat4 world(1);for(int r=0;r<4;++r)for(int c=0;c<4;++c)world[r][c]=m.m[r][c];
        if(!bApplyParentScale)for(int i=0;i<3;++i)world[i]=glm::vec4(glm::normalize(glm::vec3(world[i])),0);
        delta=glm::vec3(glm::inverse(world)*glm::vec4(delta,0));}
    SetPosition(GetLocalPosition()+delta);
}
std::string Character::SaveProjectData() const {
    Json data{{"version",1}};EngineProperties::Visitor visitor{EngineProperties::Visitor::Save,data};Visit(*this,visitor);return data.dump();
}
void Character::LoadProjectData(const std::string& text) {
    if(text.empty())return;auto data=Json::parse(text);
    if(!data.is_object()||data.at("version")!=1)throw std::invalid_argument("Character schema");
    EngineProperties::Visitor visitor{EngineProperties::Visitor::Validate,data};Visit(*this,visitor);
    auto nextCamera=cameraSettings;visitor.mode=EngineProperties::Visitor::Load;CameraProperties::Visit(nextCamera,visitor);nextCamera.Validate();
    Visit(*this,visitor);needsLegacySettings_=!data.contains("playerControlled");
}
void Character::RemapProjectReferences(const std::map<ComponentId,ComponentId>& ids) {auto it=ids.find(camera);if(it!=ids.end())camera=it->second;}
void Character::ImportLegacySettings(const PlaySettings& settings) {
    if(!needsLegacySettings_)return;
    playerControlled=settings.character==GetComponentId();
    if(playerControlled){camera=settings.camera;standardControl=settings.standardControl;cameraDistance=settings.cameraDistance;cameraHeight=settings.cameraHeight;lookSensitivity=settings.lookSensitivity;}
    needsLegacySettings_=false;
}
void MigrateLegacyPlaySettings(Scene& scene) {
    const PlaySettings* settings=nullptr;
    for(const auto& entry:scene.GetComponents())if(auto* value=dynamic_cast<PlaySettings*>(entry.second.get())){
        if(settings)throw std::invalid_argument("Only one legacy PlaySettings object is allowed per scene");settings=value;
    }
    if(settings)for(const auto& entry:scene.GetComponents())if(auto* character=dynamic_cast<Character*>(entry.second.get()))character->ImportLegacySettings(*settings);
}
void CameraProperties::ApplyProjection(Player& camera) const {camera.SetProjection(orthographic,fov,nearClip,farClip,orthoHeight);}
glm::vec3 CameraProperties::Forward() const {return viewPlane==2?glm::vec3(0,-1,0):viewPlane==3?glm::vec3(-1,0,0):glm::vec3(0,0,-1);}
glm::vec3 CameraProperties::Up() const {return viewPlane==2?glm::vec3(0,0,-1):glm::vec3(0,1,0);}
void SceneCamera::Apply(Player& camera)
{
    auto forward=GetWorldRotationQuat()*glm::vec3(0,0,-1);
    if(viewPlane==1) forward={0,0,-1};
    if(viewPlane==2) forward={0,-1,0};
    if(viewPlane==3) forward={-1,0,0};
    camera.SetProjection(orthographic,fov,nearClip,farClip,orthoHeight);
    camera.SetCameraMode(CameraMode::Free);camera.SetPosition(GetWorldPosition());
    const auto up=viewPlane==2?glm::vec3(0,0,-1):viewPlane?glm::vec3(0,1,0):GetWorldRotationQuat()*glm::vec3(0,1,0);
    camera.SetViewDirection(forward,up);
}
std::string PlaySettings::SaveProjectData()const{return Json{{"version",1},{"character",character},{"camera",camera},{"standardControl",standardControl},{"cameraDistance",cameraDistance},{"cameraHeight",cameraHeight},{"lookSensitivity",lookSensitivity}}.dump();}
void PlaySettings::LoadProjectData(const std::string& text)
{
    if(text.empty())return;auto j=Json::parse(text);
    if(j.at("version")!=1||!j.at("character").is_number_unsigned()||!j.at("camera").is_number_unsigned())throw std::invalid_argument("Play settings schema");
    auto d=j.at("cameraDistance").get<float>(),h=j.at("cameraHeight").get<float>(),s=j.at("lookSensitivity").get<float>();
    if(!std::isfinite(d)||d<0||d>10000||!std::isfinite(h)||std::abs(h)>10000||!std::isfinite(s)||s<0||s>1)throw std::invalid_argument("Invalid camera settings");
    auto control=j.at("standardControl").get<bool>();auto ch=j.at("character").get<ComponentId>(),cam=j.at("camera").get<ComponentId>();
    character=ch;camera=cam;standardControl=control;cameraDistance=d;cameraHeight=h;lookSensitivity=s;
}
void PlaySettings::RemapProjectReferences(const std::map<ComponentId,ComponentId>& ids)
{auto remap=[&](ComponentId& id){auto it=ids.find(id);if(it!=ids.end())id=it->second;};remap(character);remap(camera);}

std::string Character::DescribeEditorProperties() const {
    Json data{{"version",1},{"fields",Json::array()}};
    EngineProperties::Visitor visitor{EngineProperties::Visitor::Schema,data};
    Visit(*this,visitor);
    for(auto& field:data["fields"]) {
        const auto path=field["path"].get<std::string>();
        field["group"]=path=="/moveSpeed"||path=="/playerControlled"||path=="/standardControl"?"Игрок":"Камера игрока";
        if(path=="/camera"){field["type"]="object";field["objectKind"]=2;}
    }
    return data.dump();
}
std::string PlaySettings::DescribeEditorProperties() const {
    Json data{{"version",1},{"fields",Json::array()}};
    EngineProperties::Visitor visitor{EngineProperties::Visitor::Schema,data};
    visitor.Field("standardControl",standardControl,"Стандартное управление");
    visitor.Field("cameraDistance",cameraDistance,"Дистанция камеры",0,10000);
    visitor.Field("cameraHeight",cameraHeight,"Высота камеры",-10000,10000);
    visitor.Field("lookSensitivity",lookSensitivity,"Чувствительность камеры",0,1);
    data["fields"].push_back({{"path","/character"},{"label","Персонаж"},{"type","object"}});
    data["fields"].push_back({{"path","/camera"},{"label","Камера"},{"type","object"}});
    return data.dump();
}
