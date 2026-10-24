#pragma once
#include <Engine/Components/ComponentType.h>
#include <Engine/Runtime/SceneModuleApi.h>
#include <Engine/Scene/Scene.h>
#include <thread>

// Lives beside Scene INSIDE the module owning its objects. Neither dependency
// may die before this service. Generated catalog contains no live objects.
class SceneCommands final
{
public:
    SceneCommands(Scene& scene, ComponentTypeCatalog catalog, bool* sharedBusy = nullptr);
    void SetReadOnly(bool value) noexcept { readOnly_ = value; }
    // Owner-only, between calls, on the engine thread. Existing API tables stay valid.
    void Rebind(Scene& scene) noexcept { scene_ = &scene; }
    SceneCommands(const SceneCommands&) = delete;
    SceneCommands& operator=(const SceneCommands&) = delete;
    EngineSceneApi GetApi() noexcept;
private:
    Scene* scene_;
    ComponentTypeCatalog catalog_;
    std::thread::id thread_;
    bool busy_ = false;
    bool readOnly_ = false;
    bool* sharedBusy_ = nullptr;

    template<class F> static uint32_t Invoke(void* context, F&& action) noexcept
    {
        if (!context) return EngineSceneInvalidArgument;
        auto& self = *static_cast<SceneCommands*>(context);
        if (std::this_thread::get_id() != self.thread_) return EngineSceneWrongThread;
        if (self.busy_ || (self.sharedBusy_ && *self.sharedBusy_) || self.scene_->IsUpdating()) return EngineSceneBusy;
        self.busy_ = true;
        if (self.sharedBusy_) *self.sharedBusy_ = true;
        uint32_t result;
        try { result = action(self); }
        catch (...) { result = EngineSceneFailure; }
        self.busy_ = false;
        if (self.sharedBusy_) *self.sharedBusy_ = false;
        return result;
    }
    static uint32_t ENGINE_MODULE_CALL ListTypes(void*, EngineSceneTypeVisitor, void*) noexcept;
    static uint32_t ENGINE_MODULE_CALL ReadScene(void*, EngineSceneInfo*, EngineSceneObjectVisitor, void*) noexcept;
    static uint32_t ENGINE_MODULE_CALL CreateObject(void*, const char*, const char*, const EngineSceneTransform*, uint64_t*) noexcept;
    static uint32_t ENGINE_MODULE_CALL RemoveObject(void*, uint64_t) noexcept;
    static uint32_t ENGINE_MODULE_CALL RenameObject(void*, uint64_t, const char*) noexcept;
    static uint32_t ENGINE_MODULE_CALL SetTransform(void*, uint64_t, const EngineSceneTransform*) noexcept;
    static uint32_t ENGINE_MODULE_CALL SetPointLight(void*, uint64_t, const EngineScenePointLight*) noexcept;
};
