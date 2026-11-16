#pragma once

#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/PointShadow.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Render/Material.h>
#include <QCryptographicHash>
#include <cmath>
#include <cstring>

// A parameterized, real-renderer commandlet. No game DLL or interactive editor is required.
namespace PointShadowDiagnostics {
inline void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
inline float Number(const QJsonObject& in, const char* key, float fallback) {
    if (!in.contains(key)) return fallback;
    Check(in[key].isDouble() && std::isfinite(float(in[key].toDouble())), "finite numeric field required");
    return float(in[key].toDouble());
}
inline int Integer(const QJsonObject& in, const char* key, int fallback, int minimum, int maximum) {
    Check(minimum<=maximum && fallback>=minimum && fallback<=maximum,"invalid integer bounds or fallback");
    if (!in.contains(key)) return fallback;
    Check(in[key].isDouble(),"integer field must be a JSON number");
    const double value=in[key].toDouble();
    Check(std::isfinite(value) && value==std::trunc(value) &&
          value>=static_cast<double>(minimum) && value<=static_cast<double>(maximum),
          "integer field must be whole, finite, and within supported range");
    return static_cast<int>(value);
}
inline glm::vec3 Vec(const QJsonValue& value, glm::vec3 fallback) {
    if (value.isUndefined()) return fallback;
    const auto a=value.toArray(); Check(a.size()==3, "expected a three-element vector");
    for (auto v:a) Check(v.isDouble() && std::isfinite(float(v.toDouble())), "finite vector required");
    return {float(a[0].toDouble()),float(a[1].toDouble()),float(a[2].toDouble())};
}
inline QJsonArray Array(glm::vec3 v) { return {v.x,v.y,v.z}; }
inline std::vector<float> Numbers(const QJsonValue& value, size_t limit) {
    Check(value.isArray(),"numeric array required"); std::vector<float> values;
    for(auto v:value.toArray()) {
        Check(v.isDouble()&&std::isfinite(float(v.toDouble()))&&v.toDouble()>0,"positive finite array values required");
        values.push_back(float(v.toDouble()));
    }
    Check(values.size()<=limit,"too many array entries"); return values;
}
inline void ApplyLight(PointLightComponent& light,const QJsonObject& state) {
    light.SetPosition(Vec(state["position"],light.GetWorldPosition()));
    light.SetLightColor(Vec(state["color"],light.GetLightColor()));
    const float radius=Number(state,"radius",light.GetRadius()),intensity=Number(state,"intensity",light.GetIntensity());
    Check(radius>0&&radius<=200&&intensity>=0&&intensity<=20,"invalid light radius/intensity");
    light.SetRadius(radius); light.SetIntensity(intensity);
    if(state.contains("enabled")) light.SetEnabled(state["enabled"].toBool());
    if(state.contains("shadows")) light.SetCastShadows(state["shadows"].toBool());
}

inline std::shared_ptr<SkeletalModel> MovingCube(float travel) {
    using namespace DirectX;
    auto model=std::make_shared<SkeletalModel>();
    Bone root; root.Name="Root";
    XMStoreFloat4x4(&root.LocalTransform,XMMatrixIdentity());
    XMStoreFloat4x4(&root.InverseBindPose,XMMatrixIdentity());
    model->ModelSkeleton.Bones.push_back(root);
    AnimationClip clip; clip.Name="Move"; clip.Duration=1;
    NodeAnimationChannel channel; channel.NodeName="Root"; channel.BoneIndex=0;
    channel.PositionKeys={{0,{0,0,0}},{0.5f,{travel,0,0}},{1,{0,0,0}}};
    clip.Channels.push_back(channel); model->Clips.push_back(clip);
    const auto cube=MeshPrimitives::Cube(); SkeletalMesh mesh;
    for(const auto& source:cube.Vertices) {
        SkinnedVertex vertex; vertex.Position={source.Position.x,source.Position.y,source.Position.z};
        vertex.Normal={source.Normal.x,source.Normal.y,source.Normal.z};
        vertex.TexCoord={source.TexCoord.x,source.TexCoord.y}; vertex.BoneWeights[0]=1;
        mesh.Vertices.push_back(vertex);
    }
    mesh.Indices=cube.Indices; model->TotalIndexCount=int(mesh.Indices.size());
    model->BoundingRadius=std::sqrt(3.0f)*0.5f; model->Meshes.push_back(std::move(mesh));
    return model;
}

inline QJsonObject ReadDepth(Game& game, const QJsonArray& probes) {
    using Microsoft::WRL::ComPtr;
    QJsonObject output{{"count",game.GetPointShadowCount()},{"cubes",QJsonArray{}}};
    if (!game.GetPointShadowCount()) return output;
    ComPtr<ID3D11Resource> resource; game.GetPointShadowSRV()->GetResource(&resource);
    ComPtr<ID3D11Texture2D> texture; Check(SUCCEEDED(resource.As(&texture)),"point-shadow texture unavailable");
    D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.MiscFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    Check(SUCCEEDED(game.GetDevice()->CreateTexture2D(&desc,nullptr,&staging)),"point-shadow staging failed");
    game.GetContext()->CopyResource(staging.Get(),texture.Get());
    QJsonArray cubes;
    for(int slot=0;slot<game.GetPointShadowCount();++slot) {
    QJsonObject result{{"slot",slot}};
    std::vector<std::vector<float>> maps(PointShadowFaces);
    QJsonArray faces;
    for(int face=0;face<PointShadowFaces;++face) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const int slice=slot*PointShadowFaces+face;
        Check(SUCCEEDED(game.GetContext()->Map(staging.Get(),slice,D3D11_MAP_READ,0,&mapped)),"point-shadow readback failed");
        QCryptographicHash hash(QCryptographicHash::Sha256); int occupied=0;
        maps[face].resize(size_t(desc.Width)*desc.Height);
        for(UINT row=0;row<desc.Height;++row) {
            const auto* data=static_cast<const char*>(mapped.pData)+row*mapped.RowPitch;
            std::memcpy(maps[face].data()+size_t(row)*desc.Width,data,desc.Width*sizeof(float));
            hash.addData(QByteArrayView(data,desc.Width*sizeof(float)));
        }
        game.GetContext()->Unmap(staging.Get(),slice);
        for(float depth:maps[face]) occupied+=depth<1;
        faces.append(QJsonObject{{"face",face},{"occupied",occupied},{"sha256",QString::fromLatin1(hash.result().toHex())}});
    }
    result["faces"]=faces;
    const auto sphere=game.GetPointShadowSpheres()[slot];
    result["light"]=QJsonArray{sphere.x,sphere.y,sphere.z,sphere.w};
    QJsonArray samples;
    for(auto entry:probes) {
        const auto world=Vec(entry,{}); const auto offset=world-glm::vec3(sphere.x,sphere.y,sphere.z);
        const auto a=glm::abs(offset); int face;
        if(a.x>=a.y && a.x>=a.z) face=offset.x>=0?0:1;
        else if(a.y>=a.z) face=offset.y>=0?2:3;
        else face=offset.z>=0?4:5;
        using namespace DirectX;
        auto matrix=PointShadowViewProjection(face,{sphere.x,sphere.y,sphere.z},sphere.w);
        auto projected=XMVector3TransformCoord(XMVectorSet(world.x,world.y,world.z,1),XMMatrixTranspose(XMLoadFloat4x4(&matrix)));
        const float u=XMVectorGetX(projected)*.5f+.5f, v=-XMVectorGetY(projected)*.5f+.5f;
        const int x=std::clamp(int(u*desc.Width),0,int(desc.Width)-1),y=std::clamp(int(v*desc.Height),0,int(desc.Height)-1);
        const float stored=maps[face][size_t(y)*desc.Width+x], expected=XMVectorGetZ(projected);
        samples.append(QJsonObject{{"position",Array(world)},{"face",face},{"stored",stored},{"depth",expected},
            {"occluded",expected>stored+.002f}});
    }
    result["probes"]=samples; cubes.append(result);
    // Retain the single-light result fields for existing commandlet clients.
    if(slot==0) for(auto it=result.begin();it!=result.end();++it) output[it.key()]=it.value();
    }
    output["cubes"]=cubes; return output;
}

inline QJsonObject Run(const QJsonObject& in) {
    const int width=Integer(in,"width",640,100,1920),height=Integer(in,"height",480,100,1080);
    const auto runtime=std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
    EnginePaths::SetContentRoot(runtime/"EngineContent");
    HWND window=CreateWindowExW(0,L"STATIC",L"Point shadow diagnostics",WS_OVERLAPPEDWINDOW,0,0,width,height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(window!=nullptr,"viewport creation failed");
    struct WindowGuard { HWND h; ~WindowGuard(){DestroyWindow(h);} } guard{window};
    Game game; Check(game.InitializeViewport(window,width,height),"D3D viewport unavailable");
    game.SetSimulationEnabled(false); game.GetSettings().SetVSync(false);
    game.SetDeferredDebugOverlay(false);
    const int rendering=Integer(in,"rendering",0,0,1);
    game.GetSettings().SetRenderingType(static_cast<RenderType>(rendering));
    // Explicit scene lighting with no sun reproduces a point-lit indoor/night scene.
    game.SetSceneLightingEnabled(in["sceneLighting"].toBool(false));
    game.SetDirectionalLightIntensity(0);
    game.SetShadowSettings(in["globalShadows"].toBool(true),100);
    auto* camera=game.GetPlayer(); camera->SetCameraMode(CameraMode::Free);
    const auto eye=Vec(in["camera"],{10,12,-16}),target=Vec(in["target"],{2,0,0});
    Check(glm::length(target-eye)>.01f,"camera and target must differ");
    camera->SetPosition(eye); camera->SetViewDirection(glm::normalize(target-eye),{0,1,0});
    camera->SetProjection(false,60,.1f,200,24); camera->StopRotate(false,false); camera->EnableWASD(false);
    auto add=[&](const char* name,std::unique_ptr<GameComponent> component) {
        auto* pointer=component.get(); Check(game.RegisterComponent(name,pointer)==RegisterResult::Ok,"component registration failed");
        component.release(); return pointer;
    };
    auto floor=std::make_unique<MeshComponent>(); floor->SetMesh(MeshPrimitives::Cube());
    floor->SetPosition({2,-.15f,0}); floor->SetScale({24,.3f,16}); floor->SetColor({.65f,.65f,.65f,1});
    add("Receiver",std::move(floor));
    const float radius=Number(in,"radius",30),intensity=Number(in,"intensity",2);
    Check(radius>1.5f&&radius<=200&&intensity>=0&&intensity<=20,"invalid light radius/intensity");
    std::vector<PointLightComponent*> lamps;
    const bool multi=in.contains("lights");
    const auto lights=multi?in["lights"].toArray():QJsonArray{QJsonObject{}};
    Check(!lights.empty()&&lights.size()<=4,"lights must contain 1..4 states");
    for(auto state:lights) {
        Check(state.isObject(),"light must be object");
        auto light=std::make_unique<PointLightComponent>(glm::vec3(0,4,0),glm::vec3(1,.85f,.65f),intensity,radius);
        light->SetCastShadows(true); ApplyLight(*light,state.toObject()); lamps.push_back(light.get());
        const auto name="Lamp"+std::to_string(lamps.size()); add(name.c_str(),std::move(light));
    }
    const auto kind=in["caster"].toString("mesh");
    Check(kind=="mesh"||kind=="skeletal"||kind=="crowd","caster must be mesh, skeletal or crowd");
    std::shared_ptr<SkeletalModel> model;
    if(kind!="mesh") {
        const auto path=in["model"].toString();
        if(path.isEmpty()) model=MovingCube(Number(in,"travel",6));
        else {
            QFile file(path); Check(QFileInfo(path).isAbsolute()&&file.open(QIODevice::ReadOnly),"absolute readable model path required");
            Check(file.size()<=64*1024*1024,"model exceeds 64 MB");
            const auto bytes=file.readAll(); const auto hint=QFileInfo(path).suffix().toUtf8();
            auto loaded=LoadSkeletalModelFromMemory(bytes.data(),size_t(bytes.size()),hint.constData());
            if(!loaded.Success) throw std::runtime_error(loaded.ErrorMessage);
            model=loaded.Model;
        }
        Check(model&&!model->Clips.empty(),"animated model must have a clip");
    }
    const float scale=Number(in,"scale",1); Check(scale>0&&scale<=20,"scale must be 0..20");
    SkeletalMeshComponent* skeletal=nullptr; InstancedMeshComponent* crowd=nullptr; GameComponent* caster=nullptr;
    if(kind=="mesh") { auto c=std::make_unique<MeshComponent>(); c->SetMesh(MeshPrimitives::Cube()); caster=add("Caster",std::move(c)); }
    if(kind=="skeletal") { auto c=std::make_unique<SkeletalMeshComponent>(); Check(c->SetModel(model),"SetModel failed"); skeletal=c.get(); caster=add("Caster",std::move(c)); }
    if(kind=="crowd") {
        auto c=std::make_unique<InstancedMeshComponent>();
        if(in["manualTexture"].toBool(false)) {
            auto bake=BakeSkeletalModel(*model,30); Check(bake.Success,"animation baking failed");
            std::vector<CrowdSkinnedVertex> vertices; std::vector<uint32_t> indices;
            model->FlattenCrowd(vertices,indices); Check(c->SetSkinnedLod(0,vertices,indices),"skinned LOD failed");
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
            Check(CreateBakedTextureResource(game.GetDevice(),bake.Texture,texture,srv),"VAT creation failed");
            c->SetAnimationTexture(srv,bake.Texture.Clips);
        } else Check(c->SetAnimatedModel(game.GetDevice(),*model,30),"animation baking failed");
        if(in.contains("replaceLodOffset")) {
            const auto offset=Vec(in["replaceLodOffset"],{});
            std::vector<CrowdSkinnedVertex> vertices; std::vector<uint32_t> indices;
            model->FlattenCrowd(vertices,indices);
            for(auto& vertex:vertices) {
                vertex.Position.x+=offset.x; vertex.Position.y+=offset.y; vertex.Position.z+=offset.z;
            }
            Check(c->SetSkinnedLod(0,vertices,indices),"replacement LOD failed");
        }
        if(in.contains("lodScales")) {
            const auto scales=Numbers(in["lodScales"],InstancedMeshComponent::MaxLodCount);
            std::vector<CrowdSkinnedVertex> vertices; std::vector<uint32_t> indices; model->FlattenCrowd(vertices,indices);
            for(size_t level=0;level<scales.size();++level) {
                auto scaled=vertices;
                for(auto& vertex:scaled) {vertex.Position.x*=scales[level];vertex.Position.y*=scales[level];vertex.Position.z*=scales[level];}
                Check(c->SetSkinnedLod(int(level),scaled,indices),"diagnostic LOD failed");
            }
        }
        c->SetLodDistances(in.contains("lodDistances")?Numbers(in["lodDistances"],3):std::vector<float>{1000,2000});
        if(in.contains("lodScreenHeights")) c->SetLodScreenHeights(Numbers(in["lodScreenHeights"],3));
        const float fade=Number(in,"lodFade",0); Check(fade>=0&&fade<=.5f,"lodFade must be 0..0.5");
        c->SetLodFadeFraction(fade); c->SetAnimationLerp(true);
        MeshInstance instance; instance.Position={0,1,0}; instance.Scale=scale; instance.Color={.3f,.55f,.85f,1}; c->AddInstance(instance);
        crowd=c.get(); caster=add("Caster",std::move(c));
    }
    caster->SetColor({.3f,.55f,.85f,1});
    if(in["material"].toBool(false)) Check(caster->SetMaterial(game.GetDefaultMaterial()),"default material assignment failed");
    std::shared_ptr<Material> assignedMaterial;
    if(in.contains("materialAsset")) {
        const auto path=in["materialAsset"].toString();
        Check(QFileInfo(path).isAbsolute(),"materialAsset must be an absolute .mat path");
        std::string error; assignedMaterial=game.LoadMaterial(path.toUtf8().constData(),&error);
        if(!assignedMaterial) throw std::runtime_error(error);
        Check(!assignedMaterial->HasErrors(),assignedMaterial->GetErrors().c_str());
        Check(caster->SetMaterial(assignedMaterial),"material asset assignment failed");
    }
    caster->SetScale({scale,scale,scale});
    auto frames=in["frames"].toArray(); Check(!frames.empty()&&frames.size()<=64,"frames must contain 1..64 states");
    const auto probes=in["probes"].toArray(); Check(probes.size()<=64,"at most 64 probes");
    QJsonArray output;
    for(auto entry:frames) {
        Check(entry.isObject(),"frame must be object"); const auto frame=entry.toObject();
        if(frame.contains("materialParams")) {
            Check(assignedMaterial!=nullptr&&frame["materialParams"].isObject(),"materialParams requires materialAsset and an object");
            const auto params=frame["materialParams"].toObject();
            for(auto it=params.begin();it!=params.end();++it) {
                const auto values=it.value().isArray()?it.value().toArray():QJsonArray{it.value()};
                Check(!values.empty()&&values.size()<=4,"material parameter must have 1..4 components");
                std::array<float,4> value{};
                for(int i=0;i<values.size();++i) {
                    Check(values[i].isDouble()&&std::isfinite(float(values[i].toDouble())),"material parameter must be finite numeric");
                    value[i]=float(values[i].toDouble());
                }
                Check(assignedMaterial->SetParameter(it.key().toStdString(),value),"unknown or unsupported material parameter");
            }
        }
        const auto position=Vec(frame["position"],{0,1,0});
        if(!multi) {lamps[0]->SetPosition(Vec(frame["light"],{0,4,0})); lamps[0]->SetCastShadows(frame["shadows"].toBool(true));}
        if(frame.contains("lights")) {
            const auto states=frame["lights"].toArray(); Check(states.size()==int(lamps.size()),"frame light count must match scene");
            for(int i=0;i<states.size();++i) {Check(states[i].isObject(),"light must be object");ApplyLight(*lamps[i],states[i].toObject());}
        }
        if(frame.contains("camera")||frame.contains("target")) {
            const auto frameEye=Vec(frame["camera"],eye),frameTarget=Vec(frame["target"],target);
            Check(glm::length(frameTarget-frameEye)>.01f,"camera and target must differ");
            camera->SetPosition(frameEye); camera->SetViewDirection(glm::normalize(frameTarget-frameEye),{0,1,0});
        }
        const float time=Number(frame,"time",0); Check(time>=0&&time<=3600,"time must be 0..3600");
        if(crowd) { crowd->SetInstancePosition(0,position); crowd->SetInstanceAnimationPhase(0,time/model->Clips[0].Duration); }
        else caster->SetPosition(position);
        if(skeletal) { skeletal->SetAnimationTime(time); skeletal->Tick(0); }
        const auto image=frame["image"].toString();
        if(!image.isEmpty()) {
            Check(QFileInfo(image).isAbsolute()&&!QFileInfo::exists(image)&&QFileInfo(image).dir().exists(),"image must be a new absolute path with existing parent");
            game.RequestFrameDump(image.toUtf8().toStdString());
        }
        Check(game.RenderViewportFrame(0),"render failed");
        if(!image.isEmpty()) Check(QFileInfo(image).size()>54,"frame capture missing");
        const auto frameProbes=frame.contains("probes")?frame["probes"].toArray():probes;
        Check(frameProbes.size()<=64,"at most 64 probes");
        QJsonObject result=ReadDepth(game,frameProbes); result["time"]=time; result["position"]=Array(position);
        result["directionalShadows"]=game.AreShadowsActive();
        if(assignedMaterial) result["material"]=QJsonObject{{"errors",QString::fromStdString(assignedMaterial->GetErrors())},
            {"gbuffer",assignedMaterial->WritesGBuffer()},{"castsShadow",caster->CastsShadow()}};
        const auto& constants=game.GetFrameConstants(); QJsonArray bindings;
        for(int i=0;i<int(lamps.size());++i) {
            const auto p=constants.LightPositions[i],params=constants.LightParams[i];
            bindings.append(QJsonObject{{"index",i},{"position",QJsonArray{p.x,p.y,p.z}},
                {"radius",params.y},{"intensity",params.x},{"enabled",params.z!=0},{"slot",int(params.w)-1}});
        }
        result["lights"]=bindings;
        if(crowd) {
            const auto& d=crowd->GetLodDiagnostics(); QJsonArray levels;
            for(auto count:d.perLod) levels.append(int(count));
            result["lod"]=QJsonObject{{"perLod",levels},{"drawn",int(d.drawn)},
                {"distance",d.minDistance},{"screenSize",d.screenSize},{"fade",d.fadeFraction}};
        }
        result["image"]=image; output.append(result);
    }
    return {{"adapter",QString::fromStdString(game.GetAdapterName())},{"caster",kind},{"frames",output}};
}
}
