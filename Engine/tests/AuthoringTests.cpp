#include "SceneTestAccess.h"
#include "TestSkipped.h"
#define NOMINMAX
#include <Windows.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Components/EditorProperties.h>
#include <Engine/Components/PropertyMacros.h>
#include <Engine/Components/Character.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/SkeletalLoader.h>
#include <Engine/Scene/SceneAssets.h>
#include <Engine/Scene/SceneDocument.h>
#include "../ThirdParty/nlohmann/json.hpp"
#include <fstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace fs=std::filesystem;
#define CHECK(v) do {if(!(v))throw std::runtime_error(#v);}while(false)
namespace {
struct MacroSettings : GameComponent {
    float speed=3; bool enabled=true; std::string title="hello";
    ENGINE_PROPERTIES(GameComponent,
        ENGINE_PROPERTY_FIELD(speed,"Speed",0,20)
        ENGINE_PROPERTY_FIELD(enabled,"Enabled")
        ENGINE_PROPERTY_FIELD(title,"Title")
    )
};
struct DerivedSettings : MacroSettings {
    int count=2;
    ENGINE_PROPERTIES(MacroSettings, ENGINE_PROPERTY_FIELD(count,"Count",0,10))
};
struct Settings : GameComponent, EditorPropertyProvider {
    Json data={{"version",1},{"speed",1},{"enabled",true},{"mode","walk"},{"target",0},{"asset",""},{"readonly",1}};
    std::string SaveProjectData() const override{return data.dump();}
    void LoadProjectData(const std::string& text) override{if(!text.empty()){auto next=Json::parse(text);if(next.at("speed")==13)throw std::runtime_error("rejected by component");data=next;}}
    std::string DescribeEditorProperties() const override{return R"({"version":1,"fields":[
        {"path":"/speed","type":"number","min":0,"max":20},
        {"path":"/enabled","type":"bool"},{"path":"/mode","type":"enum","options":[{"label":"Walk","value":"walk"},{"label":"Run","value":"run"}]},
        {"path":"/target","type":"object","objectType":"Test.Settings"},
        {"path":"/asset","type":"asset","extensions":[".bmp"]},{"path":"/readonly","type":"integer","readOnly":true}]})";}
};
std::unique_ptr<GameComponent> Make(){return std::make_unique<Settings>();}
void Write(const fs::path& p,const std::string& text){std::ofstream f(p,std::ios::binary);f<<text;CHECK(bool(f));}
std::string Read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
struct Row{uint64_t id;std::string name,model,material;float x;};
std::vector<Row> Rows(const EngineEditSessionApi& api){std::vector<Row> rows;EngineSceneInfo info{};CHECK(api.readActiveScene(api.context,&info,[](void* p,const EngineSceneObjectInfo* o){static_cast<std::vector<Row>*>(p)->push_back({o->id,o->name,o->modelAsset,o->materialAsset,o->transform.position[0]});},&rows)==EngineSceneOk);return rows;}
}
void AuthoringTests(){
    GameComponent plain; CHECK(plain.DescribeEditorProperties().empty());
    Settings legacy; GameComponent& legacyBase=legacy;
    CHECK(legacyBase.DescribeEditorProperties()==legacy.DescribeEditorProperties());
    CHECK(dynamic_cast<EditorPropertyProvider*>(&legacyBase)->DescribeEditorProperties()==legacy.DescribeEditorProperties());
    DerivedSettings macro; CHECK(Json::parse(macro.DescribeEditorProperties())["fields"].size()==4);
    macro.LoadProjectData(R"({"version":1,"speed":7,"enabled":false,"title":"saved","count":4})");
    CHECK(macro.speed==7 && !macro.enabled && macro.title=="saved" && macro.count==4);
    const auto original=macro.SaveProjectData();
    try { macro.LoadProjectData(R"({"version":1,"speed":8,"count":99})");CHECK(false); } catch(const std::invalid_argument&) {}
    CHECK(macro.SaveProjectData()==original);
    macro.LoadProjectData(R"({"version":1,"speed":2})"); CHECK(macro.count==4);
    // Modern components use only GameComponent, including editor dispatch and Play restore.
    const ComponentType modernType{"Test.Modern","Modern",[]()->std::unique_ptr<GameComponent>{return std::make_unique<DerivedSettings>();}};
    SceneEditSession modernSession({&modernType,1});
    auto modernApi=modernSession.GetApi();auto modernAuthor=modernSession.GetAuthoringApi();
    EngineSceneTransform modernTransform{{0,0,0},{0,0,0,1},{1,1,1}};uint64_t modernId=0;char modernError[1024]{};
    CHECK(modernApi.scene.createObject(modernApi.scene.context,modernType.typeName,"Modern",&modernTransform,&modernId)==EngineSceneOk);
    std::string modernSchema;
    CHECK(modernAuthor.describe(modernAuthor.context,modernId,[](void* p,const char* text){*static_cast<std::string*>(p)=text;},&modernSchema)==EngineSceneOk);
    CHECK(Json::parse(modernSchema)["fields"].size()==4);
    CHECK(modernAuthor.setProperty(modernAuthor.context,modernId,"/speed","8",modernError,sizeof(modernError))==EngineSceneOk);
    CHECK(modernApi.control(modernApi.context,EnginePlay)==EngineSceneOk);
    CHECK(modernApi.control(modernApi.context,EngineStop)==EngineSceneOk);
    auto modernData=modernSession.GetProjectDataApi();std::string modernSaved;
    CHECK(modernData.read(modernData.context,modernId,[](void* p,const char* text){*static_cast<std::string*>(p)=text;},&modernSaved)==EngineSceneOk);
    CHECK(Json::parse(modernSaved)["speed"]==8);
    // Components without a property provider expose {}, not a reflected field
    // list. Editing one must fail through the normal contract without mutation.
    const ComponentType plainType{"Test.Plain","Plain",[]()->std::unique_ptr<GameComponent>{return std::make_unique<GameComponent>();}};
    SceneEditSession plainSession({&plainType,1});
    auto plainApi=plainSession.GetApi();auto plainAuthor=plainSession.GetAuthoringApi();
    uint64_t plainId=0;
    CHECK(plainApi.scene.createObject(plainApi.scene.context,plainType.typeName,"Plain",&modernTransform,&plainId)==EngineSceneOk);
    std::string plainSchema;
    CHECK(plainAuthor.describe(plainAuthor.context,plainId,[](void* p,const char* text){*static_cast<std::string*>(p)=text;},&plainSchema)==EngineSceneOk);
    CHECK(Json::parse(plainSchema)==Json::object());
    CHECK(plainAuthor.setProperty(plainAuthor.context,plainId,"/transform/position","[99,99,99]",modernError,sizeof(modernError))==EngineSceneFailure);
    CHECK(std::string(modernError)=="Property is not editable");
    CHECK(Rows(plainApi).size()==1 && Rows(plainApi).front().x==0);
    SceneCamera camera; camera.orthographic=true;camera.viewPlane=2;camera.fov=80;
    SceneCamera copy;copy.LoadProjectData(camera.SaveProjectData());CHECK(copy.orthographic && copy.viewPlane==2 && copy.fov==80);
    const auto cameraBefore=copy.SaveProjectData();auto invalidCamera=Json::parse(cameraBefore);invalidCamera["farClip"]=0.05;
    try {copy.LoadProjectData(invalidCamera.dump());CHECK(false);}catch(const std::invalid_argument&){}
    CHECK(copy.SaveProjectData()==cameraBefore);
    Player player;copy.Apply(player);CHECK(player.GetForward()==glm::vec3(0,-1,0));
    Character hero; CHECK(hero.SetMaterialAsset("/Game/test.mat"));
    CHECK(hero.GetRenderComponent()!=&hero && hero.GetRenderComponent()->SupportsMaterials());
    hero.SetMaterialAsset("");
    CHECK(hero.GetRenderComponent()->GetMaterialAsset().empty() && hero.GetRenderComponent()->GetIndexCount()>0);

    const ComponentType type{"Test.Settings","Settings",Make};SceneEditSession session({&type,1});
    auto api=session.GetApi();auto author=session.GetAuthoringApi();auto data=session.GetProjectDataApi();char error[2048]{};
    EngineSceneTransform t{{3,0,0},{0,0,0,1},{1,1,1}};uint64_t id=0,target=0;
    CHECK(api.scene.createObject(api.scene.context,type.typeName,"Settings",&t,&id)==EngineSceneOk);
    CHECK(api.scene.createObject(api.scene.context,type.typeName,"Target",&t,&target)==EngineSceneOk);
    std::string schema;CHECK(author.describe(author.context,id,[](void* p,const char* s){*static_cast<std::string*>(p)=s;},&schema)==EngineSceneOk);
    CHECK(Json::parse(schema).at("fields").size()==6);
    auto set=[&](const char* key,const std::string& value){return author.setProperty(author.context,id,key,value.c_str(),error,sizeof(error));};
    auto read=[&](){std::string value;CHECK(data.read(data.context,id,[](void* p,const char* s){*static_cast<std::string*>(p)=s;},&value)==EngineSceneOk);return Json::parse(value);};
    CHECK(set("/speed","5.5")==EngineSceneOk);CHECK(read()["speed"]==5.5);
    for(const auto* bad:{"-1","21","null","\"fast\"","13"}){CHECK(set("/speed",bad)!=EngineSceneOk);CHECK(read()["speed"]==5.5);}
    CHECK(set("/enabled","false")==EngineSceneOk);CHECK(set("/enabled","0")!=EngineSceneOk);
    CHECK(set("/mode","\"run\"")==EngineSceneOk);CHECK(set("/mode","\"fly\"")!=EngineSceneOk);
    CHECK(set("/target",std::to_string(target))==EngineSceneOk);CHECK(set("/target","999999")!=EngineSceneOk);
    CHECK(set("/readonly","2")!=EngineSceneOk);CHECK(set("/version","2")!=EngineSceneOk);CHECK(set("/unknown","2")!=EngineSceneOk);
    CHECK(api.control(api.context,EnginePlay)==EngineSceneOk);CHECK(set("/speed","2")==EngineSceneReadOnly);CHECK(api.control(api.context,EngineStop)==EngineSceneOk);

    const auto root=fs::absolute(fs::path("Engine/build/authoring-tests")/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())));
    const auto content=root/"Content";fs::create_directories(content/"Scenes");fs::create_directories(content/"Materials");
    CHECK(api.setContentRoot(api.context,content.u8string().c_str(),error,sizeof(error))==EngineSceneOk);
    std::string bmp(58,'\0');bmp[0]='B';bmp[1]='M';bmp[2]=58;bmp[10]=54;bmp[14]=40;bmp[18]=1;bmp[22]=1;bmp[26]=1;bmp[28]=24;bmp[34]=4;bmp[54]=20;
    Write(root/"pixel.bmp",bmp);const auto texture=SceneAssets::Import(content,root/"pixel.bmp",SceneAssets::Texture);
    CHECK(set("/asset",Json(texture).dump())==EngineSceneOk);CHECK(set("/asset","\"/Game/Missing.bmp\"")!=EngineSceneOk);CHECK(set("/asset","\"/Game/../pixel.bmp\"")!=EngineSceneOk);
    Write(root/"mesh.obj","v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
#ifndef ENGINE_ENABLE_ASSIMP
    bool unavailable=false;try{const auto probe=SceneAssets::Import(content,root/"mesh.obj",SceneAssets::Model);hero.SetModelAsset(probe);}catch(const std::runtime_error& e){unavailable=std::string(e.what()).find("Assimp not enabled")!=std::string::npos;}
    CHECK(unavailable);
    throw TestSkipped("typed properties/texture checks passed; model transaction checks require -EnableAssimp");
#endif
    const auto model=SceneAssets::Import(content,root/"mesh.obj",SceneAssets::Model);const auto modelFile=content/fs::u8path(model.substr(6));
    hero.SetMoveSpeed(9);
    hero.SetModelAsset(model);hero.SetPosition({2,3,4});
    CHECK(hero.GetModelAsset()==model && hero.GetMoveSpeed()==9);
    CHECK(hero.GetRenderComponent()->GetWorldPosition()==hero.GetWorldPosition());
    Trace::Statistics traceStats;auto trace=hero.RaycastGeometry({{2.2f,3.2f,9},{0,0,-1},10},{},traceStats);
    CHECK(trace && std::abs(trace.hit->distance-5)<.001f && trace.hit->triangleIndex==0);
    const auto originalNormal=trace.hit->normal;hero.SetScale({-2,3,1});trace=hero.RaycastGeometry({{1.6f,3.6f,9},{0,0,-1},10},{},traceStats);
    CHECK(trace && glm::dot(trace.hit->normal,originalNormal)<-.99f);hero.SetScale({1,1,1});
    try {hero.SetModelAsset("/Game/missing.obj");CHECK(false);}catch(const std::runtime_error&){}
    CHECK(hero.GetModelAsset()==model);hero.SetModelAsset("");CHECK(hero.GetRenderComponent()==&hero);
    const ComponentType heroType{"Test.Hero","Hero",[]()->std::unique_ptr<GameComponent>{return std::make_unique<Character>();}};
    Scene savedScene("Visual test"),restoredScene("Restore");
    std::unique_ptr<GameComponent> ownedHero=std::make_unique<Character>();
    static_cast<Character*>(ownedHero.get())->SetMoveSpeed(12);ownedHero->SetModelAsset(model);
    CHECK(savedScene.AddComponent("Hero",ownedHero,heroType.typeName).result==SceneResult::Ok);
    auto snapshot=SceneDocument::Parse(SceneDocument::Serialize(SceneTestAccess::Snapshot(savedScene)));
    SceneDocument::Restore(snapshot,{&heroType,1},restoredScene);
    auto* restoredHero=dynamic_cast<Character*>(restoredScene.FindComponentByName("Hero"));
    CHECK(restoredHero && restoredHero->GetMoveSpeed()==12 && restoredHero->GetModelAsset()==model);
    const auto mat=modelFile.parent_path()/"Custom.mat";const auto shader=modelFile.parent_path()/"Custom.hlsl";
    Write(mat,R"({"shader":"Custom.hlsl","params":{"Value":0.4}})");Write(shader,"void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = float3(1,0,0); }");
    uint64_t mesh=0;CHECK(api.createModel(api.context,model.c_str(),"Mesh",&t,&mesh,error,sizeof(error))==EngineSceneOk);
    const auto material="/Game/"+mat.lexically_relative(content).generic_u8string();CHECK(api.setMaterial(api.context,mesh,material.c_str(),error,sizeof(error))==EngineSceneOk);
    const auto saved=content/"Scenes/Main.scene.json";CHECK(api.saveScene(api.context,saved.u8string().c_str(),error,sizeof(error))==EngineSceneOk);
    const auto oldMat=Read(mat),oldShader=Read(shader),oldModel=Read(modelFile);
    const auto sidecarPath=fs::u8path(modelFile.u8string()+".importsettings.json");
    Write(sidecarPath,R"({"version":1,"kind":"model","model":{"scale":0.01,"mergeMeshes":false}})");
    Write(root/"mesh.obj","g MeshA\nv 0 0 0\nv 2 0 0\nv 0 2 0\nf 1 2 3\ng MeshB\nv 3 0 0\nv 5 0 0\nv 3 2 0\nf 4 5 6\n");
    CHECK(author.asset(author.context,0,model.c_str(),"",error,sizeof(error))==EngineSceneOk);
    CHECK(Read(modelFile)!=oldModel);CHECK(Read(mat)==oldMat);CHECK(Read(shader)==oldShader);
    CHECK(Read(sidecarPath).find("0.01")!=std::string::npos && Read(sidecarPath).find("false")!=std::string::npos);
    const auto resSeparate=LoadSkeletalModelFromVfs(model);
    CHECK(resSeparate.Success && resSeparate.Model && resSeparate.Model->Meshes.size()==2);
    bool found=false;for(const auto& row:Rows(api))if(row.name=="Mesh"){CHECK(row.model==model&&row.material==material&&row.x==3);found=true;}CHECK(found);
    fs::remove(sidecarPath);
    CHECK(author.asset(author.context,0,model.c_str(),"",error,sizeof(error))==EngineSceneOk);
    const auto resDefault=LoadSkeletalModelFromVfs(model);
    CHECK(resDefault.Success && resDefault.Model && resDefault.Model->Meshes.size()==1);
    CHECK(Read(mat)==oldMat);
    for(const auto& row:Rows(api))if(row.name=="Mesh")CHECK(row.model==model&&row.material==material&&row.x==3);
    Write(sidecarPath,R"({"version":1,"kind":"model","model":{"scale":0.01,"mergeMeshes":false}})");
    const auto validModel=Read(modelFile);Write(root/"mesh.obj","broken geometry");
    CHECK(author.asset(author.context,0,model.c_str(),"",error,sizeof(error))!=EngineSceneOk);CHECK(Read(modelFile)==validModel);
    Write(root/"different_source.obj","v 0 0 0\nv 3 0 0\nv 0 3 0\nf 1 2 3\n");
    CHECK(author.asset(author.context,0,model.c_str(),(root/"different_source.obj").u8string().c_str(),error,sizeof(error))==EngineSceneOk);
    CHECK(!fs::exists(modelFile.parent_path()/"different_source.obj.importsettings.json"));
    CHECK(fs::exists(sidecarPath));
    CHECK(Read(sidecarPath).find("0.01")!=std::string::npos);
    const std::string moved="/Game/Models/Renamed/mesh.obj";
    CHECK(author.asset(author.context,1,model.c_str(),moved.c_str(),error,sizeof(error))==EngineSceneOk);
    CHECK(!fs::exists(modelFile));CHECK(Read(saved).find(moved)!=std::string::npos);
    const auto movedSidecar=content/"Models/Renamed/mesh.obj.importsettings.json";
    CHECK(fs::exists(movedSidecar));
    CHECK(!fs::exists(sidecarPath));
    CHECK(Read(movedSidecar).find("0.01")!=std::string::npos);
    CHECK(author.asset(author.context,1,"/Game/Models/Renamed/Custom.mat","/Game/Materials/Moved.mat",error,sizeof(error))!=EngineSceneOk);
    for(const auto& row:Rows(api))if(row.name=="Mesh")CHECK(row.model==moved&&row.material=="/Game/Models/Renamed/Custom.mat"&&row.x==3);
    Write(content/"Materials/Texture.mat",R"({"shader":"../Models/Renamed/Custom.hlsl","textures":{"BaseMap":"../Textures/pixel.bmp"}})");
    CHECK(author.asset(author.context,1,texture.c_str(),"/Game/Images/Renamed.bmp",error,sizeof(error))==EngineSceneOk);
    CHECK(Json::parse(Read(content/"Materials/Texture.mat"))["textures"]["BaseMap"]=="../Images/Renamed.bmp");
    CHECK(Read(saved).find("/Game/Images/Renamed.bmp")!=std::string::npos);
    Write(content/"Scenes/Broken.scene.json","broken");const auto sceneBefore=Read(saved);
    CHECK(author.asset(author.context,1,moved.c_str(),"/Game/Models/Rejected/mesh.obj",error,sizeof(error))!=EngineSceneOk);
    CHECK(fs::exists(content/"Models/Renamed/mesh.obj")&&!fs::exists(content/"Models/Rejected"));CHECK(Read(saved)==sceneBefore);
    CHECK(author.asset(author.context,1,moved.c_str(),"/Game/../Escape/mesh.obj",error,sizeof(error))!=EngineSceneOk);

    // --- Missing and Corrupted Model Tests (a, b, c, d, e) ---
    fs::remove(content/"Scenes/Broken.scene.json");
    const auto missingModelVirtual = "/Game/Models/NonExistentModel.obj";
    const auto missingModelPhysical = content/"Models/NonExistentModel.obj";
    CHECK(!fs::exists(missingModelPhysical));

    const auto missingSceneFile = content/"Scenes/MissingModelTest.scene.json";
    SceneSnapshot missingSnap;
    missingSnap.name = "MissingModelScene";
    missingSnap.documentVersion = 3;
    SceneComponentSnapshot comp;
    comp.id = 100;
    comp.name = "MissingObject";
    comp.typeName = SceneAssets::ModelType;
    comp.modelAsset = missingModelVirtual;
    comp.textureAsset = "/Game/Images/Renamed.bmp";
    comp.materialAsset = "/Game/Materials/Texture.mat";
    comp.projectData = R"({"version":1,"materialSlots":{"0":"/Game/Materials/Texture.mat"}})";
    missingSnap.components.push_back(comp);
    Write(missingSceneFile, SceneDocument::Serialize(missingSnap));

    // (a) Сцена с удалённой моделью открывается без исключений и создаёт плейсхолдер
    const auto snapshotMissing = SceneDocument::Read(missingSceneFile);
    Scene restoredMissingScene("RestoredMissing");
    SceneDocument::Restore(snapshotMissing, {}, restoredMissingScene);
    auto* placeholderComp = dynamic_cast<SkeletalMeshComponent*>(restoredMissingScene.FindComponentByName("MissingObject"));
    CHECK(placeholderComp != nullptr);
    CHECK(placeholderComp->GetModelAsset() == missingModelVirtual);

    // (b) Сохранение этой сцены сохраняет modelAsset, textureAsset и materialAsset без потерь
    const auto resavedSnapshot = SceneTestAccess::Snapshot(restoredMissingScene);
    CHECK(resavedSnapshot.components.size() == 1);
    const auto& resavedComp = resavedSnapshot.components.front();
    CHECK(resavedComp.modelAsset == missingModelVirtual);
    CHECK(resavedComp.textureAsset == "/Game/Images/Renamed.bmp");
    CHECK(resavedComp.materialAsset == "/Game/Materials/Texture.mat");
    CHECK(resavedComp.projectData.find("materialSlots") != std::string::npos);

    // Round-trip через Parse(Serialize) также строго сохраняет поля в снапшоте
    const auto roundTripSnapshot = SceneDocument::Parse(SceneDocument::Serialize(resavedSnapshot));
    CHECK(roundTripSnapshot.components.size() == 1);
    const auto& roundTripComp = roundTripSnapshot.components.front();
    CHECK(roundTripComp.modelAsset == missingModelVirtual);
    CHECK(roundTripComp.textureAsset == "/Game/Images/Renamed.bmp");
    CHECK(roundTripComp.materialAsset == "/Game/Materials/Texture.mat");
    CHECK(roundTripComp.projectData.find("materialSlots") != std::string::npos);

    // (c) Play запускается и останавливается на сессии со сценой с отсутствующей моделью
    uint64_t missingSceneId = 0;
    CHECK(api.loadScene(api.context, missingSceneFile.u8string().c_str(), &missingSceneId, error, sizeof(error)) == EngineSceneOk);
    CHECK(api.control(api.context, EnginePlay) == EngineSceneOk);
    CHECK(api.control(api.context, EngineStop) == EngineSceneOk);

    // (d) Реимпорт другого ассета проходит (SceneAuthoring валидирует все сцены, включая MissingModelTest.scene.json)
    CHECK(author.asset(author.context, 1, "/Game/Images/Renamed.bmp", "/Game/Images/Renamed2.bmp", error, sizeof(error)) == EngineSceneOk);
    CHECK(author.asset(author.context, 1, "/Game/Images/Renamed2.bmp", "/Game/Images/Renamed.bmp", error, sizeof(error)) == EngineSceneOk);

    // (e) п.1: Реимпорт/валидация существующего, но битого файла модели откатывается
    const auto corruptModelVirtual = "/Game/Models/CorruptMesh.obj";
    const auto corruptModelPhysical = content/"Models/CorruptMesh.obj";
    Write(corruptModelPhysical, "THIS IS CORRUPT GEOMETRY THAT CANNOT BE LOADED");
    CHECK(fs::exists(corruptModelPhysical));

    const auto corruptSceneFile = content/"Scenes/CorruptScene.scene.json";
    SceneSnapshot corruptSnap;
    corruptSnap.name = "CorruptModelScene";
    corruptSnap.documentVersion = 3;
    SceneComponentSnapshot corruptComp;
    corruptComp.id = 200;
    corruptComp.name = "CorruptObject";
    corruptComp.typeName = SceneAssets::ModelType;
    corruptComp.modelAsset = corruptModelVirtual;
    corruptSnap.components.push_back(corruptComp);
    Write(corruptSceneFile, SceneDocument::Serialize(corruptSnap));

    // Для существующего битого файла SceneDocument::Restore ОБЯЗАН выбрасывать исключение
    const auto snapshotCorrupt = SceneDocument::Read(corruptSceneFile);
    Scene restoredCorruptScene("RestoredCorrupt");
    bool threwOnExistingCorrupt = false;
    try {
        SceneDocument::Restore(snapshotCorrupt, {}, restoredCorruptScene);
    } catch (const std::runtime_error&) {
        threwOnExistingCorrupt = true;
    }
    CHECK(threwOnExistingCorrupt);

    // Наличие существующей битой модели на сцене в проекте блокирует валидацию и откатывает реимпорт
    const auto sceneBeforeReimport = Read(missingSceneFile);
    CHECK(author.asset(author.context, 1, "/Game/Images/Renamed.bmp", "/Game/Images/MustRollback.bmp", error, sizeof(error)) != EngineSceneOk);
    CHECK(!fs::exists(content/"Images/MustRollback.bmp"));
    CHECK(Read(missingSceneFile) == sceneBeforeReimport);

    // Реимпорт САМОЙ модели с новым битым исходником откатывается: код ошибки, старый файл модели и sidecar как до
    const auto movedModelFile = content/"Models/Renamed/mesh.obj";
    const auto movedSidecarFile = content/"Models/Renamed/mesh.obj.importsettings.json";
    CHECK(fs::exists(movedModelFile));
    CHECK(fs::exists(movedSidecarFile));
    const auto modelFileBefore = Read(movedModelFile);
    const auto sidecarBefore = Read(movedSidecarFile);
    const auto brokenSource = root/"broken_source_replacement.obj";
    Write(brokenSource, "NOT A VALID 3D OBJ GEOMETRY");
    CHECK(author.asset(author.context, 0, moved.c_str(), brokenSource.u8string().c_str(), error, sizeof(error)) != EngineSceneOk);
    CHECK(Read(movedModelFile) == modelFileBefore);
    CHECK(Read(movedSidecarFile) == sidecarBefore);

    // Очистка тестовых файлов
    fs::remove(corruptSceneFile);
    fs::remove(corruptModelPhysical);
    fs::remove(missingSceneFile);
    fs::remove(brokenSource);
}
