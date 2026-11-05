#include <Engine/SaveGame/SaveGame.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/MainGame/CharacterController.h>
#include <Engine/Components/Character.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <fstream>
#include <atomic>
#include <set>

namespace {
using Json=SaveArchive::Json;
constexpr size_t MaxBytes=16*1024*1024;
void Identifier(const std::string& value) {
    if(value.empty() || value.size()>64) throw std::runtime_error("Save identifier must be 1..64 ASCII letters, digits, '-' or '_'");
    for(unsigned char c:value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))
        throw std::runtime_error("Invalid save identifier");
}
Json Parse(const std::string& bytes) {
    if(bytes.size()>MaxBytes)throw std::runtime_error("Save exceeds 16 MiB");
    std::map<int,std::set<std::string>> keys;
    auto data=Json::parse(bytes,[&](int depth,Json::parse_event_t event,Json& value){
        if(depth>64)throw std::runtime_error("Save nesting limit exceeded");
        if(event==Json::parse_event_t::object_start)keys[depth+1].clear();
        if(event==Json::parse_event_t::key && !keys[depth].insert(value.get<std::string>()).second)throw std::runtime_error("Duplicate save field");
        return true;
    });
    SaveArchive::Check(data);
    if(!data.is_object() || !data.at("format").is_number_unsigned() || data.at("format")!=1)throw std::runtime_error("Unsupported SaveGame format");
    for(auto field:{"scene","components","game","subsystems","controller"})if(!data.at(field).is_object())throw std::runtime_error("Missing save state section");
    return data;
}
void NoReparse(const std::filesystem::path& path) {
    std::filesystem::path current;
    for(const auto& part:path){current/=part;const DWORD attr=GetFileAttributesW(current.c_str());
        if(attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Save path contains a reparse point");}
}
template<class F> Json WriteState(F&& visit) {Json value;SaveArchive a(value,SaveArchive::Mode::Write);visit(a);return value;}
template<class F> void ReadState(Json& value,F&& visit,const std::map<uint64_t,uint64_t>* ids=nullptr) {
    SaveArchive a(value,SaveArchive::Mode::Read,ids);visit(a);
}
}
SaveGame& Game::Saves() {if(!saveGame_)saveGame_=std::make_unique<SaveGame>(*this);return *saveGame_;}
SaveGame::SaveGame(Game& game):game_(game){}
void SaveGame::CheckThread() const {if(thread_!=std::this_thread::get_id())throw std::runtime_error("SaveGame requires its owner thread");}
void SaveGame::Configure(std::string id,std::filesystem::path root) {
    CheckThread();if(pending_||processing_)throw std::runtime_error("SaveGame is busy");Identifier(id);
    if(root.empty()){wchar_t path[32768];auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);
        if(!count||count>=32768)throw std::runtime_error("LOCALAPPDATA unavailable");root=std::filesystem::path(path)/"PainEngine"/"Saves";}
    if(!root.is_absolute())throw std::runtime_error("Save root must be absolute");
    root_=root.lexically_normal()/id/"default";NoReparse(root_);gameId_=std::move(id);
}
std::filesystem::path SaveGame::SlotPath(const std::string& slot) const {
    CheckThread();Identifier(slot);if(gameId_.empty())throw std::runtime_error("Configure SaveGame with a stable game ID first");
    auto file=root_/(slot+".save.json");NoReparse(file);return file;
}
std::vector<std::string> SaveGame::ListSlots() const {
    SlotPath("check");std::vector<std::string> slots;if(!std::filesystem::exists(root_))return slots;
    for(const auto& entry:std::filesystem::directory_iterator(root_)){
        auto name=entry.path().filename().u8string();const std::string suffix=".save.json";
        if(entry.is_regular_file()&&name.size()>suffix.size()&&name.compare(name.size()-suffix.size(),suffix.size(),suffix)==0){
            name.resize(name.size()-suffix.size());SlotPath(name);slots.push_back(name);}}
    std::sort(slots.begin(),slots.end());return slots;
}
void SaveGame::DeleteSlot(const std::string& slot){if(pending_||processing_)throw std::runtime_error("SaveGame is busy");std::filesystem::remove(SlotPath(slot));}
std::string SaveGame::ReadSlot(const std::string& slot) const {
    const auto file=SlotPath(slot);std::ifstream input(file,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot read save: "+file.u8string());
    std::string bytes;char chunk[8192];while(input.read(chunk,sizeof(chunk))||input.gcount()){
        bytes.append(chunk,static_cast<size_t>(input.gcount()));if(bytes.size()>MaxBytes)throw std::runtime_error("Save exceeds 16 MiB");}
    if(!input.eof())throw std::runtime_error("Save read failed");return bytes;
}
void SaveGame::WriteSlot(const std::string& slot,const std::string& bytes) {
    const auto file=SlotPath(slot);auto data=Parse(bytes);
    if(data.at("gameId")!=gameId_)throw std::runtime_error("Save belongs to another game");
    std::filesystem::create_directories(root_);NoReparse(root_);
    static std::atomic<uint64_t> serial{0};auto temp=file;temp+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(++serial);
    HANDLE handle=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create save temporary file: "+temp.u8string());
    DWORD written=0;bool ok=WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(handle);
    ok=CloseHandle(handle)&&ok;
    if(!ok||!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        const auto error=GetLastError();DeleteFileW(temp.c_str());throw std::runtime_error("Cannot commit save '"+file.u8string()+"': Windows error "+std::to_string(error));}
}
SaveGameResult SaveGame::Request(const std::string& slot,bool load){
    CheckThread();if(pending_||processing_)return {SaveGameResult::Status::Busy,0,"Another save operation is pending"};
    try{SlotPath(slot);}catch(const std::exception& e){return {SaveGameResult::Status::Failed,0,e.what()};}
    pendingSlot_=slot;load_=load;pending_=true;result_={SaveGameResult::Status::Accepted,++serial_,{}};return result_;
}
SaveGameResult SaveGame::RequestSave(const std::string& slot){return Request(slot,false);}
SaveGameResult SaveGame::RequestLoad(const std::string& slot){return Request(slot,true);}
void SaveGame::ProcessPending(){
    CheckThread();if(!pending_||processing_||game_.scene.IsUpdating())return;
    processing_=true;pending_=false;
    if(game_.Session&&!game_.Session->IsRunning()){
        result_.status=SaveGameResult::Status::Failed;result_.message="Game session stopped; save operation cancelled";processing_=false;return;
    }
    try{if(load_){Restore(ReadSlot(pendingSlot_));result_.status=SaveGameResult::Status::Loaded;}
        else{WriteSlot(pendingSlot_,Capture());result_.status=SaveGameResult::Status::Saved;}}
    catch(const std::exception& e){result_.status=SaveGameResult::Status::Failed;result_.message=e.what();}
    catch(...){result_.status=SaveGameResult::Status::Failed;result_.message="Save callback failed";}
    processing_=false;
}
std::string SaveGame::Capture(){
    CheckThread();if(game_.scene.IsUpdating())throw std::runtime_error("Cannot capture while scene is updating");
    if(gameId_.empty())throw std::runtime_error("Configure SaveGame first");
    auto snapshot=game_.scene.GetSnapshot();snapshot.documentVersion=3;
    for(auto& object:snapshot.components)if(object.typeName.empty()){
        const auto* instance=game_.scene.FindComponent(object.id);
        for(size_t i=0;i<catalog_.size;++i){auto prototype=catalog_.data[i].create();if(prototype&&typeid(*prototype)==typeid(*instance)){object.typeName=catalog_.data[i].typeName;break;}}
        if(object.typeName.empty())throw std::runtime_error("Register saved component type in the catalog: "+object.name);
    }
    SceneDocument::Validate(snapshot,catalog_);
    Json data={{"format",1},{"gameId",gameId_},{"scene",Json::parse(SceneDocument::Serialize(snapshot))},
        {"components",Json::object()},{"subsystems",Json::object()},{"controller",Json::object()}};
    for(const auto& object:snapshot.components)data["components"][std::to_string(object.id)]=WriteState([&](SaveArchive& a){game_.scene.FindComponent(object.id)->SerializeSaveGame(a);});
    data["game"]=WriteState([&](SaveArchive& a){game_.SerializeSaveGame(a);});
    if(game_.Session)data["subsystems"]=WriteState([&](SaveArchive& a){game_.Session->SerializeSaveGame(a);});
    if(game_.Controller)data["controller"]=WriteState([&](SaveArchive& a){game_.Controller->SerializeSaveGame(a);});
    auto bytes=data.dump(2);if(bytes.size()>MaxBytes)throw std::runtime_error("Save exceeds 16 MiB");return bytes;
}
void SaveGame::Restore(const std::string& bytes){
    CheckThread();if(game_.scene.IsUpdating())throw std::runtime_error("Cannot restore while scene is updating");
    auto data=Parse(bytes);if(gameId_.empty()||data.at("gameId")!=gameId_)throw std::runtime_error("Save belongs to another game");
    const auto snapshot=SceneDocument::Parse(data.at("scene").dump());
    Scene candidate;SceneDocument::Restore(snapshot,catalog_,candidate);
    if(data["components"].size()!=snapshot.components.size())throw std::runtime_error("Missing or extra saved component state");
    std::map<uint64_t,uint64_t> ids;
    for(const auto& item:snapshot.components)ids.emplace(item.id,candidate.FindComponentByName(item.name)->GetComponentId());
    for(const auto& item:snapshot.components){auto* object=candidate.FindComponent(ids.at(item.id));
        ReadState(data["components"].at(std::to_string(item.id)),[&](SaveArchive& a){object->SerializeSaveGame(a);},&ids);
        object->LoadedFromSaveGame=true;}
    std::unique_ptr<CharacterController> controller;
    std::unique_ptr<Player> coldPlayer;
    std::unique_ptr<UIComponent> coldHud;
    if(!game_.ProjectHudComponent && data["game"].contains("Game") && data["game"]["Game"].contains("HUD")){
        coldHud=std::make_unique<UIComponent>();
        ReadState(data["game"]["Game"]["HUD"],[&](SaveArchive& a){coldHud->SerializeSaveGame(a);},&ids);
    }
    if(!game_.FirstPlayer && data["game"].contains("Game") && data["game"]["Game"].contains("Player")){
        coldPlayer=std::make_unique<Player>();
        ReadState(data["game"]["Game"]["Player"],[&](SaveArchive& a){coldPlayer->SerializeSaveGame(a);});
    }
    if(!data["controller"].empty()){
        controller=game_.CreateCharacterController();if(!controller)throw std::runtime_error("Controller factory returned null");
        ReadState(data["controller"],[&](SaveArchive& a){controller->SerializeSaveGame(a);},&ids);
    }
    std::unique_ptr<GameSession> coldSession;
    auto* session=game_.Session;
    if(!session){coldSession=std::make_unique<GameSession>();std::string error;
        if(!coldSession->Initialize(error))throw std::runtime_error(error);session=coldSession.get();}
    auto oldGame=WriteState([&](SaveArchive& a){game_.SerializeSaveGame(a);});
    Json oldSubsystems=Json::object();
    if(session){
        oldSubsystems=WriteState([&](SaveArchive& a){session->SerializeSaveGame(a);});
        for(auto it=data["subsystems"].begin();it!=data["subsystems"].end();++it)
            if(!oldSubsystems.contains(it.key()))throw std::runtime_error("Unknown saved subsystem: "+it.key());
    }else if(!data["subsystems"].empty())throw std::runtime_error("Initialize game session before loading subsystem state");
    try{
        ReadState(data["game"],[&](SaveArchive& a){game_.SerializeSaveGame(a);},&ids);
        if(session)ReadState(data["subsystems"],[&](SaveArchive& a){session->SerializeSaveGame(a);},&ids);
    }catch(...){
        ReadState(oldGame,[&](SaveArchive& a){game_.SerializeSaveGame(a);});
        if(session)ReadState(oldSubsystems,[&](SaveArchive& a){session->SerializeSaveGame(a);});
        throw;
    }
    game_.ActionInput.Reset();game_.Controller.reset();
    game_.scene.ExchangeSavedContents(candidate);game_.loadedFromSaveGame_=true;
    if(coldSession){game_.OwnedSession=std::move(coldSession);game_.Session=game_.OwnedSession.get();}
    game_.renderedSceneRevision=UINT64_MAX;game_.bStaticShadowDirty=true;
    game_.Controller=std::move(controller);
    if(coldPlayer){game_.FirstPlayer=std::move(coldPlayer);game_.FirstPlayer->SetGamePointer(&game_);}
    if(coldHud)game_.ProjectHudComponent=std::move(coldHud);
    // Post-commit notifications cannot reject the prepared world. Failures pause it and report a warning.
    try{
        for(const auto& item:game_.scene.GetComponents())item.second->SetGame(&game_);
        if(game_.ProjectHudComponent)game_.ProjectHudComponent->SetGame(&game_);
        if(game_.GameplayInitialized&&game_.Controller){auto* character=game_.Controller->GetCharacter();if(character&&character->standardControl)game_.Controller->BindInput();}
        if(game_.GameplayInitialized){
            if(game_.Session)game_.Session->NotifySaveGameLoaded(game_.scene);
            game_.OnSaveGameLoaded();
        }
    }catch(...){game_.bSimulationEnabled=false;result_.message="World loaded; a post-load callback failed. Simulation paused.";}
}
