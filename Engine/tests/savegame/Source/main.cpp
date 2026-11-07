#define NOMINMAX
#include <Engine/SaveGame/SaveGame.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/Light/DirectionalLightComponent.h>
#include <Engine/Components/Character.h>
#include <Engine/MainGame/CharacterController.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Resources/Paths.h>
#include <iostream>
#include <fstream>
#include <thread>

static int checks=0;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
template<class F> void Reject(F&& f,const char* message){bool rejected=false;try{f();}catch(...){rejected=true;}Check(rejected,message);}
class Probe : public CubeComponent {
public:
    float elapsed=0;uint64_t target=0;int begins=0;
    void Tick(float dt)override{elapsed+=dt;}
    void BeginPlay()override{++begins;}
    void SerializeSaveGame(SaveArchive& a)override{
        CubeComponent::SerializeSaveGame(a);a.Section("Probe",[&](SaveArchive& s){s.Version(1);s.Field("elapsed",elapsed);s.Reference("target",target);});
    }
};
class Counter : public GameSubsystem {
public:
    int value=0,loads=0;bool stopOnLoad=false;GameSession* session=nullptr;
    bool Initialize(SubsystemContext& context,std::string&)override{session=&context.session;return true;}
    void SerializeSaveGame(SaveArchive& a)override{GameSubsystem::SerializeSaveGame(a);a.Section("Counter",[&](SaveArchive& s){s.Version(1);s.Field("value",value);});}
    void OnSaveGameLoaded(Scene&)override{++loads;if(stopOnLoad)session->Stop();}
};
class TestGame : public Game {
public:
    int score=0,loads=0;
    void OnSaveGameLoaded()override{++loads;}
    void SerializeSaveGame(SaveArchive& a)override{Game::SerializeSaveGame(a);a.Section("TestGame",[&](SaveArchive& s){s.Version(1);s.Field("score",score);if(score<0)throw std::runtime_error("Invalid score");});}
};
static const ComponentType types[]={
    {"Probe","Probe",EngineComponentDetail::Create<Probe>},
    {"Mesh","Mesh",EngineComponentDetail::Create<MeshComponent>},
    {"Instances","Instances",EngineComponentDetail::Create<InstancedMeshComponent>},
    {"UI","UI",EngineComponentDetail::Create<UIComponent>},
    {"Particles","Particles",EngineComponentDetail::Create<ParticleSystemComponent>},
    {"Skeletal","Skeletal",EngineComponentDetail::Create<SkeletalMeshComponent>},
    {"Light.Point","Point",[]()->std::unique_ptr<GameComponent>{return std::make_unique<PointLightComponent>(glm::vec3(0),glm::vec3(1));}},
    {"Light.Directional","Sun",EngineComponentDetail::Create<DirectionalLightComponent>},
    {"Camera","Camera",EngineComponentDetail::Create<SceneCamera>},
    {"Character","Character",EngineComponentDetail::Create<Character>}
};
static ComponentTypeCatalog Catalog(){return {types,std::size(types)};}
static const GameSubsystemType subsystems[]={{"Counter",CreateGameSubsystem<Counter>}};
static void Configure(TestGame& game,GameSession& session,const std::filesystem::path& root){
    std::string error;Check(session.Initialize({subsystems,1},error),"initialize subsystem");game.AttachSession(session);
    game.Saves().SetCatalog(Catalog());game.Saves().Configure("savegame-tests",root);game.SetSimulationTimeScale(1);
    game.InitializeGameplay();
}
static Probe* Find(TestGame& game,const char* name){return dynamic_cast<Probe*>(game.GetScene().FindComponentByName(name));}
static TestGame* viewportGame=nullptr;
static std::unique_ptr<Game> ViewportFactory(){auto game=std::make_unique<TestGame>();viewportGame=game.get();return game;}
int wmain(int argc,wchar_t** argv){try{
    Check(argc>=2,"test root argument");const auto root=std::filesystem::absolute(argv[1]);
    if(argc==3 && std::wstring(argv[2])==L"--read"){
        GameSession session;TestGame game;Configure(game,session,root);game.Saves().RequestLoad("restart");game.Saves().ProcessPending();
        Check(game.Saves().LastResult().Succeeded(),game.Saves().LastResult().message.c_str());
        Check(Find(game,"Parent")->elapsed==3.25f && game.score==42,"cross-process data restored");
        std::cout<<"SaveGame cross-process read passed\n";return 0;
    }
    GameSession session;TestGame game;Configure(game,session,root);
    // BUG-R2-002: a restored disabled controller must ignore both movement and look.
    {
        GameSession inputSession;TestGame inputGame;Configure(inputGame,inputSession,root);
        std::unique_ptr<GameComponent> actor=std::make_unique<Character>();
        auto* character=static_cast<Character*>(actor.get());character->standardControl=true;
        const auto added=inputGame.GetScene().AddComponent("Controlled",actor,"Character");
        inputGame.SetCharacterController(std::make_unique<CharacterController>(inputGame));
        auto* control=inputGame.GetCharacterController();control->Possess(added.id);control->BindInput();
        for(bool enabled:{false,true}){
            control->SetEnabled(enabled);const auto saved=inputGame.Saves().Capture();
            inputGame.Saves().Restore(saved);control=inputGame.GetCharacterController();
            Player camera;control->UpdateCamera(camera);const auto beforeYaw=camera.GetYaw();
            const auto beforePosition=control->GetCharacter()->GetLocalPosition();
            auto& input=inputGame.GetInput();input.ReleaseAll();input.SetContextEnabled(Input::Context::Game,true);
            input.SetKey(Keys::W,true);input.SetKey(Keys::RightButton,true);input.AddPointer(60,0);input.Tick(.1f);
            control->Tick(.1f);control->UpdateCamera(camera);
            Check(enabled ? std::abs(camera.GetYaw()-beforeYaw)>.01f : camera.GetYaw()==beforeYaw,"restored controller enabled state gates look");
            Check(enabled ? control->GetCharacter()->GetLocalPosition()!=beforePosition : control->GetCharacter()->GetLocalPosition()==beforePosition,"restored controller enabled state gates movement");
            input.ReleaseAll();
        }
    }
    std::unique_ptr<GameComponent> unused=std::make_unique<Probe>();auto hole=game.GetScene().AddComponent("removed",unused,"Probe");game.GetScene().RemoveComponent(hole.id);
    std::unique_ptr<GameComponent> parent=std::make_unique<Probe>(),child=std::make_unique<Probe>();
    const auto p=game.GetScene().AddComponent("Parent",parent,"Probe"),c=game.GetScene().AddComponent("Child",child,"Probe");
    game.GetScene().SetParent(c.id,p.id,false);game.GetScene().FindComponent(p.id)->SetPosition({2,3,4});
    Find(game,"Parent")->elapsed=3.25f;Find(game,"Parent")->target=c.id;
    Find(game,"Parent")->SetCollision(true);Find(game,"Parent")->SetColor({.2f,.3f,.4f,1});Find(game,"Parent")->traceChannels=17;
    game.GetSubsystem<Counter>()->value=91;game.score=42;game.SetSimulationEnabled(false);
    const auto snapshot=game.Saves().Capture();game.Saves().WriteSlot("restart",snapshot);
    auto original=SaveArchive::Json::parse(snapshot);
    Find(game,"Parent")->elapsed=90;game.score=5;game.GetSubsystem<Counter>()->value=0;
    game.Saves().Restore(snapshot);
    Check(game.score==42&&game.GetSubsystem<Counter>()->value==91,"derived game and subsystem round trip");
    Check(game.GetSubsystem<Counter>()->loads==1,"one subsystem notification");
    Check(Find(game,"Parent")->target==Find(game,"Child")->GetComponentId(),"derived reference remapped");
    Check(Find(game,"Child")->GetParent()==Find(game,"Parent"),"hierarchy restored");
    Check(Find(game,"Parent")->HasCollison()&&Find(game,"Parent")->traceChannels==17,"base fields restored");
    Check(Find(game,"Parent")->WasLoadedFromSaveGame()&&game.WasLoadedFromSaveGame(),"load reason exposed");
    Check(!game.IsSimulationEnabled(),"simulation status restored");
    Check(Find(game,"Parent")->elapsed==3.25f&&Find(game,"Parent")->GetLocalPosition()==glm::vec3(2,3,4),"scene and runtime data restored");
    auto* alive=Find(game,"Parent");
    for(int variant=0;variant<8;++variant){auto bad=original;
        if(variant==0)bad["format"]=2;
        if(variant==1)bad["gameId"]="other";
        if(variant==2)bad["scene"]["objects"][0]["type"]="Missing";
        if(variant==3)bad["components"][std::to_string(p.id)]["Probe"]["target"]=99999;
        if(variant==4)bad["game"]["TestGame"]["score"]=-1;
        if(variant==5)bad["subsystems"]["Unknown"]=SaveArchive::Json::object();
        if(variant==6)bad["components"].erase(std::to_string(c.id));
        if(variant==7)bad["components"][std::to_string(p.id)]["GameComponent"]["color"][0]=1e100;
        Reject([&]{game.Saves().Restore(bad.dump());},"invalid save rejected");
        Check(Find(game,"Parent")==alive&&game.score==42&&game.GetSubsystem<Counter>()->value==91,"failed load preserves live world and data");
    }
    Reject([&]{game.Saves().Restore(snapshot.substr(0,30));},"truncated JSON rejected");
    Reject([&]{game.Saves().WriteSlot("../escape",snapshot);},"traversal rejected");
    Reject([&]{game.Saves().Restore(std::string(16*1024*1024+1,'x'));},"size limit");
    auto locked=root/"savegame-tests/default/restart.save.json";
    HANDLE lock=CreateFileW(locked.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    Check(lock!=INVALID_HANDLE_VALUE,"lock save file");
    Reject([&]{game.Saves().WriteSlot("restart",snapshot);},"failed replace reported");CloseHandle(lock);
    Check(game.Saves().ReadSlot("restart")==snapshot,"failed write preserves slot");
    for(const auto& entry:std::filesystem::directory_iterator(locked.parent_path()))Check(entry.path().extension()==".json","temporary files cleaned");
    {auto update=game.GetScene().BeginUpdate();std::unique_ptr<GameComponent> deferred=std::make_unique<Probe>();
        game.GetScene().AddComponent("Deferred",deferred,"Probe");
        Check(game.Saves().RequestSave("deferred").status==SaveGameResult::Status::Accepted,"save queued");
        Check(game.Saves().RequestLoad("restart").status==SaveGameResult::Status::Busy,"second request busy");
        game.Saves().ProcessPending();Check(game.Saves().LastResult().status==SaveGameResult::Status::Accepted,"no capture mid-update");}
    game.Saves().ProcessPending();Check(game.Saves().LastResult().Succeeded(),"deferred save completed");
    Check(SaveArchive::Json::parse(game.Saves().ReadSlot("deferred"))["scene"]["objects"].size()==3,"deferred object included");
    game.Saves().DeleteSlot("deferred");Check(game.Saves().ListSlots()==std::vector<std::string>{"restart"},"list and delete slots");
    bool wrongThread=false;std::thread thread([&]{try{game.Saves().Capture();}catch(...){wrongThread=true;}});thread.join();Check(wrongThread,"wrong thread rejected");
    // Built-in classes use the same engine path; no game-specific codec.
    for(size_t i=1;i<std::size(types);++i){auto component=types[i].create();game.GetScene().AddComponent(types[i].typeName,component,types[i].typeName);}
    auto* ui=dynamic_cast<UIComponent*>(game.GetScene().FindComponentByName("UI"));
    auto label=std::make_unique<UILabel>();label->Name="saved-label";label->SetText("Сохранено");ui->Add(std::move(label));
    auto* mesh=dynamic_cast<MeshComponent*>(game.GetScene().FindComponentByName("Mesh"));mesh->SetMesh(MeshPrimitives::Cube());
    auto* instances=dynamic_cast<InstancedMeshComponent*>(game.GetScene().FindComponentByName("Instances"));
    Check(instances->SetLod(0,MeshPrimitives::Cube()),"set saved instance geometry");
    MeshInstance instance;instance.Position={4,5,6};instance.AnimationPhase=.7f;instance.CustomData={.1f,.2f,.3f};instances->AddInstance(instance);
    auto* skeletal=dynamic_cast<SkeletalMeshComponent*>(game.GetScene().FindComponentByName("Skeletal"));skeletal->SetAnimationTime(2.5f);skeletal->SetPlaybackSpeed(.5f);
    auto* point=dynamic_cast<PointLightComponent*>(game.GetScene().FindComponentByName("Light.Point"));point->SetIntensity(3);point->SetRadius(17);point->SetEnabled(false);
    auto* camera=dynamic_cast<SceneCamera*>(game.GetScene().FindComponentByName("Camera"));camera->orthographic=true;camera->orthoHeight=31;
    auto* particles=dynamic_cast<ParticleSystemComponent*>(game.GetScene().FindComponentByName("Particles"));particles->SetParticleCount(127);particles->SetBrightness(7);
    auto all=game.Saves().Capture();game.Saves().Restore(all);
    Check(dynamic_cast<MeshComponent*>(game.GetScene().FindComponentByName("Mesh"))->GetMesh().Indices.size()>0,"procedural mesh restored");
    const auto& restoredMesh=dynamic_cast<MeshComponent*>(game.GetScene().FindComponentByName("Mesh"))->GetMesh();
    const auto expectedMesh=MeshPrimitives::Cube();
    Check(restoredMesh.Vertices.size()==expectedMesh.Vertices.size(),"saved mesh vertex count preserved");
    for(size_t i=0;i<restoredMesh.Vertices.size();++i){
        const auto& actual=restoredMesh.Vertices[i].Tangent;
        const auto& expected=expectedMesh.Vertices[i].Tangent;
        Check(std::abs(actual.x-expected.x)<1e-5f&&std::abs(actual.y-expected.y)<1e-5f&&
              std::abs(actual.z-expected.z)<1e-5f&&actual.w==expected.w,"saved mesh tangent regenerated with handedness");
    }
    Check(dynamic_cast<UIComponent*>(game.GetScene().FindComponentByName("UI"))->Find<UILabel>("saved-label")->GetText()=="Сохранено","UI state and Unicode restored");
    instances=dynamic_cast<InstancedMeshComponent*>(game.GetScene().FindComponentByName("Instances"));
    Check(instances->GetMeshInstanceCount()==1&&instances->GetMeshInstance(0).Position==instance.Position&&instances->GetMeshInstance(0).AnimationPhase==.7f&&instances->GetIndexCount()>0,"instance geometry, transform and animation phase restored");
    skeletal=dynamic_cast<SkeletalMeshComponent*>(game.GetScene().FindComponentByName("Skeletal"));Check(skeletal->GetAnimationTime()==2.5f&&skeletal->GetPlaybackSpeed()==.5f,"skeletal playback state restored");
    point=dynamic_cast<PointLightComponent*>(game.GetScene().FindComponentByName("Light.Point"));Check(point->GetIntensity()==3&&point->GetRadius()==17&&!point->IsEnabled(),"point light state restored");
    camera=dynamic_cast<SceneCamera*>(game.GetScene().FindComponentByName("Camera"));Check(camera->orthographic&&camera->orthoHeight==31,"scene camera projection restored");
    const auto restoredAll=SaveArchive::Json::parse(game.Saves().Capture());
    const auto particleId=game.GetScene().FindComponentByName("Particles")->GetComponentId();
    Check(restoredAll["components"][std::to_string(particleId)]["ParticleSystemComponent"]["brightness"]==7,"particle emitter settings restored");
    game.StepProjectSimulation(.5f);Check(Find(game,"Parent")->elapsed==3.75f,"restored component ticks");
    Check(Find(game,"Parent")->begins==1,"BeginPlay exactly once after restore");
    if(argc==3){
        EnginePaths::SetContentRoot(std::filesystem::path(argv[2]));
        HWND window=CreateWindowExW(0,L"STATIC",L"SaveGame viewport test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Check(window!=nullptr,"create viewport");
        {
            SceneEditSession editor(Catalog(),ViewportFactory);auto api=editor.GetApi();uint64_t id=0;
            Check(api.scene.createObject(api.scene.context,"Probe","Visible",nullptr,&id)==EngineSceneOk,"create edit object");
            Check(api.attachViewport(api.context,window,320,240)==EngineSceneOk,"attach D3D viewport");
            Check(api.control(api.context,EnginePlay)==EngineSceneOk,"enter Play");
            viewportGame->Saves().Configure("viewport",root);viewportGame->Saves().RequestSave("frame");
            Check(api.frame(api.context,.025)==EngineSceneOk,"save frame");
            Check(viewportGame->Saves().LastResult().Succeeded(),"automatic frame boundary saves");
            {
                TestGame cold;cold.Saves().SetCatalog(Catalog());cold.Saves().Configure("viewport",root);
                auto coldData=SaveArchive::Json::parse(viewportGame->Saves().ReadSlot("frame"));
                coldData["game"]["Game"]["HUD"]=restoredAll["components"][std::to_string(game.GetScene().FindComponentByName("UI")->GetComponentId())];
                cold.Saves().Restore(coldData.dump());
                Check(cold.GetPlayer()!=nullptr&&cold.GetPlayer()->GetPosition()==viewportGame->GetPlayer()->GetPosition(),"cold load retains player camera before window creation");
                Check(cold.loads==0,"cold load defers notification until gameplay is ready");
                cold.InitializeGameplay();cold.InitializeGameplay();
                Check(cold.loads==1,"cold load notifies once after gameplay initialization");
                Check(SaveArchive::Json::parse(cold.Saves().Capture())["game"]["Game"]["HUD"]==coldData["game"]["Game"]["HUD"],"cold load retains saved project HUD through gameplay initialization");
            }
            const auto savedTime=Find(*viewportGame,"Visible")->elapsed;
            Check(api.frame(api.context,.05)==EngineSceneOk,"advance saved world");
            viewportGame->Saves().RequestLoad("frame");
            Check(api.frame(api.context,.1)==EngineSceneOk,"load with attached viewport");
            Check(viewportGame->Saves().LastResult().Succeeded(),viewportGame->Saves().LastResult().message.c_str());
            Check(Find(*viewportGame,"Visible")->elapsed==savedTime,"load restores saved simulation time");
            Check(api.frame(api.context,.02)==EngineSceneOk&&Find(*viewportGame,"Visible")->elapsed>savedTime,"render and simulation continue after load");
            Check(api.control(api.context,EnginePause)==EngineSceneOk,"pause after restore");
            Check(api.control(api.context,EngineStop)==EngineSceneOk,"stop restored game");
            EngineSceneInfo info{};Check(api.scene.readScene(api.scene.context,&info,[](void*,const EngineSceneObjectInfo*){},nullptr)==EngineSceneOk&&info.objectCount==1,"editor source scene preserved");
        }
        DestroyWindow(window);
    }
    {
        GameSession otherSession;TestGame other;Configure(other,otherSession,root/"notify");
        auto saved=other.Saves().Capture();other.Saves().WriteSlot("notify",saved);other.GetSubsystem<Counter>()->stopOnLoad=true;
        other.Saves().RequestLoad("notify");other.Saves().ProcessPending();
        Check(!otherSession.IsRunning(),"Stop inside load callback safely destroys subsystems after iteration");
        Check(other.Saves().LastResult().status==SaveGameResult::Status::Loaded&&!other.Saves().LastResult().message.empty(),"post-commit Stop reports loaded world and warning");
    }
    game.Saves().RequestSave("cancelled");session.Stop();game.Saves().ProcessPending();
    Check(game.Saves().LastResult().status==SaveGameResult::Status::Failed,"Stop cancels pending operation");
    Check(!std::filesystem::exists(root/"savegame-tests/default/cancelled.save.json"),"Stop creates no save file");
    std::cout<<"SaveGameTests: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"SaveGameTests: "<<e.what()<<"\n";return 1;}}
