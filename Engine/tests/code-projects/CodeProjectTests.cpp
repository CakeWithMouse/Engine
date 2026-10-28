#include <Engine/Components/ProjectObject.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/Project/StandardTypes.h>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>
static int checks=0, gameTicks=0, componentTicks=0, parents=0, liveObjects=0;
static void Check(bool value,const char* text) { if(!value) throw std::runtime_error(text); ++checks; }
struct ParentComponent : GameComponent { void Tick(float dt) override { GameComponent::Tick(dt); ++parents; } };
struct Counter : ParentComponent {
    int amount=7;
    ComponentId reference=0;
    void Tick(float dt) override { ParentComponent::Tick(dt); ++componentTicks; ++amount; }
    std::string SaveProjectData() const override { return nlohmann::json{{"version",1},{"amount",amount},{"reference",reference}}.dump(); }
    void LoadProjectData(const std::string& text) override {
        if(text.empty()) return;
        const auto json=nlohmann::json::parse(text);
        if(json.at("version")!=1) throw std::runtime_error("Unsupported Counter schema");
        amount=json.at("amount").get<int>();
        reference=json.value("reference",ComponentId{0});
    }
    void RemapProjectReferences(const std::map<ComponentId,ComponentId>& ids) override {
        auto found=ids.find(reference); if(found!=ids.end()) reference=found->second;
    }
};
struct RulesBase { virtual ~RulesBase()=default; };
struct Rules : RulesBase { int limit=42; Rules(){++liveObjects;} ~Rules(){--liveObjects;} };
template<> struct ProjectObjectAdapter<Rules> {
    static std::unique_ptr<Rules> Create(){return std::make_unique<Rules>();}
    static void Tick(Rules&,float){}
    static std::string Save(const Rules& r){return nlohmann::json{{"version",1},{"limit",r.limit}}.dump();}
    static void Load(Rules& r,const std::string& text){if(!text.empty()){auto json=nlohmann::json::parse(text); if(json.at("version")!=1)throw std::runtime_error("Bad rules schema"); r.limit=json.at("limit").get<int>();}}
};
struct ProjectGame : Game {
    void PreUpdate(float dt) override { Game::PreUpdate(dt); ++gameTicks; }
    void TickComponents(float dt) override { Game::TickComponents(dt); }
};
int main() {
 try {
    const auto standard = SceneProject::GetComponentTypes();
    for (const char* type : {"Primitive.Cube", "Primitive.Sphere", "Primitive.Plane", "Primitive.Cylinder", "Primitive.Cone", "Primitive.Pyramid"}) {
        auto object = standard.Find(type)->create();
        Check(dynamic_cast<MeshComponent*>(object.get()) != nullptr, "SDK primitive has normals, UVs and material support");
        Check(object->SetMaterialAsset("/Game/Materials/Test.mat"), "SDK primitive accepts a material reference");
    }
    Check(standard.Find("Gameplay.Character") && standard.Find("Particle.Sparks") && standard.Find("Environment.Sky"), "material primitives preserve main's standard types");
    ComponentType types[]={{"Counter","Counter",EngineComponentDetail::Create<Counter>},{"Rules","Rules",EngineComponentDetail::CreateObject<Rules>}};
    ComponentTypeCatalog catalog{types,2};
    {
        SceneEditSession edit(catalog,&CreateProjectGame<ProjectGame>);
        auto api=edit.GetApi(); auto data=edit.GetProjectDataApi(); uint64_t counter=0,rules=0;
        Check(liveObjects==0,"catalog does not instantiate types");
        Check(api.scene.createObject(api.scene.context,"Counter","Counter",nullptr,&counter)==EngineSceneOk,"spawn derived component");
        Check(api.scene.createObject(api.scene.context,"Rules","Rules",nullptr,&rules)==EngineSceneOk && liveObjects==1,"spawn independent hierarchy");
        Check(data.write(data.context,counter,"{\"version\":1,\"amount\":91}")==EngineSceneOk,"edit custom component data");
        Check(data.write(data.context,rules,"{\"version\":1,\"limit\":99}")==EngineSceneOk,"edit independent object data");
        Check(data.write(data.context,counter,"{\"version\":9,\"amount\":1}")!=EngineSceneOk,"reject incompatible schema");
        std::string text;
        auto read=[](void* p,const char* value){*static_cast<std::string*>(p)=value;};
        Check(data.read(data.context,counter,read,&text)==EngineSceneOk && nlohmann::json::parse(text)["amount"]==91,"failed write preserves value");
        Check(api.frame(api.context,.016)==EngineSceneOk && gameTicks==0 && componentTicks==0,"edit does not simulate");
        Check(api.control(api.context,EnginePlay)==EngineSceneOk && liveObjects==2,"Play owns independent copy");
        Check(api.frame(api.context,.016)==EngineSceneOk && gameTicks==1 && componentTicks==1 && parents==1,"Game and parent/component tick exactly once");
        Check(data.read(data.context,counter,read,&text)==EngineSceneOk && nlohmann::json::parse(text)["amount"]==92,"Play reads runtime data, not edit copy");
        Check(data.write(data.context,counter,"{}")==EngineSceneReadOnly,"Play data read only");
        Check(api.control(api.context,EnginePause)==EngineSceneOk,"pause");
        Check(api.frame(api.context,.016)==EngineSceneOk && gameTicks==1,"paused does not simulate");
        Check(api.control(api.context,EngineStop)==EngineSceneOk && liveObjects==1,"Stop destroys game copy only");
        Check(data.read(data.context,counter,read,&text)==EngineSceneOk && nlohmann::json::parse(text)["amount"]==91,"edit data survives Play");
        const auto linked=nlohmann::json{{"version",1},{"amount",91},{"reference",counter}}.dump();
        Check(data.write(data.context,counter,linked.c_str())==EngineSceneOk,"set self reference");
        uint64_t copy=0;
        Check(api.scene.createObject(api.scene.context,"Counter","Copy",nullptr,&copy)==EngineSceneOk,"create duplicate target");
        Check(data.write(data.context,copy,linked.c_str())==EngineSceneOk && data.remapDuplicate(data.context,copy,counter)==EngineSceneOk,"remap duplicate self reference");
        Check(data.read(data.context,copy,read,&text)==EngineSceneOk && nlohmann::json::parse(text)["reference"]==copy,"copy references itself");
    }
    Check(liveObjects==0,"all independent objects destroyed");
    Scene source;
    auto object=EngineComponentDetail::Create<Counter>(); static_cast<Counter*>(object.get())->amount=123;
    Check(source.AddComponent("Stored",object,"Counter").result==SceneResult::Ok,"scene owns object");
    const auto snapshot=source.GetSnapshot(); Check(snapshot.documentVersion==3,"custom payload uses explicit format 3");
    const auto parsed=SceneDocument::Parse(SceneDocument::Serialize(snapshot)); Scene restored;
    SceneDocument::Restore(parsed,catalog,restored);
    Check(dynamic_cast<Counter*>(restored.FindComponentByName("Stored"))->amount==123,"codec round trip");
    auto invalid=parsed; invalid.components[0].typeName="Missing"; Scene untouched;
    bool failed=false;try{SceneDocument::Restore(invalid,catalog,untouched);}catch(...){failed=true;}
    Check(failed && untouched.GetComponents().empty(),"unknown type preserves target");
    std::cout << "Code project core: " << checks << " checks passed\n"; return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
