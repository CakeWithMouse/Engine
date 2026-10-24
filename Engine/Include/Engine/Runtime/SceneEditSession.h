#pragma once
#include <filesystem>
#include <Engine/Runtime/EditSessionApi.h>
#include <Engine/Scene/SceneCommands.h>
#include <Engine/MainGame/BaseGameClass/Game.h>

// Entire object and both Games live in the module containing the generated types.
// No gameplay start/update callbacks. A Game owns each Scene and renders that SAME Scene.
class SceneEditSession final
{
public:
    explicit SceneEditSession(ComponentTypeCatalog catalog);
    ~SceneEditSession();
    EngineEditSessionApi GetApi() noexcept;
    SceneEditSession(const SceneEditSession&) = delete;
    SceneEditSession& operator=(const SceneEditSession&) = delete;
private:
    ComponentTypeCatalog catalog_;
    std::thread::id thread_ = std::this_thread::get_id();
    bool busy_ = false;
    uint32_t state_ = EngineEditing;
    std::unique_ptr<Game> edit_;
    SceneCommands commands_;
    std::unique_ptr<Game> play_; // destroy before edit (shared viewport device)
    bool attached_ = false;
    uint32_t width_ = 0, height_ = 0;
    Game& Active() { return play_ ? *play_ : *edit_; }
    std::unique_ptr<Game> CopyForPlay();
    uint32_t Control(uint32_t command);
    std::filesystem::path contentRoot_, previousGameRoot_;
    bool ownsGameMount_ = false;
    void AddAssetApi(EngineEditSessionApi& api) noexcept;
    template<class F> static uint32_t Invoke(void* context, F&& action) noexcept
    {
        if (!context) return EngineSceneInvalidArgument;
        auto& self = *static_cast<SceneEditSession*>(context);
        if (std::this_thread::get_id() != self.thread_) return EngineSceneWrongThread;
        if (self.busy_) return EngineSceneBusy;
        self.busy_ = true;
        uint32_t result;
        try { result = action(self); } catch (...) { result = EngineSceneFailure; }
        self.busy_ = false;
        return result;
    }
};
