#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/Scene/SceneDocument.h>
#include "../ThirdParty/nlohmann/json.hpp"
#include <stdexcept>
#include <limits>
#define CHECK(v) do { if(!(v))throw std::runtime_error(#v); } while(false)
namespace {
struct Reference : GameComponent {
    uint64_t link=0;
    std::string SaveProjectData() const override { return nlohmann::json{{"version",1},{"link",link}}.dump(); }
    void LoadProjectData(const std::string& value) override { if(!value.empty())link=nlohmann::json::parse(value).at("link").get<uint64_t>(); }
    void RemapProjectReferences(const std::map<ComponentId,ComponentId>& map) override { if(map.count(link))link=map.at(link); }
};
std::unique_ptr<GameComponent> Make(){return std::make_unique<Reference>();}
struct Item { uint64_t id,parent;std::string name;EngineSceneTransform transform; };
std::vector<Item> Read(const EngineEditSessionApi& api) {
    std::vector<Item> rows;EngineSceneInfo info{};
    CHECK(api.readActiveScene(api.context,&info,[](void* p,const EngineSceneObjectInfo* o){static_cast<std::vector<Item>*>(p)->push_back({o->id,o->parentId,o->name,o->transform});},&rows)==EngineSceneOk);
    return rows;
}
}
void SceneBatchTests() {
    const ComponentType type{"Test.Reference","Reference",Make};SceneEditSession session({&type,1});
    const auto api=session.GetApi();const auto batch=session.GetBatchApi();const auto data=session.GetProjectDataApi();
    EngineSceneTransform t{{0,0,0},{0,0,0,1},{1,1,1}};std::vector<uint64_t> ids(100),out(100);char error[1024]{};
    for(int i=0;i<100;++i){t.position[0]=static_cast<float>(i);CHECK(api.scene.createObject(api.scene.context,"Test.Reference",("Object"+std::to_string(i)).c_str(),&t,&ids[i])==EngineSceneOk);}
    CHECK(api.setParent(api.context,ids[1],ids[0],1)==EngineSceneOk);
    const auto payload=nlohmann::json{{"version",1},{"link",ids[1]}}.dump();CHECK(data.write(data.context,ids[0],payload.c_str())==EngineSceneOk);
    t.position[0]=10;
    CHECK(batch.apply(batch.context,EngineBatchTransform,ids.data(),100,ids[0],&t,out.data(),error,sizeof(error))==EngineSceneOk);
    auto rows=Read(api);for(const auto& o:rows){if(o.id==ids[0])CHECK(o.transform.position[0]==10);if(o.id==ids[1])CHECK(o.transform.position[0]==1);}
    CHECK(out==ids);
    CHECK(batch.apply(batch.context,EngineBatchTransform,ids.data(),100,ids[1],&t,out.data(),error,sizeof(error))!=EngineSceneOk);
    t.scale[0]=0;
    CHECK(batch.apply(batch.context,EngineBatchTransform,ids.data(),100,ids[0],&t,out.data(),error,sizeof(error))!=EngineSceneOk);
    CHECK(Read(api).size()==100);t.scale[0]=1;
    t.rotation[1]=t.rotation[3]=std::sqrt(0.5f);
    CHECK(batch.apply(batch.context,EngineBatchTransform,ids.data(),100,ids[0],&t,out.data(),error,sizeof(error))==EngineSceneOk);
    rows=Read(api);for(const auto& o:rows)if(o.id==ids[2]){CHECK(std::abs(o.transform.position[0]-10)<0.001f);CHECK(std::abs(o.transform.position[2]+2)<0.001f);}
    t.scale[0]=t.scale[1]=t.scale[2]=2;
    CHECK(batch.apply(batch.context,EngineBatchTransform,ids.data(),100,ids[0],&t,out.data(),error,sizeof(error))==EngineSceneOk);
    rows=Read(api);for(const auto& o:rows){if(o.id==ids[2])CHECK(std::abs(o.transform.position[2]+4)<0.001f);if(o.id==ids[1])CHECK(o.transform.scale[0]==1);}
    CHECK(batch.apply(batch.context,EngineBatchDuplicate,ids.data(),100,ids[0],nullptr,out.data(),error,sizeof(error))==EngineSceneOk);
    CHECK(Read(api).size()==200);
    // Read payload through the public visitor, including links between clones.
    std::string copied;CHECK(data.read(data.context,out[0],[](void* p,const char* json){*static_cast<std::string*>(p)=json;},&copied)==EngineSceneOk);
    CHECK(nlohmann::json::parse(copied).at("link").get<uint64_t>()==out[1]);
    rows=Read(api);for(const auto& o:rows)if(o.id==out[1])CHECK(o.parent==out[0]);
    CHECK(batch.select(batch.context,out.data(),100)==EngineSceneOk);
    CHECK(api.control(api.context,EnginePlay)==EngineSceneOk);
    CHECK(batch.apply(batch.context,EngineBatchDelete,out.data(),100,out[0],nullptr,ids.data(),error,sizeof(error))==EngineSceneReadOnly);
    CHECK(api.control(api.context,EngineStop)==EngineSceneOk);
    CHECK(batch.apply(batch.context,EngineBatchDelete,out.data(),100,out[0],nullptr,ids.data(),error,sizeof(error))==EngineSceneOk);
    CHECK(Read(api).size()==100);
}
