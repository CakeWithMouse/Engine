#include "../SceneTestAccess.h"
#define NOMINMAX
#include <Engine/InputDevice/InputActions.h>
#include <Engine/Components/Character.h>
#include <Engine/MainGame/CharacterController.h>
#include <Engine/Project/StandardTypes.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Runtime/GameEditModule.h>
#include <Engine/Scene/SceneDocument.h>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include "InputExamples.h"

static int checks=0;
static void Check(bool v,const char* text){if(!v)throw std::runtime_error(text);++checks;}
static void InputChecks()
{
    using namespace Input;
    System in;Scope scope;std::vector<Connection> connections;
    int press=0,hold=0,release=0;float axis=0;
    in.Bind({"Walk",Context::Game,{Keys::W}});
    connections.push_back(in.Subscribe(scope,Context::Game,"Walk",Phase::Pressed,[&](auto&){++press;}));
    connections.push_back(in.Subscribe(scope,Context::Game,"Walk",Phase::Held,[&](auto& e){++hold;Check(e.deltaTime==0.1f,"held dt");}));
    connections.push_back(in.Subscribe(scope,Context::Game,"Walk",Phase::Released,[&](auto&){++release;}));
    in.SetKey(Keys::W,true);in.SetKey(Keys::W,true);in.Tick(.1f);in.Tick(.1f);
    Check(press==1&&hold==2&&release==0,"press, hold, OS repeat");
    in.SetKey(Keys::W,false);in.Tick(.1f);Check(release==1,"release");
    in.SetKey(Keys::W,true);in.SetKey(Keys::W,false);in.Tick(.1f);Check(press==2&&release==2&&hold==2,"short tap between frames");
    in.Bind({"Horizontal",Context::Game,{Keys::D},1});in.Bind({"Horizontal",Context::Game,{Keys::A},-1});
    connections.push_back(in.Subscribe(scope,Context::Game,"Horizontal",Phase::Axis,[&](auto& e){axis=e.value;}));
    in.SetKey(Keys::D,true);in.Tick(.1f);Check(axis==1,"positive axis");in.SetKey(Keys::A,true);in.Tick(.1f);Check(axis==0,"opposing keys cancel");
    in.SetKey(Keys::D,false);in.Tick(.1f);Check(axis==-1,"negative axis");in.ReleaseAll();Check(axis==0,"cancel zero axis");
    int chord=0,plain=0;
    in.Bind({"Plain",Context::Game,{Keys::K}});in.Bind({"Chord",Context::Game,{Keys::Control,Keys::Shift,Keys::Alt,Keys::K,Keys::J}});
    connections.push_back(in.Subscribe(scope,Context::Game,"Plain",Phase::Pressed,[&](auto&){++plain;}));
    connections.push_back(in.Subscribe(scope,Context::Game,"Chord",Phase::Pressed,[&](auto&){++chord;}));
    in.SetKey(Keys::LeftControl,true);in.SetKey(Keys::RightShift,true);in.SetKey(Keys::LeftAlt,true);in.SetKey(Keys::J,true);in.SetKey(Keys::K,true);in.Tick(.1f);
    Check(chord==1&&plain==0,"specific chord consumes ordinary key, modifier aliases");
    in.ReleaseAll();in.SetKey(Keys::K,true);in.Tick(.1f);Check(plain==1,"plain key remains available");in.ReleaseAll();
    int ui=0,editor=0;in.Bind({"UI",Context::UI,{Keys::K}});in.Bind({"Editor",Context::Editor,{Keys::K}});
    connections.push_back(in.Subscribe(scope,Context::UI,"UI",Phase::Pressed,[&](auto&){++ui;}));
    connections.push_back(in.Subscribe(scope,Context::Editor,"Editor",Phase::Pressed,[&](auto&){++editor;}));
    in.SetContextEnabled(Context::Editor,true);in.SetContextEnabled(Context::UI,true);in.SetKey(Keys::K,true);in.Tick(.1f);
    Check(ui==1&&editor==0&&plain==1,"UI > editor > game");
    in.SetContextEnabled(Context::UI,false);in.Tick(.1f);Check(editor==0,"held key suppressed at context switch");
    in.SetKey(Keys::K,false);in.SetKey(Keys::K,true);in.Tick(.1f);Check(editor==1,"fresh press after context switch");
    in.ReleaseAll();in.ClearBindings(Context::UI);in.Bind({"UI",Context::UI,{Keys::K},1,0,false});in.SetContextEnabled(Context::UI,true);
    in.SetKey(Keys::K,true);in.Tick(.1f);Check(ui==2&&editor==2,"non-consuming binding propagates");
    in.ReleaseAll();in.SetContextPriority(Context::Game,300);in.SetKey(Keys::K,true);in.Tick(.1f);Check(plain==2&&ui==2,"context priority override");
    in.Unbind(Context::Game,"Plain");in.Bind({"Plain",Context::Game,{Keys::L}});in.Tick(.1f);Check(plain==2,"rebind suppresses held key");
    in.SetKey(Keys::L,true);in.Tick(.1f);Check(plain==3,"same handler after rebinding");
    scope.SetEnabled(false);in.ReleaseAll();in.SetKey(Keys::L,true);in.Tick(.1f);Check(plain==3,"disabled owner");scope.SetEnabled(true);in.ReleaseAll();
    Connection survivor;{Scope owner;survivor=in.Subscribe(owner,Context::Game,"Plain",Phase::Pressed,[&](auto&){throw std::runtime_error("dead owner");});}
    in.SetKey(Keys::L,true);in.Tick(.1f);Check(plain==4,"destroyed owner never called");in.ReleaseAll();
    auto victim=std::make_unique<Scope>();Connection killed;
    auto killer=in.Subscribe(scope,Context::Game,"Plain",Phase::Pressed,[&](auto&){victim.reset();killed.Disconnect();});
    killed=in.Subscribe(*victim,Context::Game,"Plain",Phase::Pressed,[&](auto&){throw std::runtime_error("deleted during dispatch");});
    in.SetKey(Keys::L,true);in.Tick(.1f);Check(!victim,"destroy another subscriber during dispatch");
    in.ReleaseAll();float x=0,y=0;
    in.Bind({"X",Context::Game,{Keys::RightButton},1,0,true,Source::MouseX});in.Bind({"Y",Context::Game,{Keys::RightButton},1,0,true,Source::MouseY});
    connections.push_back(in.Subscribe(scope,Context::Game,"X",Phase::Axis,[&](auto& e){x=e.value;}));connections.push_back(in.Subscribe(scope,Context::Game,"Y",Phase::Axis,[&](auto& e){y=e.value;}));
    in.SetKey(Keys::RightButton,true);in.AddPointer(2,3);in.AddPointer(4,5);in.Tick(.1f);Check(x==6&&y==8,"both mouse axes accumulate with same guard");
    in.Tick(.1f);Check(x==0&&y==0,"pointer deltas cleared each frame");
}
static ComponentId Add(Scene& scene,const char* type,const char* name)
{
    auto obj=SceneProject::GetComponentTypes().Find(type)->create();auto r=scene.AddComponent(name,obj,type);Check(r.result==SceneResult::Ok,"add standard type");return r.id;
}
static void CharacterChecks()
{
    {
        Game example;InputShortcutExample plain(example.GetInput());
        std::unique_ptr<GameComponent> component=std::make_unique<InputComponentExample>();auto* subscriber=static_cast<InputComponentExample*>(component.get());
        example.GetScene().AddComponent("Subscriber",component);
        std::unique_ptr<GameComponent> character=std::make_unique<InputCharacterExample>();auto* hero=static_cast<InputCharacterExample*>(character.get());
        example.GetScene().AddComponent("Hero",character);example.InitializeGameplay();
        example.GetInput().SetKey(Keys::Control,true);example.GetInput().SetKey(Keys::Shift,true);example.GetInput().SetKey(Keys::M,true);
        example.GetInput().SetKey(Keys::U,true);example.GetInput().SetKey(Keys::L,true);example.StepProjectSimulation(.1f);
        Check(plain.activations==1&&subscriber->activations==1&&hero->GetLocalPosition().x>0,"compiled ordinary class, component and character examples");
    }
    Game game;auto& scene=game.GetScene();auto id=Add(scene,"Gameplay.Character","Hero");auto settingsId=Add(scene,"Gameplay.PlaySettings","Play");
    auto* hero=dynamic_cast<Character*>(scene.FindComponent(id));hero->SetMoveSpeed(3);
    const Trace::Ray centerRay{{0,0,5},{0,0,-1},10};
    auto shapeHit=scene.Raycast(centerRay);
    Check(bool(shapeHit)&&shapeHit.hit->componentId==id&&std::abs(shapeHit.hit->distance-4.7f)<.001f,"Character has assigned cylinder geometry");
    auto* settings=dynamic_cast<PlaySettings*>(scene.FindComponent(settingsId));settings->character=id;
    auto json=SceneDocument::Serialize(SceneTestAccess::Snapshot(scene));Scene restored;SceneDocument::Restore(SceneDocument::Parse(json),SceneProject::GetComponentTypes(),restored);
    auto* restoredHero=dynamic_cast<Character*>(restored.FindComponentByName("Hero"));auto* restoredSettings=dynamic_cast<PlaySettings*>(restored.FindComponentByName("Play"));
    Check(restoredHero&&restoredHero->GetMoveSpeed()==3&&restoredSettings->character==restoredHero->GetComponentId(),"no-code save/load settings and references");
    shapeHit=restored.Raycast(centerRay);
    Check(bool(shapeHit)&&shapeHit.hit->componentId==restoredHero->GetComponentId()&&std::abs(shapeHit.hit->distance-4.7f)<.001f,"Character geometry reconstructed after scene load");
    game.InitializeGameplay();auto* control=game.GetCharacterController();Check(control&&control->GetCharacter()==hero,"explicit possession");
    game.GetInput().SetKey(Keys::W,true);game.StepProjectSimulation(1);Check(std::abs(hero->GetLocalPosition().z+3)<.001,"standard movement");
    auto second=Add(scene,"Gameplay.Character","Second");control->Possess(second);game.StepProjectSimulation(1);auto* other=dynamic_cast<Character*>(scene.FindComponent(second));Check(glm::length(other->GetLocalPosition())==0,"switch suppresses held movement");
    game.GetInput().SetKey(Keys::W,false);game.GetInput().SetKey(Keys::W,true);game.StepProjectSimulation(1);Check(glm::length(other->GetLocalPosition())>4.9f,"new character receives fresh input");
    control->SetEnabled(false);auto before=other->GetLocalPosition();game.StepProjectSimulation(1);Check(other->GetLocalPosition()==before,"disable controller");
    control->SetEnabled(true);scene.RemoveComponent(second);game.StepProjectSimulation(1);Check(!control->GetCharacter(),"deleted possessed object is safe");
    scene.RemoveComponent(id);Check(settings->character==0,"deletion clears saved assignment");
    Game ai;auto npc=Add(ai.GetScene(),"Gameplay.Character","NPC");ai.InitializeGameplay();Check(!ai.GetCharacterController(),"character without player control");
    auto* c=dynamic_cast<Character*>(ai.GetScene().FindComponent(npc));c->Move({1,0,1},1);Check(std::abs(glm::length(c->GetLocalPosition())-5)<.001,"normalized diagonal movement");
    Player camera;SceneCamera fixed;fixed.SetPosition({3,4,5});fixed.Apply(camera);Check(camera.GetPosition()==glm::vec3(3,4,5),"explicit scene camera");
}
static void PlayerPropertyChecks()
{
    Game game;auto id=Add(game.GetScene(),"Gameplay.Player","Player");
    auto* hero=dynamic_cast<Character*>(game.GetScene().FindComponent(id));
    Check(hero&&hero->playerControlled,"placed Player enables possession without PlaySettings");
    hero->cameraSettings.orthographic=true;hero->cameraSettings.viewPlane=2;hero->cameraSettings.orthoHeight=23;hero->cameraSettings.fov=80;
    const auto saved=hero->SaveProjectData();Character copy;copy.LoadProjectData(saved);
    Check(copy.playerControlled&&copy.cameraSettings.orthographic&&copy.cameraSettings.orthoHeight==23,"player camera settings round trip");
    auto invalid=nlohmann::json::parse(saved);invalid["nearClip"]=100;invalid["farClip"]=1;invalid["moveSpeed"]=12;
    try{copy.LoadProjectData(invalid.dump());Check(false,"invalid clip range rejected");}catch(const std::invalid_argument&){}
    Check(copy.SaveProjectData()==saved,"invalid player settings do not partially apply");
    game.InitializeGameplay();Check(game.GetCharacterController()->GetCharacter()==hero,"Player auto possession");
    Player camera;game.GetCharacterController()->UpdateCamera(camera);Check(camera.GetForward()==glm::vec3(0,-1,0),"XZ camera looks exactly down");
    for(int plane=1;plane<=3;++plane){
        hero->cameraSettings.viewPlane=plane;game.GetCharacterController()->UpdateCamera(camera);
        Check(camera.GetForward()==hero->cameraSettings.Forward(),"exact 2D viewing plane");
        const auto before=hero->GetLocalPosition();game.GetInput().SetKey(Keys::W,true);game.StepProjectSimulation(.1f);game.GetInput().SetKey(Keys::W,false);
        Check(glm::length((hero->GetLocalPosition()-before)-hero->cameraSettings.Up()*.5f)<.001f,"movement follows 2D plane");
    }
    const ComponentType linkTypes[]={
        {"Test.Player","Player",EngineComponentDetail::Create<PlayerCharacter>},
        {"Test.Camera","Custom camera",EngineComponentDetail::Create<SceneCamera>},
        {"Test.Other","Other",EngineComponentDetail::Create<GameComponent>}};
    SceneEditSession links({linkTypes,3});auto linkApi=links.GetApi();auto author=links.GetAuthoringApi();uint64_t playerId=0,cameraId=0,otherId=0;char error[1024]{};
    Check(linkApi.scene.createObject(linkApi.scene.context,"Test.Player","Player",nullptr,&playerId)==EngineSceneOk,"create property player");
    Check(linkApi.scene.createObject(linkApi.scene.context,"Test.Camera","Camera",nullptr,&cameraId)==EngineSceneOk,"create custom camera type");
    Check(linkApi.scene.createObject(linkApi.scene.context,"Test.Other","Other",nullptr,&otherId)==EngineSceneOk,"create non-camera");
    Check(author.setProperty(author.context,playerId,"/camera",std::to_string(cameraId).c_str(),error,sizeof(error))==EngineSceneOk,"camera reference supports custom types");
    Check(author.setProperty(author.context,playerId,"/camera",std::to_string(otherId).c_str(),error,sizeof(error))!=EngineSceneOk,"camera reference rejects unrelated objects");
    Game duplicate;Add(duplicate.GetScene(),"Gameplay.Player","One");Add(duplicate.GetScene(),"Gameplay.Player","Two");
    try{duplicate.InitializeGameplay();Check(false,"multiple players rejected");}catch(const std::runtime_error&){}
    Game legacy;auto legacyId=Add(legacy.GetScene(),"Gameplay.Character","Old hero");auto settingsId=Add(legacy.GetScene(),"Gameplay.PlaySettings","Old settings");
    auto camId=Add(legacy.GetScene(),"Gameplay.Camera","Camera");auto* settings=dynamic_cast<PlaySettings*>(legacy.GetScene().FindComponent(settingsId));
    settings->character=legacyId;settings->camera=camId;settings->cameraDistance=17;settings->standardControl=false;
    auto snapshot=SceneTestAccess::Snapshot(legacy.GetScene());
    for(auto& row:snapshot.components)if(row.id==legacyId)row.projectData=R"({"version":1,"moveSpeed":9})";
    Scene migrated;SceneDocument::Restore(snapshot,SceneProject::GetComponentTypes(),migrated);
    auto* migratedHero=dynamic_cast<Character*>(migrated.FindComponentByName("Old hero"));
    Check(migratedHero->playerControlled&&!migratedHero->standardControl&&migratedHero->cameraDistance==17&&migratedHero->GetMoveSpeed()==9,"legacy Play settings migrate into player properties");
    Check(migratedHero->camera==migrated.FindComponentByName("Camera")->GetComponentId(),"migrated camera reference remapped");
    migratedHero->cameraDistance=22;Scene again;SceneDocument::Restore(SceneTestAccess::Snapshot(migrated),SceneProject::GetComponentTypes(),again);
    Check(dynamic_cast<Character*>(again.FindComponentByName("Old hero"))->cameraDistance==22,"legacy record cannot overwrite modern settings");
    migrated.RemoveComponent(migratedHero->camera);Check(migratedHero->camera==0,"deleting camera clears player reference");
}
static void SessionChecks()
{
    SceneEditSession session(SceneProject::GetComponentTypes());auto api=session.GetApi();auto input=session.GetViewportInputApi();auto data=session.GetProjectDataApi();
    EngineSceneTransform t{{0,0,0},{0,0,0,1},{1,1,1}};uint64_t hero=0,settings=0;
    Check(api.scene.createObject(api.scene.context,"Gameplay.Character","Hero",&t,&hero)==EngineSceneOk,"edit create character");
    Check(api.scene.createObject(api.scene.context,"Gameplay.PlaySettings","Play",&t,&settings)==EngineSceneOk,"edit create settings");
    Character config;config.playerControlled=true;Check(data.write(data.context,hero,config.SaveProjectData().c_str())==EngineSceneOk,"edit assign Player in properties");
    auto z=[&](bool active){float value=999;EngineSceneInfo info{};auto visitor=[](void* p,const EngineSceneObjectInfo* o){if(std::string(o->name)=="Hero")*static_cast<float*>(p)=o->transform.position[2];};
        if(active)api.readActiveScene(api.context,&info,visitor,&value);else api.scene.readScene(api.scene.context,&info,visitor,&value);return value;};
    Check(api.control(api.context,EnginePlay)==EngineSceneOk,"Play");input.key(input.context,87,1);api.frame(api.context,.1);Check(z(true)==0,"unfocused Play ignores keys");
    input.focus(input.context,1);input.key(input.context,87,1);api.frame(api.context,.1);Check(z(true)<-.49f&&z(false)==0,"Play copy moves, edit does not");
    input.focus(input.context,0);float stopped=z(true);api.frame(api.context,.1);Check(z(true)==stopped,"focus loss cancels movement");
    input.focus(input.context,1);api.frame(api.context,.1);Check(z(true)==stopped,"focus regain has no stale input");
    input.key(input.context,87,1);api.frame(api.context,.1);stopped=z(true);
    Check(api.control(api.context,EnginePause)==EngineSceneOk,"Pause");input.key(input.context,87,1);api.frame(api.context,.1);Check(z(true)==stopped,"pause ignores movement");
    Check(api.control(api.context,EngineResume)==EngineSceneOk,"Resume");api.frame(api.context,.1);Check(z(true)==stopped,"resume no stuck key");
    Check(api.control(api.context,EngineStop)==EngineSceneOk&&z(true)==0,"Stop restores edit scene");
    Check(api.control(api.context,EnginePlay)==EngineSceneOk,"repeat Play");api.frame(api.context,.1);Check(z(true)==0,"repeat Play starts neutral");
    api.control(api.context,EngineStop);
}
static void CameraChecks()
{
    {
        HWND clockWindow=CreateWindowExW(0,L"STATIC",L"Material clock test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        struct Guard { HWND h; ~Guard(){DestroyWindow(h);} } clockGuard{clockWindow};
        static Game* observed=nullptr;
        SceneEditSession session(SceneProject::GetComponentTypes(),[]()->std::unique_ptr<Game>{
            auto game=std::make_unique<Game>(); observed=game.get(); return game;
        });
        auto api=session.GetApi();
        Check(clockWindow && api.attachViewport(api.context,clockWindow,320,240)==EngineSceneOk,"material clock viewport");
        auto editorCamera=session.GetEditorCameraApi();EngineEditorCameraState authored{8,4,12,-.8f,.2f};
        Check(editorCamera.write(editorCamera.context,&authored)==EngineSceneOk,"set editor camera before empty Play");
        Check(api.control(api.context,EnginePlay)==EngineSceneOk && observed,"material clock Play");
        const auto emptyView=observed->GetPlayer()->GetViewMatrix();
        Check(!observed->GetCharacterController() && observed->GetPlayer()->GetPosition()==glm::vec3(8,4,12),"without Player retain editor camera");
        Check(api.frame(api.context,.05)==EngineSceneOk && api.frame(api.context,.05)==EngineSceneOk,"material clock frames");
        Check(memcmp(&emptyView,&observed->GetPlayer()->GetViewMatrix(),sizeof(emptyView))==0,"empty Play does not move editor view");
        Check(std::abs(observed->GetFrameConstants().TimeParams.x-.1f)<.0001f,"material real time advances in editor Play");
        Check(api.control(api.context,EngineStop)==EngineSceneOk,"material clock Stop");
    }
    // A real shared D3D viewport verifies separate Players and Stop camera restoration.
    HWND window=CreateWindowExW(0,L"STATIC",L"Input camera test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(window!=nullptr,"camera test HWND");
    struct WindowGuard{HWND handle;~WindowGuard(){DestroyWindow(handle);}} guard{window};
    Game edit;Check(edit.InitializeViewport(window,320,240),"editor camera viewport");edit.SetSimulationEnabled(false);
    edit.GetPlayer()->SetPosition({8,3,6});edit.GetPlayer()->SetRotate(-.8f,.1f);edit.GetPlayer()->MoveEditorCamera(0,0,0,0,0,0);
    auto position=edit.GetPlayer()->GetPosition();auto view=edit.GetPlayer()->GetViewMatrix();
    edit.SuspendViewportTargets();
    {
        Game play;Check(play.InitializePlayViewport(edit),"separate Play camera viewport");
        auto hero=Add(play.GetScene(),"Gameplay.Character","Hero");auto id=Add(play.GetScene(),"Gameplay.PlaySettings","Settings");
        dynamic_cast<PlaySettings*>(play.GetScene().FindComponent(id))->character=hero;
        play.InitializeGameplay();play.GetInput().SetKey(Keys::W,true);play.StepProjectSimulation(.1f);
        Check(play.GetPlayer()!=edit.GetPlayer()&&play.GetPlayer()->GetPosition()!=position,"Play camera moves independently");
        Check(edit.GetPlayer()->GetPosition()==position&&memcmp(&view,&edit.GetPlayer()->GetViewMatrix(),sizeof(view))==0,"editor camera remains unchanged in Play");
        auto cameraId=Add(play.GetScene(),"Gameplay.Camera","Fixed");auto* camera=dynamic_cast<SceneCamera*>(play.GetScene().FindComponent(cameraId));camera->SetPosition({0,15,0});
        play.GetCharacterController()->Possess(hero,cameraId);play.StepProjectSimulation(.1f);Check(play.GetPlayer()->GetPosition()==glm::vec3(0,15,0),"explicit camera selected in Play");
    }
    Check(edit.RestoreViewportTargets()&&edit.RenderViewportFrame(0),"Stop restores editor D3D targets");
    Check(edit.GetPlayer()->GetPosition()==position,"Stop restores editor camera");
}
static void SdkChecks(const std::filesystem::path& dll,const std::filesystem::path& content)
{
    GameEditModule module(dll,content);auto api=module.Api();auto data=module.ProjectDataApi();auto input=module.ViewportInputApi();
    Check(input.key&&input.objectKind,"external SDK exports optional input extension");
    uint64_t hero=0,settings=0;EngineSceneTransform transform{{0,0,0},{0,0,0,1},{1,1,1}};
    Check(api.scene.createObject(api.scene.context,"SdkHero","Hero",&transform,&hero)==EngineSceneOk,"SDK generated Character descendant in catalog");
    uint32_t kind=0;input.objectKind(input.context,hero,&kind);Check(kind==1,"SDK descendant recognized as Character");
    Check(api.scene.createObject(api.scene.context,"Gameplay.PlaySettings","Settings",&transform,&settings)==EngineSceneOk,"SDK includes no-code standard types");
    Character config;config.playerControlled=true;data.write(data.context,hero,config.SaveProjectData().c_str());
    input.focus(input.context,1);Check(api.control(api.context,EnginePlay)==EngineSceneOk,"SDK custom Game controller starts");
    auto z=[&]{float v=0;EngineSceneInfo info{};api.readActiveScene(api.context,&info,[](void* p,const EngineSceneObjectInfo* o){if(std::string(o->name)=="Hero")*static_cast<float*>(p)=o->transform.position[2];},&v);return v;};
    input.key(input.context,87,1);api.frame(api.context,.1);Check(z()==0,"custom controller replaces WASD");input.key(input.context,87,0);
    input.key(input.context,73,1);api.frame(api.context,.1);Check(z()<-.49f,"custom SDK controller moves with I");
    api.control(api.context,EngineStop);
}
int wmain(int argc,wchar_t** argv){try{InputChecks();CharacterChecks();PlayerPropertyChecks();SessionChecks();if(argc==3)SdkChecks(std::filesystem::absolute(argv[1]),std::filesystem::absolute(argv[2]));if(argc==2&&std::wstring(argv[1])==L"--camera")CameraChecks();std::cout<<"Input/Character: "<<checks<<" checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
