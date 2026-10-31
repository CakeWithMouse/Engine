#define NOMINMAX
#include <Windows.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Components/EditorProperties.h>
#include <Engine/Scene/SceneAssets.h>
#include "../ThirdParty/nlohmann/json.hpp"
#include <fstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace fs=std::filesystem;
#define CHECK(v) do {if(!(v))throw std::runtime_error(#v);}while(false)
namespace {
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
    const auto model=SceneAssets::Import(content,root/"mesh.obj",SceneAssets::Model);const auto modelFile=content/fs::u8path(model.substr(6));
    const auto mat=modelFile.parent_path()/"Custom.mat";const auto shader=modelFile.parent_path()/"Custom.hlsl";
    Write(mat,R"({"shader":"Custom.hlsl","params":{"Value":0.4}})");Write(shader,"void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = float3(1,0,0); }");
    uint64_t mesh=0;CHECK(api.createModel(api.context,model.c_str(),"Mesh",&t,&mesh,error,sizeof(error))==EngineSceneOk);
    const auto material="/Game/"+mat.lexically_relative(content).generic_u8string();CHECK(api.setMaterial(api.context,mesh,material.c_str(),error,sizeof(error))==EngineSceneOk);
    const auto saved=content/"Scenes/Main.scene.json";CHECK(api.saveScene(api.context,saved.u8string().c_str(),error,sizeof(error))==EngineSceneOk);
    const auto oldMat=Read(mat),oldShader=Read(shader),oldModel=Read(modelFile);
    Write(root/"mesh.obj","v 0 0 0\nv 2 0 0\nv 0 2 0\nf 1 2 3\n");
    CHECK(author.asset(author.context,0,model.c_str(),"",error,sizeof(error))==EngineSceneOk);
    CHECK(Read(modelFile)!=oldModel);CHECK(Read(mat)==oldMat);CHECK(Read(shader)==oldShader);
    bool found=false;for(const auto& row:Rows(api))if(row.name=="Mesh"){CHECK(row.model==model&&row.material==material&&row.x==3);found=true;}CHECK(found);
    const auto validModel=Read(modelFile);Write(root/"mesh.obj","broken geometry");
    CHECK(author.asset(author.context,0,model.c_str(),"",error,sizeof(error))!=EngineSceneOk);CHECK(Read(modelFile)==validModel);
    const std::string moved="/Game/Models/Renamed/mesh.obj";
    CHECK(author.asset(author.context,1,model.c_str(),moved.c_str(),error,sizeof(error))==EngineSceneOk);
    CHECK(!fs::exists(modelFile));CHECK(Read(saved).find(moved)!=std::string::npos);
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
}
