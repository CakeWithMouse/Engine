#pragma once
#include <Engine/Base/Config/BaseResources.h>
#include <Engine/Base/Config/BaseGameConfig.h>
#include <Engine/Resources/VirtualFileSystem.h>
#include <Engine/Runtime/LaunchOptions.h>
#include <filesystem>
#include <memory>
#include <string>
#include <functional>
#include <optional>
#include <vector>

class Game;
class ViewportSession;

// Host owns this object; game/editor modules see only ModuleApi.h.
// One run per instance. Legacy Game still owns its independent old launch path.
class BaseEngine final
{
    BaseResources Resources;
    BaseGameConfig Config;
    VirtualFileSystem& Vfs;
    enum class State { New, Starting, Ready, Playing, Stopped };
    State state = State::New;
    bool exitRequested = false;

    struct AppliedMount
    {
        std::string virtualRoot;
        std::optional<std::filesystem::path> previousPath;
    };
    std::vector<AppliedMount> appliedMounts;

    void ApplyMount(std::string_view virtualRoot, const std::filesystem::path& physicalPath);

    class LoadedSession;
    std::unique_ptr<LoadedSession> activeSession;
    RuntimeLaunchOptions activeOptions;
    std::unique_ptr<ViewportSession> viewportSession;
    std::function<std::unique_ptr<Game>()> viewportFactory;

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
    // The executable chooses a preview scene; Engine has no dependency on game examples.
    void SetViewportFactory(std::function<std::unique_ptr<Game>()> factory) { viewportFactory = std::move(factory); }

    VirtualFileSystem& GetVFS() noexcept { return Vfs; }
    const VirtualFileSystem& GetVFS() const noexcept { return Vfs; }

    // Hot-Reload & Compilation Trigger
    bool ReloadActiveModule();
    static bool TriggerBuild(const std::filesystem::path& repositoryRoot,
                            const std::string& target = "",
                            const std::string& configuration = "Debug",
                            bool waitForCompletion = false);
    bool TriggerProjectBuild(bool waitForCompletion = false);
};
