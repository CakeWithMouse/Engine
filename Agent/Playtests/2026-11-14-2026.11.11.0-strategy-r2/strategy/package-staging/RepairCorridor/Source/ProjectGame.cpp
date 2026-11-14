#include "ProjectGame.h"
#include <Engine/AI/Tasks.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
Ai::Key<Ai::ObjectRef> Target{"Target"};
Ai::MachinePtr BuildCorridorGuardMachine() {
    Ai::MachineBuilder machine("CorridorGuard");
    auto& patrol=machine.State("Patrol");
    patrol.Task<Ai::Tasks::FollowRoute>(0.35f).GoTo("Pause").OnSucceeded();
    patrol.GoTo("Chase").If(Ai::Conditions::CanSee(Target));
    auto& pause=machine.State("Pause");
    pause.Task<Ai::Tasks::Wait>(0.65f).GoTo("Patrol").OnSucceeded();
    pause.GoTo("Chase").If(Ai::Conditions::CanSee(Target));
    auto& chase=machine.State("Chase");
    chase.Task<Ai::Tasks::MoveTo>(Target,1.0f).GoTo("Patrol").OnFailed();
    chase.GoTo("Patrol").If(Ai::Not(Ai::Conditions::CanSee(Target)));
    return machine.Build();
}
ENGINE_AI_MACHINE("CorridorGuard",BuildCorridorGuardMachine)
}

void CorridorSoldier::BeginPlay() {
    Character::BeginPlay();
    Ai::Get(*GetGame()).AddTarget(*this,Ai::Team{1});
}

void CorridorGuard::SetupAi(Ai::Controller& ai) {
    AiCharacter::SetupAi(ai);
    const glm::vec3 home=GetWorldPosition();
    ai.SetRoute({home+glm::vec3(2.6f,0,0),home+glm::vec3(-2.6f,0,0)});
    ai.GetPerception().SetTeam(Ai::Team{2});
    ai.GetPerception().EnableSight(12.0f,360.0f,3.0f);
    ai.GetPerception().WriteTargetTo(Target);
    ai.Run(Ai::GetMachine("CorridorGuard"));
}

void ProjectGame::AfterInitialize() { Game::AfterInitialize(); EnsureCorridor(); }

void ProjectGame::EnsureCorridor() {
    if(built_)return;
    SetSimulationTimeScale(1.0f);
    GetSettings().Physics().SetGravity(glm::vec3(0));
    GetSettings().SetRenderingType(Forward);
    SetDirectionalLight({-0.45f,-1.0f,-0.35f},{1.0f,0.96f,0.9f},1.2f);
    SetShadowSettings(false,120.0f);
    BuildCorridor();
    built_=true;
    ApplyTacticalCamera();
}

void ProjectGame::ApplyTacticalCamera() {
    auto* camera=GetPlayer(); if(!camera)return;
    camera->SetCameraMode(CameraMode::Free);
    camera->SetProjection(true,60.0f,0.1f,100.0f,22.0f);
    camera->SetPosition({0.0f,20.0f,4.0f});
    camera->SetViewDirection({0.0f,-1.0f,0.0f},{0.0f,0.0f,1.0f});
    camera->StopRotate(true,true);
    camera->EnableWASD(false);
    camera->RefreshMatrices();
}

void ProjectGame::AddBox(const std::string& name,glm::vec3 pos,glm::vec3 scale,glm::vec4 color,PhysicsBodyType type,float restitution,uint32_t traceChannels) {
    auto cube=std::make_unique<CubeComponent>(pos,glm::vec3(0),scale,color);
    cube->SetPhysicsEnabled(true); cube->SetPhysicsBodyType(type); cube->SetPhysicsShapeType(PhysicsShapeType::Box);
    cube->SetPhysicsFriction(0.8f); cube->SetPhysicsRestitution(restitution);
    cube->traceChannels=traceChannels;
    auto* raw=cube.get();
    if(RegisterComponent(name,raw)!=RegisterResult::Ok)throw std::runtime_error("Cannot add "+name);
    cube.release();
}

void ProjectGame::BuildCorridor() {
    auto light=std::make_unique<PointLightComponent>(glm::vec3(0,6,1),glm::vec3(1.0f,0.88f,0.72f),1.25f,32.0f);
    light->SetCastShadows(false);
    auto* lightRaw=light.get();
    if(RegisterComponent("HallLight",lightRaw)!=RegisterResult::Ok)throw std::runtime_error("Cannot add HallLight");
    light.release();
    constexpr uint32_t FloorTraceChannel=1u<<1;
    AddBox("Floor",{0,-0.55f,0},{7,0.5f,22},{0.26f,0.30f,0.27f,1},PhysicsBodyType::Static,0.15f,FloorTraceChannel);
    AddBox("WallLeft",{-3.4f,1,0},{0.35f,2.5f,22},{0.42f,0.46f,0.43f,1},PhysicsBodyType::Static);
    AddBox("WallRight",{3.4f,1,0},{0.35f,2.5f,22},{0.42f,0.46f,0.43f,1},PhysicsBodyType::Static);
    AddBox("CrateStop",{-2.9f,0.45f,2},{0.4f,2.0f,1.8f},{0.48f,0.52f,0.54f,1},PhysicsBodyType::Static,0.0f);
    AddBox("ExitMarker",{0,0.05f,9.5f},{2,0.1f,0.2f},{0.2f,0.9f,0.45f,1},PhysicsBodyType::Static);
    auto crate=std::make_unique<CubeComponent>(glm::vec3(0,0.7f,2),glm::vec3(0),glm::vec3(1.8f,1.4f,1.4f),glm::vec4(0.95f,0.52f,0.14f,1));
    crate->SetPhysicsEnabled(true); crate->SetPhysicsBodyType(PhysicsBodyType::Dynamic); crate->SetPhysicsShapeType(PhysicsShapeType::Box);
    crate->SetPhysicsDensity(12.0f); crate->SetPhysicsFriction(0.8f); crate->SetPhysicsRestitution(0.0f);
    crate_=crate.get();
    if(RegisterComponent("SupplyCrate",crate_)!=RegisterResult::Ok)throw std::runtime_error("Cannot add crate");
    crate.release();
    const float offsets[3]={-1.5f,0,1.5f}; const glm::vec4 blue(0.22f,0.55f,0.98f,1);
    for(int i=0;i<3;++i) {
        auto unit=std::make_unique<CorridorSoldier>(); unit->SetPosition({offsets[i],0.65f,-6}); unit->SetMoveSpeed(3.0f);
        auto* body=unit.get();
        if(RegisterComponent("Squad"+std::to_string(i+1),body)!=RegisterResult::Ok)throw std::runtime_error("Cannot add squad unit");
        unit.release();
        auto visual=std::make_unique<CubeComponent>(glm::vec3(offsets[i],0.65f,-6),glm::vec3(0),glm::vec3(0.62f,1.1f,0.45f),blue);
        auto* mesh=visual.get();
        if(RegisterComponent("SquadVisual"+std::to_string(i+1),mesh)!=RegisterResult::Ok)throw std::runtime_error("Cannot add soldier visual");
        visual.release();
        auto marker=std::make_unique<CubeComponent>(glm::vec3(offsets[i],-0.35f,-6),glm::vec3(0),glm::vec3(0.42f,0.08f,0.42f),glm::vec4(0.15f,0.92f,0.78f,1));
        auto* markerRaw=marker.get();
        if(RegisterComponent("OrderMarker"+std::to_string(i+1),markerRaw)!=RegisterResult::Ok)throw std::runtime_error("Cannot add order marker");
        marker.release();
        squad_.push_back({body,mesh,markerRaw});
    }
    auto guard=std::make_unique<CorridorGuard>(); guard->SetPosition({0,0.65f,6}); guard->team=2; guard->SetMoveSpeed(1.6f); guard->stateMachine="CorridorGuard";
    auto* guardBody=guard.get();
    if(RegisterComponent("CorridorGuard",guardBody)!=RegisterResult::Ok)throw std::runtime_error("Cannot add guard");
    guard.release();
    auto guardVisual=std::make_unique<CubeComponent>(glm::vec3(0,0.65f,6),glm::vec3(0),glm::vec3(0.8f,1.2f,0.55f),glm::vec4(0.94f,0.22f,0.18f,1));
    auto* guardMesh=guardVisual.get();
    if(RegisterComponent("GuardVisual",guardMesh)!=RegisterResult::Ok)throw std::runtime_error("Cannot add guard visual");
    guardVisual.release(); guards_.push_back({guardBody,guardMesh});
    auto ui=std::make_unique<UIComponent>(); FontAtlasSettings font; font.Face=L"Consolas"; font.PixelHeight=14; ui->SetFont(font);
    UIRect panel; panel.X=10; panel.Y=10; panel.Width=230; panel.Height=160;
    auto* interactivePanel=ui->AddPanel(panel,{0.03f,0.06f,0.09f,0.9f});
    interactivePanel->bInteractive=true;
    UIRect text; text.X=19; text.Y=17; text.Width=200; text.Height=20;
    auto* title=ui->AddLabel(text,"РЕМОНТНЫЙ КОРИДОР",{0.98f,0.84f,0.52f,1}); title->Name="title";
    text.Y=40; hudGoal_=ui->AddLabel(text,"Проход: ЗАКРЫТ",{0.92f,0.96f,1,1}); hudGoal_->Name="goal";
    text.Y=62; hudFsm_=ui->AddLabel(text,"FSM: Patrol",{1.0f,0.5f,0.42f,1}); hudFsm_->Name="fsm";
    text.Y=84; hudSelected_=ui->AddLabel(text,"Боец выбран: 1",{0.68f,0.84f,1,1}); hudSelected_->Name="selected";
    text.Y=106; hudControls_=ui->AddLabel(text,"1/2/3: выбор",{0.68f,0.84f,1,1}); hudControls_->Name="controls";
    text.Y=126; hudHelp_=ui->AddLabel(text,"ПКМ: приказ | WASD: ход",{0.68f,0.84f,1,1}); hudHelp_->Name="help";
    text.Y=146; hudOrder_=ui->AddLabel(text,"Цель: —",{0.15f,0.92f,0.78f,1}); hudOrder_->Name="order";
    hud_=ui.get(); const auto shader=EnginePaths::Shader("UI.hlsl").generic_u8string();
    if(RegisterComponent("TacticalHud",hud_,shader,shader)!=RegisterResult::Ok)throw std::runtime_error("Cannot add HUD");
    ui.release();
}

void ProjectGame::PreUpdate(float dt) {
    EnsureCorridor();
    Game::PreUpdate(dt);
    ApplyTacticalCamera();
    auto& input=GetInput();
    pointerBlocked_=input.IsPointerBlocked();
    const bool one=input.IsPhysicalKeyDown(Keys::D1),two=input.IsPhysicalKeyDown(Keys::D2),three=input.IsPhysicalKeyDown(Keys::D3),breach=input.IsPhysicalKeyDown(Keys::E);
    const bool command=input.IsPhysicalKeyDown(Keys::RightButton);
    if(one&&!keys_[0])selected_=0; if(two&&!keys_[1])selected_=1; if(three&&!keys_[2])selected_=2;
    if(breach&&!keys_[3]&&crate_)impulsePending_=true;
    if(impulsePending_&&crate_&&crate_->HasPhysicsBody()) {
        crate_->AddPhysicsImpulse({-150.0f,3.0f,0});
        impulsePending_=false;
    }
    keys_[0]=one; keys_[1]=two; keys_[2]=three; keys_[3]=breach;
    if(command&&!keys_[4]) {
        Trace::Query query;
        query.channels=1u<<1;
        query.geometry=Trace::Geometry::Bounds;
        auto result=RaycastCursor(query);
        lastTraceStatus_=Trace::StatusName(result.status);
        if(result&&result.hit) {
            auto& selected=squad_[selected_];
            const auto hit=result.hit->position;
            selected.orderTarget={hit.x,selected.unit->GetWorldPosition().y,hit.z};
            selected.hasOrder=true;
            selected.movingToOrder=true;
            selected.orderCompleted=false;
            selected.orderCancelled=false;
            selected.orderMarker->SetPosition({hit.x,-0.23f,hit.z});
        }
    }
    keys_[4]=command;
    glm::vec3 direction(0);
    if(input.IsPhysicalKeyDown(Keys::W)||input.IsPhysicalKeyDown(Keys::Up))direction.z=1;
    if(input.IsPhysicalKeyDown(Keys::S)||input.IsPhysicalKeyDown(Keys::Down))direction.z=-1;
    if(input.IsPhysicalKeyDown(Keys::A)||input.IsPhysicalKeyDown(Keys::Left))direction.x=-1;
    if(input.IsPhysicalKeyDown(Keys::D)||input.IsPhysicalKeyDown(Keys::Right))direction.x=1;
    for(size_t i=0;i<squad_.size();++i) {
        auto& item=squad_[i];
        const glm::vec3 before=item.unit->GetWorldPosition();
        if(i==static_cast<size_t>(selected_)&&glm::length(direction)>0.1f) {
            if(item.hasOrder){item.orderCancelled=true;item.orderCompleted=false;}
            item.movingToOrder=false;
            item.unit->Move(glm::normalize(direction),dt);
        } else if(item.movingToOrder) {
            glm::vec3 delta=item.orderTarget-before; delta.y=0;
            const float distance=glm::length(delta);
            if(distance<0.12f){item.movingToOrder=false;item.orderCompleted=true;}
            else item.unit->Move(delta/distance,dt);
        }
        const auto after=item.unit->GetWorldPosition();
        if(crate_&&crate_->GetWorldPosition().x>-1.55f&&before.z<1.15f&&after.z>=1.15f)item.unit->SetPosition(before);
        if(item.movingToOrder) {
            glm::vec3 remaining=item.orderTarget-item.unit->GetWorldPosition(); remaining.y=0;
            if(glm::length(remaining)<0.12f){item.movingToOrder=false;item.orderCompleted=true;}
        }
        item.mesh->SetPosition(item.unit->GetWorldPosition()+glm::vec3(0,0.1f,0));
    }
    for(auto& item:guards_)item.mesh->SetPosition(item.unit->GetWorldPosition()+glm::vec3(0,0.1f,0));
    if(!won_&&!lost_)for(auto& item:squad_) {
        if(glm::length(item.unit->GetWorldPosition()-guards_[0].unit->GetWorldPosition())<1.05f)lost_=true;
        if(crate_&&crate_->GetWorldPosition().x<=-1.55f&&item.unit->GetWorldPosition().z>9.0f)won_=true;
    }
    UpdateHud();
}

void ProjectGame::UpdateHud() {
    if(!hud_)return;
    const std::string status=won_?"ПОБЕДА":lost_?"ПОРАЖЕНИЕ":(crate_&&crate_->GetWorldPosition().x<=-1.55f?"Проход: ОТКРЫТ":"Проход: ЗАКРЫТ");
    if(hudGoal_)hudGoal_->SetText(status);
    const std::string state=!guards_.empty()&&guards_[0].unit->GetAi()?guards_[0].unit->GetAi()->ActiveState():"ожидает Play";
    if(hudFsm_)hudFsm_->SetText("Охранник FSM: "+state);
    if(hudSelected_)hudSelected_->SetText("Боец выбран: "+std::to_string(selected_+1));
    if(hudControls_)hudControls_->SetText("1/2/3 выбор | E толчок");
    if(hudHelp_)hudHelp_->SetText("ПКМ: приказ | WASD: ход");
    if(hudOrder_) {
        const auto& selected=squad_[selected_];
        if(selected.hasOrder) {
            const char* orderState=selected.movingToOrder?"...":selected.orderCancelled?"отменён":selected.orderCompleted?"готово":"ожидает";
            hudOrder_->SetText("Цель: "+std::to_string(static_cast<int>(std::round(selected.orderTarget.x)))+","+
                               std::to_string(static_cast<int>(std::round(selected.orderTarget.z)))+" "+orderState);
        } else hudOrder_->SetText("Цель: —");
    }
    if(const char* path=std::getenv("PAINENGINE_STRATEGY_TELEMETRY")) {
        if(++telemetryFrames_%30==0&&!squad_.empty()&&!guards_.empty()) {
            const auto p=squad_[selected_].unit->GetWorldPosition(); const auto c=crate_->GetWorldPosition(); const auto g=guards_[0].unit->GetWorldPosition();
            std::ofstream out(path,std::ios::app);
            if(out) out<<"selected="<<selected_+1<<" unit="<<p.x<<","<<p.y<<","<<p.z<<" crate="<<c.x<<","<<c.y<<","<<c.z
                <<" guard="<<g.x<<","<<g.y<<","<<g.z<<" state="<<state<<" won="<<won_<<" lost="<<lost_<<" paused=false"
                <<" pointerBlocked="<<pointerBlocked_<<" trace="<<lastTraceStatus_;
            if(out) for(size_t i=0;i<squad_.size();++i) {
                const auto unit=squad_[i].unit->GetWorldPosition(); const auto& order=squad_[i];
                out<<" squad"<<i+1<<"="<<unit.x<<","<<unit.z<<" target="<<(order.hasOrder?1:0)<<":"<<order.orderTarget.x<<","<<order.orderTarget.z<<" moving="<<order.movingToOrder<<" completed="<<order.orderCompleted<<" cancelled="<<order.orderCancelled;
            }
            if(out) out<<"\n";
        }
    }
}
void ProjectGame::TickComponents(float dt) { Game::TickComponents(dt); }
