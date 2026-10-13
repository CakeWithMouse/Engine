#pragma once
#include <Engine/Base/Config/BaseResources.h>
#include <Engine/Base/Config/BaseGameConfig.h>
#include <Engine/Runtime/LaunchOptions.h>
#include <filesystem>
#include <memory>
#include <string>

// Host owns this object; game/editor modules see only ModuleApi.h.
// One run per instance. Legacy Game still owns its independent old launch path.
class BaseEngine final
{
    BaseResources Resources;
    BaseGameConfig Config;
    enum class State { New, Starting, Ready, Playing, Stopped };
    State state = State::New;
    bool exitRequested = false;

    class LoadedSession;
    std::unique_ptr<LoadedSession> activeSession;
    RuntimeLaunchOptions activeOptions;

    bool Initialize(const BaseGameConfig& config, bool failForTest);
    bool StartUp(bool failForTest);
    int Play(const RuntimeLaunchOptions& options);
public:
    BaseEngine();
    ~BaseEngine();
    BaseEngine(const BaseEngine&) = delete;
    BaseEngine& operator=(const BaseEngine&) = delete;
    int Run(const RuntimeLaunchOptions& options, const BaseGameConfig& config = {});
    bool IsPlaying() const { return state == State::Playing; }
    void RequestExit() noexcept { exitRequested = true; }
    void Stop() noexcept;

    // Hot-Reload & Compilation Trigger
    bool ReloadActiveModule();
    static bool TriggerBuild(const std::filesystem::path& repositoryRoot,
                            const std::string& target = "",
                            const std::string& configuration = "Debug",
                            bool waitForCompletion = false);
    bool TriggerProjectBuild(bool waitForCompletion = false);
};
