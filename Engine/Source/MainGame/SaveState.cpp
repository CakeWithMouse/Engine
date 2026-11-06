#include <Engine/SaveGame/SaveGame.h>
#include <Engine/MainGame/CharacterController.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Components/SpecificComponents/FBXComponent.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/UI/UIMarkup.h>

namespace {
void MeshState(SaveArchive& a,MeshLodData& mesh) {
    std::vector<std::array<float,12>> vertices;
    for(const auto& v:mesh.Vertices) vertices.push_back({v.Position.x,v.Position.y,v.Position.z,v.Position.w,
        v.Normal.x,v.Normal.y,v.Normal.z,v.Normal.w,v.TexCoord.x,v.TexCoord.y,v.TexCoord.z,v.TexCoord.w});
    auto indices=mesh.Indices;a.Field("vertices",vertices);a.Field("indices",indices);
    if(vertices.size()>1000000 || indices.size()>3000000 || indices.size()%3)throw std::runtime_error("Invalid saved mesh size");
    for(auto index:indices)if(index>=vertices.size())throw std::runtime_error("Invalid saved mesh index");
    if(a.IsReading()){mesh.Vertices.clear();for(const auto& v:vertices)mesh.Vertices.push_back({{v[0],v[1],v[2],v[3]},{v[4],v[5],v[6],v[7]},{v[8],v[9],v[10],v[11]}});mesh.Indices=std::move(indices);
        // The save format keeps twelve floats per vertex; the tangents are derived data.
        GenerateTangents(mesh);}
}
}

void GameComponent::SerializeSaveGame(SaveArchive& archive)
{
    archive.Section("GameComponent",[&](SaveArchive& a) {
        a.Version(1);
        a.Field("color",Color); a.Field("collision",bHasCollision); a.Field("opacity",bHasOpacity);
        a.Field("skybox",bIsSkybox); a.Field("staticShadow",bStaticShadowCaster);
        a.Field("velocitySpeed",VelocitySpeed); a.Field("rotationSpeed",RotationSpeed);
        a.Field("reflection",ReflectionStrength); a.Field("fresnel",FresnelPower);
        a.Field("uvScale",uvScale); a.Field("uvOffset",uvOffset); a.Field("traceChannels",traceChannels);
        a.Field("vertexShader",VertexShaderName);a.Field("pixelShader",PixelShaderName);
        bool primitive=VisualPrimitive;a.Field("visualPrimitive",primitive);
        if(primitive){
            if(a.IsReading())VisualComponent=std::make_unique<MeshComponent>();
            if(VisualComponent)a.Section("visual",[&](SaveArchive& v){VisualComponent->SerializeSaveGame(v);});
            if(a.IsReading())VisualPrimitive=true;
        }
        else if(a.IsReading()&&VisualPrimitive){VisualPrimitive=false;VisualComponent.reset();}
        unsigned count=static_cast<unsigned>(InstanceArray.size()); a.Field("instanceCount",count);
        if(count>100000) throw std::runtime_error("Too many component instances");
        if(a.IsReading()) InstanceArray.clear();
        for(unsigned i=0;i<count;++i) {
            if(a.IsReading()) CreateInstance({}, {}, glm::vec3(1),glm::vec4(1));
            if(i>=InstanceArray.size()) continue;
            auto& instance=*InstanceArray[i];
            a.Section(std::to_string(i).c_str(),[&](SaveArchive& item) {
                auto position=instance.GetLocalPosition(), rotation=instance.GetRotation(), scale=instance.GetScale();
                item.Field("position",position); item.Field("rotation",rotation); item.Field("scale",scale);
                item.Field("color",instance.Color);
                if(item.IsReading()){instance.SetPosition(position);instance.SetRotation(rotation);instance.SetScale(scale);}
            });
        }
    });
}
void Game::SerializeSaveGame(SaveArchive& archive)
{
    archive.Section("Game",[&](SaveArchive& a) {
        a.Version(1); a.Field("sceneTime",TotalTime); a.Field("realTime",RealTime);
        a.Field("timeScale",SimulationTimeScale); a.Field("simulationEnabled",bSimulationEnabled);
        a.Field("sceneLighting",bSceneLighting); a.Field("ambient",AmbientIntensity); a.Field("shininess",SpecularShininess);
        bool vsync=Settings.IsVSyncEnabled(); unsigned aa=Settings.GetAntialiasing(); int renderer=Settings.GetRenderingType();
        a.Field("vsync",vsync); a.Field("antialiasing",aa); a.Field("renderer",renderer);
        if(renderer<0 || renderer>2 || SimulationTimeScale<0) throw std::runtime_error("Invalid saved game settings");
        if(a.IsReading()){Settings.SetVSync(vsync);Settings.SetAntialiasing(aa);Settings.SetRenderingType(static_cast<RenderType>(renderer));}
        if(FirstPlayer) a.Section("Player",[&](SaveArchive& p){FirstPlayer->SerializeSaveGame(p);});
        if(ProjectHudComponent)a.Section("HUD",[&](SaveArchive& p){ProjectHudComponent->SerializeSaveGame(p);});
    });
}
void Player::SerializeSaveGame(SaveArchive& a)
{
    a.Version(1); a.Field("position",position); a.Field("yaw",yaw); a.Field("pitch",pitch);
    a.Field("orthographic",orthographic); a.Field("fov",fovDegrees); a.Field("orthoHeight",orthographicHeight);
    a.Field("near",nearPlane); a.Field("far",farPlane); a.Field("orbitTarget",orbitTarget);
    a.Field("orbitRadius",orbitRadius); a.Field("orbitYaw",orbitYaw); a.Field("orbitPitch",orbitPitch);
    a.Field("orbitMin",minimalOrbitRadius); a.Field("orbitMax",maximalOrbitRadius);
    int mode=static_cast<int>(cameraMode); a.Field("mode",mode);
    if(mode<0 || mode>2 || nearPlane<=0 || farPlane<=nearPlane || fovDegrees<=0 || fovDegrees>=180 || orthographicHeight<=0)
        throw std::runtime_error("Invalid saved camera");
    a.Field("wasd",bCanWASD); a.Field("switchMode",bCanSwitchMode);
    a.Field("stopYaw",bStopRotateYaw);a.Field("stopPitch",bStopRotatePitch);
    a.Field("moveSpeed",moveSpeed); a.Field("lookSpeed",lookSpeed); a.Field("zoomSpeed",zoomSpeed);
    a.Field("sensitivity",mouseSensitivity); a.Field("orbitSensitivity",mouseSensitivityOrbital);
    if(a.IsReading()){cameraMode=static_cast<CameraMode>(mode);RefreshMatrices();}
}
void CharacterController::SerializeSaveGame(SaveArchive& a)
{
    a.Section("CharacterController",[&](SaveArchive& s){
        s.Version(1); s.Reference("character",character_); s.Reference("camera",camera_);
        s.Field("yaw",yaw_);s.Field("pitch",pitch_);s.Field("distance",distance_);s.Field("height",height_);
        s.Field("sensitivity",sensitivity_);s.Field("enabled",enabled_);
    });
    if(a.IsReading()){
        // Restore local input gating without resetting the live game's input while
        // a candidate controller is still being prepared transactionally.
        inputScope_.SetEnabled(enabled_);right_=forward_=0;
    }
}
void GameSubsystem::SerializeSaveGame(SaveArchive& a)
{ a.Section("GameSubsystem",[&](SaveArchive& s){s.Version(1);s.Field("tickEnabled",bTickEnabled);}); }
void GameSession::SerializeSaveGame(SaveArchive& a)
{
    CheckThread(); if(callbacks_) throw std::runtime_error("Cannot save subsystem during a callback");
    for(auto& entry:entries_) a.Section(entry.key.c_str(),[&](SaveArchive& s){entry.object->SerializeSaveGame(s);});
}
void GameSession::NotifySaveGameLoaded(Scene& scene)
{
    CheckThread();if(!IsRunning()||callbacks_)throw std::runtime_error("Session cannot receive load notification");
    ++callbacks_;
    try{for(auto& entry:entries_){entry.object->OnSaveGameLoaded(scene);if(!IsRunning())break;}}
    catch(...){EndCallback();throw;}
    EndCallback();
    if(!IsRunning())throw std::runtime_error("Session stopped during load notification");
}
void SkeletalMeshComponent::SerializeSaveGame(SaveArchive& a)
{
    GameComponent::SerializeSaveGame(a);
    a.Section("SkeletalMeshComponent",[&](SaveArchive& s){
        s.Version(1);s.Field("clip",currentClipIndex);s.Field("time",animationTime);s.Field("speed",playbackSpeed);
        s.Field("playing",isPlaying);s.Field("yaw",modelYawDegrees);
        if(skeletalModel && !skeletalModel->Clips.empty() && currentClipIndex>=skeletalModel->Clips.size()) throw std::runtime_error("Invalid saved animation clip");
    });
}
void ParticleSystemComponent::SerializeSaveGame(SaveArchive& a)
{
    GameComponent::SerializeSaveGame(a);
    a.Section("ParticleSystemComponent",[&](SaveArchive& s){
        s.Version(1);s.Field("count",ParticleCount);s.Field("rate",SimulationRate);s.Field("time",SimulationTime);
        s.Field("maxStep",MaxSimulationStep);s.Field("lifetime",BaseLifetime);s.Field("spread",SpreadRadius);
        s.Field("speed",BaseSpeed);s.Field("maxDistance",MaxDistance);s.Field("velocity",BaseVelocity);
        s.Field("acceleration",Acceleration);s.Field("speedRandomness",SpeedRandomness);s.Field("size",BaseSize);
        s.Field("brightness",Brightness);s.Field("sizeRandomness",SizeRandomness);s.Field("minLife",MinLifeFraction);
        s.Field("depthCollision",bDepthCollisionEnabled);s.Field("sorting",bSortingEnabled);
        s.Field("collisionBias",DepthCollisionBias);s.Field("bounce",DepthCollisionBounce);s.Field("friction",DepthCollisionFriction);
        if(ParticleCount>1000000 || BaseLifetime<=0)throw std::runtime_error("Invalid saved emitter");
        // GPU particle buffers are rebuilt; individual transient particles are not gameplay entities.
    });
}
void InstancedMeshComponent::SerializeSaveGame(SaveArchive& a)
{
    GameComponent::SerializeSaveGame(a);
    a.Section("InstancedMeshComponent",[&](SaveArchive& s){
        s.Version(1);s.Field("lodDistances",LodDistances);s.Field("lodHeights",LodScreenHeights);
        s.Field("drawDistance",DrawDistance);s.Field("shadowDistance",ShadowDistance);
        s.Field("shadowFade",ShadowDistanceFade);s.Field("shadowLodBias",ShadowLodBias);
        s.Field("shadowCascadeLimit",ShadowCascadeLimit);s.Field("shadowExtrusion",ShadowCasterExtrusion);
        s.Field("lodFade",LodFadeFraction);s.Field("animationLerp",bAnimationLerp);
        s.Field("lodCount",LodCount);
        if(LodCount<0||LodCount>MaxLodCount)throw std::runtime_error("Invalid saved LOD count");
        for(int i=0;i<LodCount;++i)s.Section(("lod"+std::to_string(i)).c_str(),[&](SaveArchive& m){MeshState(m,Lods[i].Data);
            if(m.IsReading() && !Lods[i].Data.Vertices.empty()){auto mesh=Lods[i].Data;if(!SetLod(i,std::move(mesh)))throw std::runtime_error("Cannot restore saved LOD");}});
        unsigned count=static_cast<unsigned>(Instances.size()); s.Field("count",count);
        if(count>100000)throw std::runtime_error("Too many saved mesh instances");
        if(s.IsReading()) Instances.resize(count);
        for(unsigned i=0;i<count && i<Instances.size();++i)s.Section(std::to_string(i).c_str(),[&](SaveArchive& item){
            auto& v=Instances[i];item.Field("position",v.Position);item.Field("yaw",v.Yaw);item.Field("scale",v.Scale);
            item.Field("color",v.Color);item.Field("custom",v.CustomData);item.Field("visible",v.bVisible);
            item.Field("clip",v.ClipId);item.Field("phase",v.AnimationPhase);item.Field("speed",v.Speed);
        });
        if(s.IsReading()){PreparedFrame=UINT64_MAX;bStaticShadowDataDirty=true;}
    });
}

void MeshComponent::SerializeSaveGame(SaveArchive& a){
    GameComponent::SerializeSaveGame(a);
    a.Section("MeshComponent",[&](SaveArchive& m){m.Version(1);MeshState(m,Mesh);});
    if(a.IsReading()){auto mesh=Mesh;SetMesh(std::move(mesh));}
}
#if defined(ENGINE_ENABLE_ASSIMP)
// FBXComponent.cpp is only compiled with Assimp, so LoadModel and modelPath exist only then.
// Without the guard this definition is the one reference that keeps the whole engine from
// linking when the optional dependency is absent.
void FBXComponent::SerializeSaveGame(SaveArchive& a){
    GameComponent::SerializeSaveGame(a);
    a.Section("FBXComponent",[&](SaveArchive& s){s.Version(1);auto path=modelPath;s.Field("model",path);
        if(s.IsReading()&&!path.empty()&&!LoadModel(path))throw std::runtime_error("Cannot restore imported model: "+path);});
}
#endif
void UIComponent::SerializeSaveGame(SaveArchive& a){
    GameComponent::SerializeSaveGame(a);
    a.Section("UIComponent",[&](SaveArchive& s){
        s.Version(1);s.Field("visible",bVisible);
        std::string markup=UIMarkup::Serialize(*Root);s.Field("markup",markup);
        if(s.IsReading()){
            std::unique_ptr<UIElement> root;std::string error;
            if(!UIMarkup::Parse(markup,"SaveGame UI",root,error))throw std::runtime_error(error);
            Router.Reset();Root=std::move(root);ApplyBindings();++MarkupRevision;
        }
    });
}
