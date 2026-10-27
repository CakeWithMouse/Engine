#pragma once
#include <filesystem>
#include <map>
#include <set>
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

    // Material editing: previews are small scenes of their own (one per editor window).
    std::map<uint64_t, std::unique_ptr<Game>> previews_;
    uint64_t nextPreview_ = 1;
    std::set<std::string> modifiedMaterials_; // normalized .mat paths with unsaved edits
    void AddMaterialApi(EngineEditSessionApi& api) noexcept;
    /** Applies an edit to the material in every game that loaded it and marks it modified. */
    template<class F> uint32_t EditMaterial(const char* material, F&& change);
    /** Every game that may draw a material: the edit scene, the Play copy and the previews. */
    std::vector<Game*> Games();
    void RenderPreviews(double dt);
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
