#define NOMINMAX
#include <Windows.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Render/MaterialAsset.h>
#include <Engine/Scene/SceneAssets.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/SaveGame/SaveGame.h>
#include "../../Source/Components/ComponentInternal.h"
#include "../../ThirdParty/nlohmann/json.hpp"
#include <fstream>
#include <iostream>

void Check(bool, const char*);
namespace fs = std::filesystem;
namespace {
class AuthoredSkeletalModel : public SkeletalMeshComponent {};
class PixelGame : public Game {
public:
    std::pair<size_t,uint64_t> ShadowDepth(ID3D11ShaderResourceView* view = nullptr, int onlySlice = -1) {
        if(!view) view=GetShadowMapSRV();
        Check(view!=nullptr,"shadow map exists");
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;view->GetResource(&resource);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> source;resource.As(&source);
        D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> copy;
        Check(SUCCEEDED(Device->CreateTexture2D(&desc,nullptr,&copy)),"shadow readback texture");
        Context->CopyResource(copy.Get(),source.Get());
        size_t count=0;uint64_t hash=14695981039346656037ull;
        for(unsigned slice=0;slice<desc.ArraySize;++slice){
            if(onlySlice>=0 && slice!=unsigned(onlySlice)) continue;
            D3D11_MAPPED_SUBRESOURCE map{};
            Check(SUCCEEDED(Context->Map(copy.Get(),slice,D3D11_MAP_READ,0,&map)),"shadow readback map");
            for(unsigned y=0;y<desc.Height;++y){
                const auto* row=reinterpret_cast<const float*>(static_cast<const unsigned char*>(map.pData)+y*map.RowPitch);
                for(unsigned x=0;x<desc.Width;++x){
                    if(row[x]<1)++count;
                    uint32_t bits;memcpy(&bits,row+x,sizeof(bits));hash=(hash^bits)*1099511628211ull;
                }
            }
            Context->Unmap(copy.Get(),slice);
        }
        return {count,hash};
    }
    std::vector<unsigned char> Pixels() {
        D3D11_TEXTURE2D_DESC desc{}; BackTexture->GetDesc(&desc);
        desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0;
        desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ; desc.MiscFlags=0;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        Check(SUCCEEDED(Device->CreateTexture2D(&desc,nullptr,&staging)),"model pixel readback texture");
        Context->CopyResource(staging.Get(),BackTexture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(SUCCEEDED(Context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"model pixel readback map");
        std::vector<unsigned char> pixels(desc.Width*desc.Height*4);
        for(unsigned y=0;y<desc.Height;++y)
            memcpy(pixels.data()+y*desc.Width*4,static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch,desc.Width*4);
        Context->Unmap(staging.Get(),0); return pixels;
    }
};
void Frame(PixelGame& game,const fs::path& path) {
    Check(game.RenderViewportFrame(0),"imported model frame");
    Check(game.DumpFrame(path.u8string()),"imported model screenshot");
}
bool Corners(const std::vector<unsigned char>& pixels) {
    // Orthographic height 4: plane fills x=200..440, y=120..360.
    const int points[][3]={{260,180,0},{380,180,1},{260,300,2}};
    bool ok=true;
    for(const auto& p:points){
        const auto* c=pixels.data()+(p[1]*640+p[0])*4;
        std::cout<<"UV corner "<<p[0]<<","<<p[1]<<" rgb="<<int(c[0])<<","<<int(c[1])<<","<<int(c[2])<<'\n';
        ok &= c[p[2]]>180 && c[(p[2]+1)%3]<50 && c[(p[2]+2)%3]<50;
    }
    const auto* yellow=pixels.data()+(300*640+380)*4;
    std::cout<<"UV corner 380,300 rgb="<<int(yellow[0])<<","<<int(yellow[1])<<","<<int(yellow[2])<<'\n';
    ok &= yellow[0]>180 && yellow[1]>180 && yellow[2]<50;
    return ok;
}
size_t DarkerGround(const std::vector<unsigned char>& without,const std::vector<unsigned char>& with) {
    size_t count=0;
    for(size_t i=0;i<with.size();i+=4)
        if(abs(int(without[i])-without[i+1])<3 && abs(int(without[i])-without[i+2])<3 &&
           without[i]>80 && int(without[i])-with[i]>20) ++count;
    return count;
}
}
void RunImportedModelRenderTests(HWND hwnd,const fs::path& content) {
#if defined(ENGINE_ENABLE_ASSIMP)
    if(!fs::exists(content/"Models/blend-suzanne/blend-suzanne.glb")) {
        std::cout<<"SKIP: imported Blender GPU regression (generate PAINENGINE_BLEND_FIXTURES with make-blend-fixtures.py)\n";
        return;
    }
    PixelGame game;
    game.GetSettings().SetRenderingType(Custom); game.GetSettings().SetVSync(false);
    game.SetSimulationEnabled(false);
    Check(game.InitializeViewport(hwnd,640,480,true),"Blender model viewport");
    auto* camera=game.GetPlayer(); camera->SetCameraMode(CameraMode::Free);
    camera->SetPosition({0,0,5});camera->SetViewDirection({0,0,-1},{0,1,0});camera->SetProjection(true,60,.1f,100,4);
    const auto output=content.parent_path();
    auto model=SceneAssets::CreateModel("/Game/Models/blend-uv/blend-uv.glb");
    model->SetRotation({0,180,0});
    const auto materialPath=content/fs::u8path(model->GetMaterialAsset().substr(6));
    auto data=nlohmann::json::parse(std::ifstream(materialPath));
    data["render"]["lighting"]="unlit";
    data["render"]["twoSided"]=true;
    std::ofstream(materialPath)<<data.dump(2);
    MaterialAsset material;std::string error;
    Check(MaterialAsset::Load(materialPath.u8string(),material,error),"UV chart material");
    const auto texture=MaterialAsset::ResolvePath(materialPath.u8string(),material.Textures.front().second);
    auto added=game.GetScene().AddComponent("UV chart",model,SceneAssets::ModelType);
    Frame(game,output/"uv-material.bmp");
    const bool materialUV=Corners(game.Pixels());
    // Same shader/lighting, image moved from BaseMap to the component texture slot.
    std::ofstream(content/"Materials/UVComponent.hlsl")<<"void Surface(MaterialInput i, inout MaterialOutput o) {}\n";
    std::ofstream(content/"Materials/UVComponent.mat")<<R"({"shader":"UVComponent.hlsl","render":{"lighting":"unlit","twoSided":true}})";
    auto* component=game.GetScene().FindComponent(added.id);
    component->SetMaterialAsset("/Game/Materials/UVComponent.mat");
    Check(component->TrySetTexture(texture),"assign same UV image to component");
    Frame(game,output/"uv-component.bmp");
    const bool componentUV=Corners(game.Pixels());
    game.GetScene().RemoveComponent(added.id);

    camera->SetPosition({4,3,6});camera->SetViewDirection({-4,-2,-6},{0,1,0});camera->SetProjection(false,55,.1f,100,4);
    game.SetDirectionalLight({-.6f,-1,.4f},{1,1,1},1);
    auto ground=std::make_unique<MeshComponent>(); ground->SetMesh(MeshPrimitives::Cube());
    ground->SetColor({1,1,1,1});ground->SetScale({8,.1f,8});ground->SetPosition({0,-1.3f,0});
    std::ofstream(content/"Materials/Ground.mat")<<R"({"shader":"UVComponent.hlsl","render":{"castShadows":false}})";
    ground->SetMaterialAsset("/Game/Materials/Ground.mat");
    std::unique_ptr<GameComponent> object=std::move(ground);
    game.GetScene().AddComponent("Ground",object);
    model=SceneAssets::CreateModel("/Game/Models/blend-suzanne/blend-suzanne.glb");
    model->SetRotation({0,180,0});
    added=game.GetScene().AddComponent("Suzanne",model,SceneAssets::ModelType);
    game.SetShadowSettings(false,30);Frame(game,output/"suzanne-no-shadows.bmp");
    const auto without=game.Pixels();
    game.SetShadowSettings(true,30);Frame(game,output/"suzanne-shadows.bmp");
    const auto with=game.Pixels();const auto darkerGround=DarkerGround(without,with);
    std::cout<<"ImportedModelProbe materialUV="<<materialUV<<" componentUV="<<componentUV<<" darkerGround="<<darkerGround<<'\n';
    const auto opaqueDepth=game.ShadowDepth();
    Check(opaqueDepth.first>100,"imported model writes cascade depth");
    for(auto mode:{Forward,Deffered}){
        game.GetSettings().SetRenderingType(mode);
        game.SetShadowSettings(false,30);Check(game.RenderViewportFrame(0),"render-mode unshadowed reference");
        const auto reference=game.Pixels();
        game.SetShadowSettings(true,30);
        Frame(game,output/(mode==Forward?"suzanne-forward.bmp":"suzanne-deferred.bmp"));
        Check(DarkerGround(reference,game.Pixels())>50 && game.ShadowDepth()==opaqueDepth,
              "imported model casts a visible consistent shadow in Forward and Deferred");
    }
    game.GetSettings().SetRenderingType(Custom);
    auto* suzanne=dynamic_cast<SkeletalMeshComponent*>(game.GetScene().FindComponent(added.id));
    suzanne->SetMaterialAsset("");
    Frame(game,output/"suzanne-default-material.bmp");
    Check(game.ShadowDepth()==opaqueDepth,"default material preserves opaque skinned shadow silhouette");
    std::ofstream(content/"Materials/Masked.hlsl")<<"void Surface(MaterialInput i,inout MaterialOutput o){o.Alpha=step(0.5,i.UV.x);}\n";
    std::ofstream(content/"Materials/Masked.mat")<<R"({"shader":"Masked.hlsl","render":{"blend":"masked"}})";
    suzanne->SetMaterialAsset("/Game/Materials/Masked.mat");
    Frame(game,output/"suzanne-masked.bmp");
    const auto maskedDepth=game.ShadowDepth();
    Check(maskedDepth.first>0 && maskedDepth.first<opaqueDepth.first,"masked skinned shadow clips its UV silhouette");
    std::ofstream(content/"Materials/NoShadow.mat")<<R"({"shader":"UVComponent.hlsl","render":{"castShadows":false}})";
    suzanne->SetMaterialAsset("/Game/Materials/NoShadow.mat");
    Check(game.RenderViewportFrame(0),"render shadow disabled material");
    Check(game.ShadowDepth().first==0,"castShadows false removes imported model depth immediately");
    suzanne->SetMaterialAsset("");

    // Give the actual imported geometry a one-bone translation clip. The depth
    // pass runs before colour, so this catches a stale palette/one-frame lag.
    const auto originalModel=suzanne->GetModel();
    auto rig=std::make_shared<SkeletalModel>(*originalModel);
    Bone bone;bone.Name="Root";
    DirectX::XMStoreFloat4x4(&bone.InverseBindPose,DirectX::XMMatrixIdentity());
    DirectX::XMStoreFloat4x4(&bone.LocalTransform,DirectX::XMMatrixIdentity());
    rig->ModelSkeleton.Bones={bone};
    for(auto& mesh:rig->Meshes)for(auto& vertex:mesh.Vertices){
        for(int i=0;i<4;++i){vertex.BoneIndices[i]=0;vertex.BoneWeights[i]=i==0?1.f:0.f;}
    }
    AnimationClip clip;clip.Name="Translate";clip.Duration=2;
    NodeAnimationChannel channel;channel.NodeName="Root";channel.BoneIndex=0;
    channel.PositionKeys={{0,{0,0,0}},{1,{1.5f,0,0}},{2,{0,0,0}}};clip.Channels={channel};rig->Clips={clip};
    Check(suzanne->SetModel(rig),"animate imported Blender geometry");
    EngineInternal::ComponentAccess::StaticShadow(*suzanne,true);
    Check(!suzanne->IsStaticShadowCaster(),"animated models cannot freeze in the static shadow cache");
    Check(game.RenderViewportFrame(0),"animated shadow initial pose");const auto pose0=game.ShadowDepth();
    suzanne->SetAnimationTime(1);suzanne->Tick(0);
    Frame(game,output/"suzanne-animated.bmp");const auto pose1=game.ShadowDepth();
    Check(pose0.first>0 && pose1.first>0 && pose0.second!=pose1.second,"shadow follows the current animated bone palette");
    suzanne->SetAnimationTime(0);suzanne->Tick(0);
    Check(game.RenderViewportFrame(0) && game.ShadowDepth()==pose0,"returning animation time restores the same shadow in one frame");

    Check(suzanne->SetModel(originalModel) && suzanne->IsStaticShadowCaster(),"static imported geometry can use the shadow cache");
    game.SetStaticShadowArea({0,0,0},8);
    Frame(game,output/"suzanne-static-shadow.bmp");
    const auto staticDepth=game.ShadowDepth(game.GetStaticShadowSRV());
    size_t nearStaticPixels=0,farStaticPixels=0;
    for(int cascade=0;cascade<MaxShadowCascades;++cascade) {
        const auto count=game.ShadowDepth(nullptr,cascade).first;
        (cascade<StaticCasterCascades?nearStaticPixels:farStaticPixels)+=count;
    }
    std::cout<<"StaticImportedProbe baked="<<staticDepth.first<<" near="<<nearStaticPixels<<" far="<<farStaticPixels<<'\n';
    Check(staticDepth.first>100 && nearStaticPixels>100 && farStaticPixels==0,
          "static imported caster uses precise near cascades and baked far shadow");
    Check(game.RenderViewportFrame(0) && game.ShadowDepth(game.GetStaticShadowSRV())==staticDepth,
          "unchanged imported caster keeps its baked shadow");
    suzanne->SetModelYawDegrees(75);
    Check(game.RenderViewportFrame(0),"static model yaw changed");
    Check(game.ShadowDepth(game.GetStaticShadowSRV()).second!=staticDepth.second,"model yaw invalidates the static shadow");

    game.GetScene().RemoveComponent(added.id);
    model=SceneAssets::CreateModel("/Game/Models/blend-suzanne-multiple/blend-suzanne-multiple.glb");
    model->SetRotation({0,180,0});
    auto* multiple=dynamic_cast<SkeletalMeshComponent*>(model.get());
    Check(multiple && multiple->GetMaterialSlots().size()==2 && multiple->GetMaterialAsset().empty(),"multiple materials automatically assigned to distinct slots");
    const auto slots=multiple->GetMaterialSlots();
    const auto savedData=multiple->SaveProjectData();
    Check(!multiple->SetMaterialSlot(99999,"") && !multiple->SetMaterialSlot(slots.begin()->first,"/Game/../outside.mat"),"unknown slot and escaping path rejected");
    Check(multiple->SaveProjectData()==savedData,"invalid slot assignment is atomic");
    added=game.GetScene().AddComponent("Suzanne multiple materials",model,SceneAssets::ModelType);
    Frame(game,output/"suzanne-multiple-materials.bmp");
    Check(game.GetFrameStats().VisibleObjects==2,"submesh parts do not inflate visible scene object count");
    auto magenta=[](const std::vector<unsigned char>& pixels){size_t n=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]>80 && pixels[i+2]>80 && pixels[i+1]<40)++n;return n;};
    const auto multicolored=game.Pixels();
    Check(magenta(multicolored)>100,"second Blender material appears on its own submesh");
    const auto originalDepth=game.ShadowDepth();
    // A slot can have its own blend/shadow state without moving the opaque slot
    // into the transparent pass or dropping its G-buffer contribution.
    std::ofstream(content/"Materials/SlotTransparent.hlsl")<<"void Surface(MaterialInput i,inout MaterialOutput o){o.Albedo=float3(1,0,1);o.Alpha=0.5;}\n";
    std::ofstream(content/"Materials/SlotTransparent.mat")<<R"({"shader":"SlotTransparent.hlsl","render":{"blend":"translucent","lighting":"unlit","twoSided":true}})";
    Check(multiple->SetMaterialSlot(slots.rbegin()->first,"/Game/Materials/SlotTransparent.mat"),"transparent slot assignment");
    for(auto mode:{Forward,Deffered}) {
        game.GetSettings().SetRenderingType(mode);
        Frame(game,output/(mode==Forward?"slots-mixed-forward.bmp":"slots-mixed-deferred.bmp"));
        Check(game.ShadowDepth().first<originalDepth.first && magenta(game.Pixels())>20,"mixed opaque/transparent slots render in their own pass and shadow state");
    }
    game.GetSettings().SetRenderingType(Custom);
    multiple->LoadProjectData(savedData);
    Check(multiple->SetMaterialSlot(slots.rbegin()->first,"/Game/Materials/Green.mat"),"replace only second material slot");
    Frame(game,output/"suzanne-slot-replaced.bmp");
    Check(magenta(game.Pixels())==0,"slot replacement changes second material pixels");
    multiple->LoadProjectData(savedData);
    Check(game.RenderViewportFrame(0) && game.Pixels()==multicolored,"project data restores exact per-slot pixels");
    Check(multiple->SetMaterialSlot(slots.rbegin()->first,"/Game/Materials/LateSlot.mat"),"missing slot material keeps path");
    Check(game.RenderViewportFrame(0),"draw slot error checker");
    std::ofstream(content/"Materials/LateSlot.mat")<<R"({"shader":"Green.hlsl","render":{"lighting":"unlit"}})";
    game.InvalidateAllShaders();
    Check(game.RenderViewportFrame(0) && magenta(game.Pixels())==0,"reloading repaired slot replaces error checker");
    multiple->LoadProjectData(savedData);
    // Restore one model in a fresh scene: no import sidecar is needed for assignments.
    SceneSnapshot snapshot; snapshot.name="Material slots";snapshot.documentVersion=3;
    SceneComponentSnapshot item;item.id=1;item.name="Multiple";item.typeName=SceneAssets::ModelType;
    item.modelAsset=multiple->GetModelAsset();item.projectData=savedData;
    snapshot.components.push_back(item);
    Scene restored;
    SceneDocument::Restore(snapshot,{},restored);
    auto* reopened=dynamic_cast<SkeletalMeshComponent*>(restored.FindComponentByName("Multiple"));
    Check(reopened && reopened->GetMaterialSlots()==slots,"scene restore retains all material slot paths");
    // Capture only this built-in model; its slots are carried by the scene inside SaveGame.
    Game savedGame;savedGame.Saves().Configure("MaterialSlotsProbe",output/"Saves");
    std::unique_ptr<GameComponent> savedModel=SceneAssets::CreateModel(item.modelAsset,false);
    savedModel->LoadProjectData(savedData);savedGame.GetScene().AddComponent("Saved model",savedModel,SceneAssets::ModelType);
    const auto save=savedGame.Saves().Capture();
    savedGame.Saves().Restore(save);
    auto* loaded=dynamic_cast<SkeletalMeshComponent*>(savedGame.GetScene().FindComponentByName("Saved model"));
    Check(loaded && loaded->GetMaterialSlots()==slots,"SaveGame retains every material slot");
    // A project subclass is default-constructed by its catalog, unlike Engine.Model.
    const ComponentType derivedType{"Test.AuthoredSkeletal", "Authored skeletal", []()->std::unique_ptr<GameComponent>{return std::make_unique<AuthoredSkeletalModel>();}};
    const ComponentTypeCatalog derivedCatalog{&derivedType,1};
    snapshot.components.front().typeName=derivedType.typeName;
    Scene derivedScene;
    SceneDocument::Restore(snapshot,derivedCatalog,derivedScene);
    auto* derived=dynamic_cast<AuthoredSkeletalModel*>(derivedScene.FindComponentByName("Multiple"));
    Check(derived && derived->GetModel() && derived->GetMaterialSlots()==slots,"derived model restores geometry before its material-slot codec");
    Game derivedGame;derivedGame.Saves().SetCatalog(derivedCatalog);derivedGame.Saves().Configure("DerivedSlots",output/"DerivedSaves");
    SceneDocument::Restore(snapshot,derivedCatalog,derivedGame.GetScene());
    const auto derivedSave=derivedGame.Saves().Capture();derivedGame.Saves().Restore(derivedSave);
    auto* derivedLoaded=dynamic_cast<AuthoredSkeletalModel*>(derivedGame.GetScene().FindComponentByName("Multiple"));
    Check(derivedLoaded && derivedLoaded->GetModel() && derivedLoaded->GetMaterialSlots()==slots,"SaveGame restores derived model geometry and all slots");
    {
        const auto sceneFile=output/"DerivedSlots.scene.json";
        SceneDocument::Write(sceneFile,snapshot);
        SceneEditSession editor(derivedCatalog);const auto api=editor.GetApi();
        char error[2048]{};uint64_t sceneId=0,sourceId=0,copyId=0;
        Check(api.setContentRoot(api.context,content.u8string().c_str(),error,sizeof(error))==EngineSceneOk,"derived clone content root");
        Check(api.loadScene(api.context,sceneFile.u8string().c_str(),&sceneId,error,sizeof(error))==EngineSceneOk,"editor loads derived material slots");
        EngineSceneInfo info{};
        Check(api.scene.readScene(api.scene.context,&info,[](void* p,const EngineSceneObjectInfo* o){*static_cast<uint64_t*>(p)=o->id;},&sourceId)==EngineSceneOk,"find derived object to duplicate");
        const auto batch=editor.GetBatchApi();
        Check(batch.apply(batch.context,EngineBatchDuplicate,&sourceId,1,sourceId,nullptr,&copyId,error,sizeof(error))==EngineSceneOk && copyId!=sourceId,"batch duplicate restores derived slot codec before reference remapping");
        const auto data=editor.GetProjectDataApi();std::string copiedData;
        Check(data.read(data.context,copyId,[](void* p,const char* value){*static_cast<std::string*>(p)=value;},&copiedData)==EngineSceneOk && copiedData==savedData,"duplicated derived model retains material assignments");
    }
    Check(materialUV,"Blender automatic material keeps all UV corners upright");
    Check(componentUV,"Blender component texture keeps the same UV orientation as its material");
    Check(darkerGround>50,"imported Blender model casts a visible shadow on the receiving ground");
#else
    (void)hwnd;(void)content;
#endif
}
