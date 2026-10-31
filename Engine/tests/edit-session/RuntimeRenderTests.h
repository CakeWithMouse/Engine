#include <Engine/MainGame/Player.h>
static void ExerciseRuntimeRender(const std::filesystem::path& content)
{
    HWND window = nullptr;
    const auto ticksBefore = ticks;
    {
        Game game;
        game.SetSimulationTimeScale(1);
        game.SetVSync(false);
        const auto data = SceneDocument::Parse(R"({"version":1,"name":"Render test","objects":[
            {"id":1,"type":"Probe","name":"visible","parent":0,"inheritParentScale":true,
            "transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]}}]})");
        SceneDocument::Restore(data, {types, 1}, game.GetScene());
        auto* original = game.GetScene().FindComponentByName("visible");
        const auto id = game.GetScene().GetSnapshot().components.front().id;
        Check(original != nullptr, "runtime JSON scene before graphics");
        Check(game.InitializeRuntimeWindow(false), "hidden runtime window initialized");
        window = game.GetDisplay()->GetHwnd();
        game.GetPlayer()->SetPosition({0,0,5});
        Check(ticks == ticksBefore, "runtime initialization does not Tick");
        Check(game.RunRuntimeFrame(0.0625f) == Game::RuntimeFrameResult::Running, "runtime frame");
        Check(ticks == ticksBefore + 1 && original->GetLocalPosition().x == 0.0625f, "runtime Tick once with seconds");
        Check(game.GetFrameStats().DrawCalls > 0 && static_cast<Probe*>(original)->HasBuffers(), "loaded scene geometry draws");
        Check(original->GetGame() == &game && game.GetScene().FindComponent(id) == original, "renderer and simulation use original scene");
        SetWindowPos(window, nullptr, 0, 0, 640, 480, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        Check(game.RunRuntimeFrame(0.0625f) == Game::RuntimeFrameResult::Running && game.GetDisplay()->GetWidth() > 0, "runtime resize");
        game.OnWindowResized(0,0,true);
        const auto pausedTicks = ticks;
        Check(game.RunRuntimeFrame(0.0625f) == Game::RuntimeFrameResult::Running && ticks == pausedTicks, "minimized skips simulation/render");
        game.OnWindowResized(320,240,false);
        Check(game.RunRuntimeFrame(0.0625f) == Game::RuntimeFrameResult::Running && ticks == pausedTicks + 1, "restore resumes frame");
        PostMessageW(window, WM_CLOSE, 0, 0);
        Check(game.RunRuntimeFrame(0.0625f) == Game::RuntimeFrameResult::Closed && ticks == pausedTicks + 1, "close before Tick");
        Check(IsWindow(window), "close deferred to teardown");
    }
    Check(!IsWindow(window), "runtime destroys owned window");
    MSG message{};
    Check(!PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE), "teardown leaves no stale quit");
    EnginePaths::SetContentRoot(content / "missing-runtime-shaders");
    { Game game; Check(!game.InitializeRuntimeWindow(false), "runtime shader failure without modal dialog"); }
    EnginePaths::SetContentRoot(content);
    { Game game; Check(game.InitializeRuntimeWindow(false), "window class reusable after failure"); }
}
