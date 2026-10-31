#define NOMINMAX
#include <Windows.h>
#include <Engine/Runtime/GameEditModule.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Base/Base.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

static int checks = 0;
static void Check(bool value, const char* text)
{
    if (!value) throw std::runtime_error(text);
    ++checks;
}
struct Row { uint64_t id, parent; std::string name, type; EngineSceneTransform transform; };
struct Snapshot { EngineSceneInfo info{}; std::vector<Row> rows; };
static Snapshot Read(const EngineEditSessionApi& api, bool active = false)
{
    Snapshot result;
    auto visitor = [](void* p, const EngineSceneObjectInfo* o) {
        try { static_cast<Snapshot*>(p)->rows.push_back({o->id, o->parentId, o->name, o->typeName, o->transform}); }
        catch (...) { std::terminate(); }
    };
    const auto status = active ? api.readActiveScene(api.context, &result.info, visitor, &result) :
        api.scene.readScene(api.scene.context, &result.info, visitor, &result);
    Check(status == EngineSceneOk, "snapshot"); return result;
}
static uint32_t State(const EngineEditSessionApi& api)
{
    uint32_t state = 99; Check(api.getState(api.context, &state) == EngineSceneOk, "state"); return state;
}
static int live = 0, ticks = 0;
static bool failConstruction = false, failTick = false;
static GameComponent* last = nullptr;
struct Probe : CubeComponent
{
    Probe()
    {
        if (failConstruction) throw std::runtime_error("expected constructor failure");
        InitCube(); last = this; ++live;
    }
    ~Probe() override { --live; }
    bool HasBuffers() const { return vb && ib; }
    void Tick(float dt) override
    {
        if (failTick) throw std::runtime_error("expected tick failure");
        ++ticks; SetPosition(GetLocalPosition() + glm::vec3(dt, 0, 0));
    }
};
static const ComponentType types[] = {{"Probe", "Probe", EngineComponentDetail::Create<Probe>, {}}};

static void Exercise(const EngineEditSessionApi& api, const char* type)
{
    Check(State(api) == EngineEditing, "initial Edit");
    uint64_t parent = 0, child = 0;
    EngineSceneTransform t{{4,0,0}, {0,0,0,1}, {1,1,1}};
    Check(api.scene.createObject(api.scene.context, type, "parent", &t, &parent) == EngineSceneOk, "create edit object");
    Check(api.scene.createObject(api.scene.context, type, "child", nullptr, &child) == EngineSceneOk, "create child");
    Check(api.setParent(api.context, child, parent, 0) == EngineSceneOk, "set parent");
    Check(api.setParent(api.context, parent, child, 1) == EngineSceneInvalidArgument, "cycle rejected");
    const auto edit = Read(api);
    Check(api.frame(api.context, 0.05) == EngineSceneOk, "edit frame");
    Check(Read(api).rows[0].transform.position[0] == 4, "no simulation in edit");
    Check(api.control(api.context, EnginePlay) == EngineSceneOk && State(api) == EnginePlaying, "start play");
    auto playing = Read(api, true);
    Check(playing.info.id != edit.info.id && playing.rows.size() == 2, "independent play Scene");
    Check(playing.rows[1].parent == playing.rows[0].id, "parent remapped");
    Check(api.scene.renameObject(api.scene.context, parent, "bad") == EngineSceneReadOnly, "edit locked in play");
    Check(api.setParent(api.context, child, 0, 1) == EngineSceneReadOnly, "hierarchy locked");
    Check(api.frame(api.context, 0.05) == EngineSceneOk, "play frame");
    Check(std::abs(Read(api, true).rows[0].transform.position[0] - 4.05f) < 1e-5f, "derived logic once per frame");
    Check(Read(api).rows[0].transform.position[0] == 4, "edit unchanged by simulation");
    Check(api.control(api.context, EnginePause) == EngineSceneOk && State(api) == EnginePaused, "pause");
    playing = Read(api, true);
    Check(api.frame(api.context, 0.05) == EngineSceneOk, "paused frame");
    auto paused = Read(api, true);
    Check(paused.info.id == playing.info.id && paused.rows[0].transform.position[0] == playing.rows[0].transform.position[0], "pause preserves same instances/data");
    uint64_t bad = 88;
    Check(api.scene.createObject(api.scene.context, type, "bad", nullptr, &bad) == EngineSceneReadOnly && bad == 0, "create locked while paused");
    Check(api.control(api.context, EngineResume) == EngineSceneOk, "resume");
    Check(api.frame(api.context, 0.05) == EngineSceneOk, "resumed frame");
    Check(api.control(api.context, EngineStop) == EngineSceneOk && State(api) == EngineEditing, "stop");
    Check(Read(api, true).info.id == edit.info.id && Read(api).rows[0].transform.position[0] == 4, "stop restores original Scene");
    Check(api.control(api.context, EnginePlay) == EngineSceneOk, "second play");
    Check(Read(api, true).rows[0].transform.position[0] == 4, "second play starts from edit data");
    Check(api.control(api.context, EngineStop) == EngineSceneOk, "second stop");
    Check(api.control(api.context, EngineResume) == EngineSceneInvalidArgument, "invalid transition");
    Check(api.frame(api.context, std::numeric_limits<double>::quiet_NaN()) == EngineSceneInvalidArgument, "invalid dt");
    uint32_t wrong = 0;
    std::thread worker([&] { uint32_t s; wrong = api.getState(api.context, &s); }); worker.join();
    Check(wrong == EngineSceneWrongThread, "wrong thread");
    struct Callback { const EngineEditSessionApi* api; uint32_t status = 0; } callback{&api};
    Check(api.scene.listTypes(api.scene.context, [](void* p, const EngineSceneTypeInfo*) {
        auto& c = *static_cast<Callback*>(p); c.status = c.api->control(c.api->context, EnginePlay);
    }, &callback) == EngineSceneOk && callback.status == EngineSceneBusy, "cross-service reentry blocked");
}
#include "SceneJsonTests.h"
#include "StartupSceneTests.h"
#include "GameStartupTests.h"
#include "RuntimeRenderTests.h"
int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 4, "usage: EditSessionTests <game.dll> <EngineContent> <consumer.dll>");
        const auto module = std::filesystem::absolute(argv[1]);
        const auto content = std::filesystem::absolute(argv[2]);
        EnginePaths::SetContentRoot(content);
        ExerciseRuntimeRender(content);
        {
            SceneEditSession jsonSession({types, 1});
            ExerciseJson(jsonSession.GetApi(), "Probe", true);
            ExerciseJsonLocale();
            ExerciseJsonVfsWrite();
        }
        {
            GameEditModule jsonModule(module, content);
            ExerciseJson(jsonModule.Api(), "RuntimeExample::Mover");
        }
        {
            SceneEditSession session({types, 1}); auto api = session.GetApi();
            Exercise(api, "Probe");
            Check(live == 2, "play objects destroyed on stop");
            failConstruction = true;
            Check(api.control(api.context, EnginePlay) == EngineSceneFailure, "failed clone rejected");
            failConstruction = false;
            Check(State(api) == EngineEditing && live == 2 && Read(api).rows.size() == 2, "failed clone preserves edit");
            Check(api.control(api.context, EnginePlay) == EngineSceneOk, "play before tick failure");
            failTick = true;
            Check(api.frame(api.context, 0.01) == EngineSceneSimulationFailed && State(api) == EnginePaused, "tick failure freezes game copy");
            failTick = false;
            Check(api.control(api.context, EngineStop) == EngineSceneOk, "stop after tick failure");
        }
        Check(live == 0, "all CPU objects destroyed");
        {
            GameEditModule dll(module, content); // no ordinary game lifecycle callbacks
            Exercise(dll.Api(), "RuntimeExample::Mover");
        }
        HWND window = CreateWindowExW(0, L"STATIC", L"Engine API test", WS_OVERLAPPEDWINDOW,
            0, 0, 320, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(window != nullptr, "test HWND (no editor)");
        try
        {
            SceneEditSession session({types, 1}); auto api = session.GetApi();
            uint64_t id;
            Check(api.scene.createObject(api.scene.context, "Probe", "Visible", nullptr, &id) == EngineSceneOk, "create before attach");
            auto* editObject = last;
            const auto sceneId = Read(api).info.id;
            EnginePaths::SetContentRoot(content / "missing-test-root");
            Check(api.attachViewport(api.context, window, 320, 240) == EngineSceneFailure, "failed viewport setup");
            Check(Read(api).info.id == sceneId && live == 1, "failed attach preserves scene objects");
            EnginePaths::SetContentRoot(content);
            Check(api.attachViewport(api.context, window, 320, 240) == EngineSceneOk, "attach same edit Scene");
            auto overlay = session.GetViewportOverlayApi();
            EngineCameraInfo camInfo{};
            Check(overlay.getCamera(overlay.context, &camInfo) == EngineSceneOk && camInfo.width == 320 && camInfo.height == 240, "getCamera returns valid camera state");
            Check(overlay.setOverlayLines(overlay.context, nullptr, 0, 1.0f) == EngineSceneOk, "empty setOverlayLines");
            EngineOverlayLine testLine{0,0,0, 1,1,1, 0xFFFFFFFF, 0};
            Check(overlay.setOverlayLines(overlay.context, &testLine, 1, 1.0f) == EngineSceneOk, "setOverlayLines with line");
            uint8_t rgba[16] = {255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255};
            Check(overlay.setOverlayIcon(overlay.context, 1, 2, 2, rgba) == EngineSceneOk, "setOverlayIcon");
            EngineOverlayBillboard testBb{1, 0,0,0, 24.0f, 0xFFFFFFFF, 1};
            Check(overlay.setOverlayBillboards(overlay.context, &testBb, 1) == EngineSceneOk, "setOverlayBillboards");
            Check(overlay.setOverlayBillboards(overlay.context, nullptr, 0) == EngineSceneOk, "clear setOverlayBillboards");
            Check(overlay.setOverlayLines(overlay.context, nullptr, 0, 1.0f) == EngineSceneOk, "clear setOverlayLines");
            const auto beforeTicks = ticks;
            Check(api.frame(api.context, 0.05) == EngineSceneOk && ticks == beforeTicks, "render Edit without Tick");
            Check(editObject->GetGame() && editObject->GetGame()->GetScene().FindComponent(id) == editObject, "renderer uses exact edited instance");
            Check(static_cast<Probe*>(editObject)->HasBuffers() && editObject->GetGame()->GetFrameStats().DrawCalls > 0, "existing geometry rendered");
            EnginePaths::SetContentRoot(content / "missing-play-root");
            Check(api.control(api.context, EnginePlay) == EngineSceneFailure, "failed Play viewport rolls back");
            EnginePaths::SetContentRoot(content);
            Check(State(api) == EngineEditing && live == 1 && api.frame(api.context, 0.05) == EngineSceneOk, "edit targets restored after failed Play");
            Check(api.control(api.context, EnginePlay) == EngineSceneOk, "viewport Play");
            auto* playObject = last;
            Check(playObject != editObject, "distinct play instance");
            Check(api.frame(api.context, 0.05) == EngineSceneOk && ticks == beforeTicks + 1, "render has no duplicate Tick");
            Check(playObject->GetGame()->GetScene().GetId() == Read(api, true).info.id, "active renderer uses play Scene");
            Check(api.control(api.context, EnginePause) == EngineSceneOk, "viewport pause");
            Check(api.resizeViewport(api.context, 400, 300) == EngineSceneOk && api.frame(api.context, 0.05) == EngineSceneOk, "paused resize/render");
            Check(ticks == beforeTicks + 1, "paused render has no Tick");
            Check(api.control(api.context, EngineStop) == EngineSceneOk && api.frame(api.context, 0.05) == EngineSceneOk, "restore edit targets");
            Check(editObject->GetGame()->GetScene().FindComponent(id) == editObject && live == 1, "original edit object retained");
            ExerciseJson(api, "Probe", true, content); // same HWND across JSON replacements and Play
        }
        catch (...) { DestroyWindow(window); throw; }
        try
        {
            GameEditModule dll(module, content);
            const auto& api = dll.Api();
            uint64_t id;
            Check(api.scene.createObject(api.scene.context, "RuntimeExample::Mover", "DLL viewport", nullptr, &id) == EngineSceneOk, "DLL viewport object");
            Check(api.attachViewport(api.context, window, 320, 240) == EngineSceneOk, "DLL viewport attach");
            Check(dll.ViewportOverlayApi().getCamera != nullptr, "DLL exposes overlay extension");
            Check(api.frame(api.context, 0.05) == EngineSceneOk, "DLL edit frame");
            Check(api.control(api.context, EnginePlay) == EngineSceneOk && api.frame(api.context, 0.05) == EngineSceneOk, "DLL play viewport");
            Check(api.control(api.context, EngineStop) == EngineSceneOk && api.frame(api.context, 0.05) == EngineSceneOk, "DLL restore viewport");
        }
        catch (...) { DestroyWindow(window); throw; }
        DestroyWindow(window);
        Check(live == 0, "viewport teardown before HWND destruction");
        // Full runtime handshake against an ABI-only consumer fixture, no Editor/Qt.
        BaseEngine engine;
        RuntimeLaunchOptions options;
        options.mode = RuntimeMode::Editor;
        options.module = std::filesystem::absolute(argv[3]);
        options.editModule = module;
        options.frames = 3;
        options.projectName = "API integration test";
        Check(engine.Run(options) == 0, "runtime binds edit API before consumer start");
        ExerciseStartup(module, std::filesystem::absolute(argv[3]));
        ExerciseGameStartup(module);
        std::cout << "Edit session: " << checks << " checks passed (no editor)\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
