#include "Counter.h"
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Components/GameComponents.h>
#include <iostream>

static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static Game* play = nullptr;
static EngineEditSessionApi callbackApi{};
static std::unique_ptr<Game> Factory() { auto game = std::make_unique<Game>(); play = game.get(); return game; }
class BeginPlayProbe final : public GameComponent {
public:
    bool begun=false;
    void BeginPlay() override {
        Check(GetGame()->GetSubsystem<Counter>()->scenes==1,"OnSceneReady precedes BeginPlay");
        begun=true;
    }
};
class Ordered final : public GameSubsystem {
    int id;
public:
    explicit Ordered(int value) : id(value) { events.push_back(id); }
    bool Initialize(SubsystemContext&, std::string&) override { events.push_back(id+10); return id!=2; }
    void Deinitialize() noexcept override { events.push_back(id+20); }
    ~Ordered() override { events.push_back(id+30); }
};
int main()
{
    try {
        std::string error;
        { Game edit; Check(!edit.GetSubsystem<Counter>() && created == 0, "edit has no subsystem"); }
        {
            GameSession session;
            Check(session.Initialize(error), "initialize catalog");
            auto* counter = session.GetSubsystem<Counter>();
            Check(counter && created == 1, "one generated instance");
            session.Tick(.1f); Check(counter->ticks == 0, "no scene no tick");
            {
                Game first; first.AttachSession(session);
                std::unique_ptr<GameComponent> probe=std::make_unique<BeginPlayProbe>();
                auto* probePtr=static_cast<BeginPlayProbe*>(probe.get());
                first.GetScene().AddComponent("probe",probe);
                first.InitializeGameplay();
                Check(probePtr->begun,"BeginPlay invoked");
                Check(first.GetSubsystem<Counter>() == counter && counter->scenes == 1, "ready first scene");
                first.InitializeGameplay(); Check(counter->scenes == 1, "no repeated ready");
                first.StepProjectSimulation(.03f); Check(counter->ticks == 0, "disabled by default");
                counter->bTickEnabled = true;
                first.StepProjectSimulation(.03f); Check(counter->ticks == 1 && counter->lastDt == .03f, "one unscaled tick");
                counter->bTickEnabled = false;
                first.StepProjectSimulation(.03f); Check(counter->ticks == 1, "runtime disable");
            }
            Check(session.IsRunning() && !session.GetScene() && destroyed == 0, "session survives Game");
            Game second; second.AttachSession(session); second.InitializeGameplay();
            Check(second.GetSubsystem<Counter>() == counter && counter->scenes == 2, "same instance across scenes");
            session.Stop(); session.Stop(); second.StepProjectSimulation(.03f);
            Check(destroyed == 1, "idempotent stop");
        }
        Check(events == std::vector<int>({1,2,3,2,3,4,5}), "lifecycle order");
        events.clear();
        {
            SceneEditSession editor({}, Factory); auto api = editor.GetApi();
            const int before = created;
            Check(api.control(api.context, EnginePlay) == EngineSceneOk, "headless play");
            auto* counter = play->GetSubsystem<Counter>(); counter->bTickEnabled = true;
            const int identity = counter->identity;
            Check(api.frame(api.context, .02) == EngineSceneOk && counter->ticks == 1, "single editor tick");
            Check(api.control(api.context, EnginePause) == EngineSceneOk, "pause");
            api.frame(api.context,.02); Check(counter->ticks == 1, "no paused tick");
            api.control(api.context, EngineResume);
            throwTick = true;
            Check(api.frame(api.context,.02) == EngineSceneSimulationFailed, "exception pauses");
            throwTick = false;
            uint32_t state; api.getState(api.context, &state); Check(state == EnginePaused, "paused on error");
            api.control(api.context, EngineStop); api.control(api.context, EngineStop);
            Check(api.control(api.context, EnginePlay) == EngineSceneOk, "replay");
            Check(play->GetSubsystem<Counter>()->identity != identity && created == before + 2, "new session on replay");
        }
        failInit = true; events.clear();
        { GameSession failed; Check(!failed.Initialize(error) && error == "expected init failure", "init failure"); }
        Check(events == std::vector<int>({1,4,5}), "partial init rollback"); failInit = false;
        events.clear(); destroyed = 0; stopTick = true;
        { Game game; game.InitializeGameplay(); game.GetSubsystem<Counter>()->bTickEnabled=true;
          game.StepProjectSimulation(.01f); Check(destroyed == 1, "deferred stop"); }
        stopTick = false;
        events.clear(); stopReady = true;
        { Game game; bool failed=false; try { game.InitializeGameplay(); } catch (...) {failed=true;}
          Check(failed && !game.GetSubsystem<Counter>(), "stop ready rolls back"); }
        stopReady=false;
        {
            SceneEditSession editor({},Factory); callbackApi=editor.GetApi();
            callbackApi.control(callbackApi.context,EnginePlay);
            play->GetSubsystem<Counter>()->bTickEnabled=true;
            onTick=[] { Check(callbackApi.control(callbackApi.context,EngineStop)==EngineSceneOk,"reentrant API Stop accepted"); };
            Check(callbackApi.frame(callbackApi.context,.01)==EngineSceneOk,"deferred API Stop frame");
            onTick=nullptr;
            uint32_t state;callbackApi.getState(callbackApi.context,&state);
            Check(state==EngineEditing,"deferred API Stop completed");
            onInitialize=[] {callbackApi.control(callbackApi.context,EngineStop);};
            Check(callbackApi.control(callbackApi.context,EnginePlay)==EngineSceneFailure,"Stop during Initialize rolls back Play");
            onInitialize=nullptr;
            Check(callbackApi.control(callbackApi.context,EnginePlay)==EngineSceneOk,"Play after interrupted startup");
            onDeinitialize=[] {callbackApi.control(callbackApi.context,EngineStop);};
            Check(callbackApi.control(callbackApi.context,EngineStop)==EngineSceneOk,"Stop from Deinitialize is deferred");
            onDeinitialize=nullptr;
        }
        const GameSubsystemType duplicate[] = {{"same", &CreateGameSubsystem<Counter>},{"same", &CreateGameSubsystem<Counter>}};
        { GameSession session; const int before=created; Check(!session.Initialize({duplicate,2},error) && before==created, "duplicates before construction"); }
        { GameSession session; Check(session.Initialize({},error), "empty catalog"); }
        events.clear();
        const GameSubsystemType ordered[] = {
            {"Z", []()->std::unique_ptr<GameSubsystem>{return std::make_unique<Ordered>(2);}},
            {"A", []()->std::unique_ptr<GameSubsystem>{return std::make_unique<Ordered>(1);}}};
        { GameSession session; Check(!session.Initialize({ordered,2},error), "second Initialize fails"); }
        Check(events == std::vector<int>({1,2,11,12,22,21,32,31}), "sorted create/init and reverse rollback");
        events.clear();
        const GameSubsystemType constructorFailure[] = {
            {"A", []()->std::unique_ptr<GameSubsystem>{return std::make_unique<Ordered>(1);}},
            {"B", []()->std::unique_ptr<GameSubsystem>{throw std::runtime_error("constructor failure");}}};
        { GameSession session; Check(!session.Initialize({constructorFailure,2},error),"factory failure"); }
        Check(events==std::vector<int>({1,31}),"construction rollback before any Initialize");
        // Real viewport: frame steps simulation, RenderViewportFrame must not tick again.
        wchar_t executable[MAX_PATH]; GetModuleFileNameW(nullptr,executable,MAX_PATH);
        EnginePaths::SetContentRoot(std::filesystem::path(executable).parent_path()/"EngineContent");
        const HWND window=CreateWindowExW(0,L"STATIC",L"Subsystem test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Check(window!=nullptr,"test window");
        {
            const int before=created;
            SceneEditSession editor({},Factory); auto api=editor.GetApi();
            Check(api.attachViewport(api.context,window,320,240)==EngineSceneOk,"attach viewport");
            Check(created==before,"viewport edit creates no subsystems");
            const HWND previewWindow=CreateWindowExW(0,L"STATIC",L"Preview test",WS_OVERLAPPEDWINDOW,0,0,160,120,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            uint64_t preview=0;
            Check(api.attachMaterialPreview(api.context,previewWindow,160,120,&preview)==EngineSceneOk,"attach material preview");
            Check(created==before,"preview creates no subsystems");
            api.detachMaterialPreview(api.context,preview);DestroyWindow(previewWindow);
            Check(api.control(api.context,EnginePlay)==EngineSceneOk,"viewport play");
            auto* counter=play->GetSubsystem<Counter>();counter->bTickEnabled=true;
            Check(api.frame(api.context,.02)==EngineSceneOk && counter->ticks==1,"viewport no double tick");
            api.control(api.context,EnginePause);api.frame(api.context,.02);
            Check(counter->ticks==1,"viewport pause");
            api.control(api.context,EngineStop);
        }
        DestroyWindow(window);
        std::cout << "Subsystem lifecycle and headless editor tests passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
