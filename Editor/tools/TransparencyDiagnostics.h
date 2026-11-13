#pragma once

#include "PointShadowDiagnostics.h"

// Controlled layered surfaces rendered by the normal scene passes, including instance LOD/VAT.
namespace TransparencyDiagnostics {
using PointShadowDiagnostics::Check;
using PointShadowDiagnostics::Number;
using PointShadowDiagnostics::Vec;

inline MeshLodData Plane(float width, float height) {
    auto mesh=MeshPrimitives::Plane(width,height);
    for(auto& v:mesh.Vertices) {
        v.Position={v.Position.x,v.Position.z,0,1};
        v.Normal={0,0,-1,0}; v.Tangent={1,0,0,1};
    }
    return mesh;
}

inline MeshLodData CrossedPlanes(float width, float height) {
    MeshLodData result;
    for(int side=0;side<2;++side) {
        auto plane=Plane(width,height); const float angle=glm::radians(side?45.f:-45.f);
        const float c=std::cos(angle),s=std::sin(angle); const uint32_t offset=uint32_t(result.Vertices.size());
        for(auto v:plane.Vertices) {
            v.Position={c*v.Position.x,v.Position.y,-s*v.Position.x,1};
            v.Normal={-s,0,-c,0};v.Tangent={c,0,-s,1};v.TexCoord.x+=side*2.f;
            result.Vertices.push_back(v);
        }
        for(auto index:plane.Indices) result.Indices.push_back(index+offset);
    }
    return result;
}

inline std::shared_ptr<SkeletalModel> AnimatedPlane(const MeshLodData& plane) {
    auto model=PointShadowDiagnostics::MovingCube(.3f);
    model->Meshes.clear(); model->BoundingRadius=0; SkeletalMesh mesh;
    for(const auto& v:plane.Vertices) {
        SkinnedVertex vertex; vertex.Position={v.Position.x,v.Position.y,v.Position.z};
        vertex.Normal={v.Normal.x,v.Normal.y,v.Normal.z}; vertex.TexCoord={v.TexCoord.x,v.TexCoord.y}; vertex.BoneWeights[0]=1;
        model->BoundingRadius=std::max(model->BoundingRadius,glm::length(glm::vec3(v.Position.x,v.Position.y,v.Position.z))+.3f);
        mesh.Vertices.push_back(vertex);
    }
    mesh.Indices=plane.Indices; model->TotalIndexCount=int(mesh.Indices.size());
    model->Meshes.push_back(mesh); return model;
}

inline QJsonObject Run(const QJsonObject& in) {
    const int width=int(Number(in,"width",320)),height=int(Number(in,"height",256));
    Check(width>=100&&width<=1920&&height>=100&&height<=1080,"invalid viewport size");
    EnginePaths::SetContentRoot(std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString())/"EngineContent");
    HWND window=CreateWindowExW(0,L"STATIC",L"Transparency diagnostics",WS_OVERLAPPEDWINDOW,0,0,width,height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(window!=nullptr,"viewport creation failed");
    struct Guard {HWND window;~Guard(){DestroyWindow(window);}} guard{window};
    Game game; Check(game.InitializeViewport(window,width,height),"D3D viewport unavailable");
    game.SetSimulationEnabled(false); game.GetSettings().SetVSync(false); game.SetDeferredDebugOverlay(false);
    const int rendering=int(Number(in,"rendering",0)); Check(rendering==0||rendering==1,"rendering must be 0 or 1");
    game.GetSettings().SetRenderingType(static_cast<RenderType>(rendering)); game.SetShadowSettings(false,100);
    auto* camera=game.GetPlayer(); camera->SetCameraMode(CameraMode::Free);
    const bool ortho=in["orthographic"].toBool(false);
    camera->SetProjection(ortho,60,.1f,200,8); camera->StopRotate(false,false); camera->EnableWASD(false);
    struct Layer {GameComponent* component; InstancedMeshComponent* batch; uint32_t index; glm::vec3 position;};
    struct Group {InstancedMeshComponent* batch; float fade;};
    std::vector<Layer> layers; std::vector<Group> batches; int next=0;
    auto add=[&](std::unique_ptr<GameComponent> c) {
        auto* p=c.get(); Check(game.RegisterComponent("Layer"+std::to_string(next++),p)==RegisterResult::Ok,"registration failed");
        c.release(); return p;
    };
    const auto groups=in["groups"].toArray(); Check(!groups.empty()&&groups.size()<=16,"groups must contain 1..16 entries");
    for(auto item:groups) {
        Check(item.isObject(),"group must be object"); const auto g=item.toObject();
        const auto kind=g["kind"].toString("mesh");
        Check(kind=="mesh"||kind=="copies"||kind=="instances"||kind=="skeletal"||kind=="crowd","unknown group kind");
        const float w=Number(g,"width",4),h=Number(g,"height",4);
        Check(w>0&&h>0&&w<=50&&h<=50,"plane dimensions must be 0..50");
        const auto geometry=g["geometry"].toString("plane");Check(geometry=="plane"||geometry=="cross","geometry must be plane or cross");
        const auto plane=geometry=="cross"?CrossedPlanes(w,h):Plane(w,h); auto model=AnimatedPlane(plane);
        const auto path=g["material"].toString(); Check(QFileInfo(path).isAbsolute(),"absolute material path required");
        std::string error; auto material=game.LoadMaterial(path.toUtf8().constData(),&error);
        if(!material) throw std::runtime_error(error);
        Check(!material->HasErrors(),material->GetErrors().c_str());
        const auto instances=g["layers"].toArray(); Check(!instances.empty()&&instances.size()<=512,"layers must contain 1..512 entries");
        InstancedMeshComponent* batch=nullptr; MeshComponent* copies=nullptr;
        if(kind=="instances"||kind=="crowd") {
            auto c=std::make_unique<InstancedMeshComponent>();
            const int count=int(Number(g,"lods",1)); Check(count>=1&&count<=3,"lods must be 1..3");
            if(kind=="crowd") Check(c->SetAnimatedModel(game.GetDevice(),*model,30),"VAT bake failed");
            for(int level=0;level<count;++level) {
                if(kind=="crowd") {std::vector<CrowdSkinnedVertex> vertices;std::vector<uint32_t> indices;model->FlattenCrowd(vertices,indices);Check(c->SetSkinnedLod(level,vertices,indices),"VAT LOD failed");}
                else Check(c->SetLod(level,plane),"mesh LOD failed");
            }
            c->SetMaterial(material); c->SetColor({1,1,1,1}); c->SetAnimationLerp(true);
            c->SetLodDistances(g.contains("lodDistances")?PointShadowDiagnostics::Numbers(g["lodDistances"],3):std::vector<float>{1000,2000});
            if(g.contains("lodScreenHeights")) c->SetLodScreenHeights(PointShadowDiagnostics::Numbers(g["lodScreenHeights"],3));
            const float fade=Number(g,"fade",.25f);Check(fade>=0&&fade<=.5f,"fade must be 0..0.5");c->SetLodFadeFraction(fade);
            batch=static_cast<InstancedMeshComponent*>(add(std::move(c)));
            if(g["impostor"].toBool(false)) batch->SetImpostorEnabled(true);
            batches.push_back({batch,fade});
        } else if(kind=="copies") {
            auto c=std::make_unique<MeshComponent>();c->SetMesh(plane);c->SetMaterial(material);c->SetColor({1,1,1,1});
            copies=static_cast<MeshComponent*>(add(std::move(c)));
        }
        uint32_t index=0;
        for(auto value:instances) {
            const auto layer=value.toObject();const auto position=Vec(layer["position"],{});
            const auto rotation=Vec(layer["rotation"],{});
            const auto rgb=Vec(layer["color"],{1,0,0});const float alpha=Number(layer,"alpha",.5f);
            Check(alpha>=0&&alpha<=1,"alpha must be 0..1");const glm::vec4 color(rgb,alpha);
            GameComponent* component=nullptr;
            if(batch) {Check(rotation.x==0&&rotation.z==0,"instanced layers support yaw only");MeshInstance instance;instance.Position=position;instance.Yaw=glm::radians(rotation.y);instance.Color=color;batch->AddInstance(instance);}
            else if(copies) component=copies->CreateInstance(position,rotation,glm::vec3(1),color);
            else {
                std::unique_ptr<GameComponent> c;
                if(kind=="skeletal") {auto s=std::make_unique<SkeletalMeshComponent>();Check(s->SetModel(model),"skeletal model failed");c=std::move(s);}
                else {auto m=std::make_unique<MeshComponent>();m->SetMesh(plane);c=std::move(m);}
                c->SetPosition(position);c->SetRotation(rotation);c->SetColor(color);c->SetMaterial(material);component=add(std::move(c));
            }
            layers.push_back({component,batch,index++,position});
        }
    }
    const auto frames=in["frames"].toArray();Check(!frames.empty()&&frames.size()<=128,"frames must contain 1..128 entries");
    QJsonArray output;
    for(auto item:frames) {
        const auto f=item.toObject();const auto eye=Vec(f["camera"],{0,0,-8}),target=Vec(f["target"],{0,0,0});
        if(f.contains("width")||f.contains("height")) {
            const int w=int(Number(f,"width",width)),h=int(Number(f,"height",height));
            Check(w>=100&&w<=1920&&h>=100&&h<=1080,"invalid resized viewport");game.OnWindowResized(w,h,false);
        }
        Check(glm::length(target-eye)>.01f,"camera and target must differ");camera->SetPosition(eye);camera->SetViewDirection(glm::normalize(target-eye),{0,1,0});
        const float time=Number(f,"time",0);Check(time>=0&&time<=100,"invalid animation time");
        const auto positions=f["positions"].toArray();Check(positions.empty()||positions.size()==int(layers.size()),"positions must match layer count");
        for(size_t i=0;i<layers.size();++i) {
            auto& layer=layers[i];const auto position=positions.empty()?layer.position:Vec(positions[int(i)],layer.position);
            if(layer.batch) {layer.batch->SetInstancePosition(layer.index,position);layer.batch->SetInstanceAnimationPhase(layer.index,time);}
            else {layer.component->SetPosition(position);if(auto* s=dynamic_cast<SkeletalMeshComponent*>(layer.component)){s->SetAnimationTime(time);s->Tick(0);}}
        }
        if(f.contains("fadeWeight")) {
            const float weight=Number(f,"fadeWeight",0);Check(weight>=0&&weight<=1&&ortho,"fadeWeight requires orthographic view and 0..1");
            for(auto b:batches) {
                const float pixels=b.batch->GetLocalBoundingRadius()*height*camera->GetProjectionMatrix()._22;
                b.batch->SetLodScreenHeights({pixels/(1+b.fade-2*b.fade*weight)});
            }
        }
        Check(game.RenderViewportFrame(0),"frame failed");
        const auto image=f["image"].toString();Check(QFileInfo(image).isAbsolute()&&!QFileInfo::exists(image),"new absolute BMP path required");
        Check(game.DumpFrame(image.toUtf8().constData()),"frame capture failed");
        QJsonArray lod;
        for(auto b:batches) {const auto& d=b.batch->GetLodDiagnostics();QJsonArray counts;for(auto n:d.perLod)counts.append(int(n));lod.append(QJsonObject{{"levels",counts},{"impostor",d.impostorLevel}});}
        output.append(QJsonObject{{"image",image},{"lod",lod},{"drawCalls",int(game.GetFrameStats().DrawCalls)}});
    }
    return {{"adapter",QString::fromStdString(game.GetAdapterName())},{"frames",output}};
}
}
