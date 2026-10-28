#include <Engine/MainGame/CharacterController.h>
#include <Engine/Components/Character.h>
#include <cmath>

CharacterController::CharacterController(Game& game):game_(game){}
Character* CharacterController::GetCharacter()const{return dynamic_cast<Character*>(game_.GetScene().FindComponent(character_));}
void CharacterController::Possess(ComponentId character,ComponentId camera)
{
    if(character&&!dynamic_cast<Character*>(game_.GetScene().FindComponent(character)))throw std::invalid_argument("Possess requires Character");
    if(camera&&!dynamic_cast<SceneCamera*>(game_.GetScene().FindComponent(camera)))throw std::invalid_argument("Play camera requires SceneCamera");
    game_.GetInput().Reset();right_=forward_=0;character_=character;camera_=camera;
    if(auto* c=GetCharacter()){auto f=c->GetLocalRotationQuat()*glm::vec3(0,0,-1);yaw_=std::atan2(f.z,f.x);pitch_=0;}
}
void CharacterController::Configure(const PlaySettings& s){distance_=s.cameraDistance;height_=s.cameraHeight;sensitivity_=s.lookSensitivity;Possess(s.character,s.camera);}
void CharacterController::SetEnabled(bool v){enabled_=v;inputScope_.SetEnabled(v);right_=forward_=0;game_.GetInput().Reset();}
void CharacterController::BindInput()
{
    auto& input=game_.GetInput();connections_.clear();
    using namespace Input;
    for (const auto* action : {"Move.Forward","Move.Right","Look.Yaw","Look.Pitch"}) input.Unbind(Context::Game,action);
    input.Bind({"Move.Forward",Context::Game,{Keys::W},1});input.Bind({"Move.Forward",Context::Game,{Keys::S},-1});
    input.Bind({"Move.Right",Context::Game,{Keys::D},1});input.Bind({"Move.Right",Context::Game,{Keys::A},-1});
    input.Bind({"Look.Yaw",Context::Game,{Keys::RightButton},1,0,true,Source::MouseX});
    input.Bind({"Look.Pitch",Context::Game,{Keys::RightButton},1,0,true,Source::MouseY});
    connections_.push_back(input.Subscribe(inputScope_,Context::Game,"Move.Forward",Phase::Axis,[this](auto& e){forward_=e.value;}));
    connections_.push_back(input.Subscribe(inputScope_,Context::Game,"Move.Right",Phase::Axis,[this](auto& e){right_=e.value;}));
    connections_.push_back(input.Subscribe(inputScope_,Context::Game,"Look.Yaw",Phase::Axis,[this](auto& e){yaw_-=e.value*sensitivity_;}));
    connections_.push_back(input.Subscribe(inputScope_,Context::Game,"Look.Pitch",Phase::Axis,[this](auto& e){pitch_=(std::max)(-1.5f,(std::min)(1.5f,pitch_-e.value*sensitivity_));}));
}
void CharacterController::Tick(float seconds)
{
    if(enabled_&&game_.GetInput().IsContextEnabled(Input::Context::Game))
        if(auto* c=GetCharacter())c->Move(glm::vec3(std::cos(yaw_),0,std::sin(yaw_))*forward_+glm::vec3(std::sin(yaw_),0,-std::cos(yaw_))*right_,seconds);
    right_=forward_=0;
}
void CharacterController::UpdateCamera(Player& camera)
{
    if(auto* fixed=dynamic_cast<SceneCamera*>(game_.GetScene().FindComponent(camera_))){fixed->Apply(camera);return;}
    if(camera_)return; // deleted explicit camera: retain last view, never silently choose another
    if(auto* c=GetCharacter()){
        camera.SetCameraMode(CameraMode::Free);camera.SetRotate(yaw_,pitch_);
        glm::vec3 forward(std::cos(yaw_)*std::cos(pitch_),std::sin(pitch_),std::sin(yaw_)*std::cos(pitch_));
        camera.SetPosition(c->GetWorldPosition()+glm::vec3(0,height_,0)-forward*distance_);camera.MoveEditorCamera(0,0,0,0,0,0);
    }
}
