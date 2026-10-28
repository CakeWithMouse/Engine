#pragma once
#include <Engine/Runtime/EditSessionApi.h>
#include <filesystem>
#include <Engine/Runtime/EditorReloadApi.h>
#include <Engine/Runtime/LegacyEditApi.h>
#include <Engine/Runtime/ProjectDataApi.h>
#include <Engine/Runtime/SelectionApi.h>
#include <Engine/Runtime/ViewportInputApi.h>

// Host-side RAII owner of an edit-only DLL session. Must be created, called and
// destroyed on the engine thread, before HWND destruction. Reload reconstructs scene objects from a validated checkpoint.
class GameEditModule final
{
public:
    GameEditModule(const std::filesystem::path& module, const std::filesystem::path& engineContent,
        const std::filesystem::path& projectContent = {});
    ~GameEditModule();
    uint32_t Reload(const std::filesystem::path& module, const std::filesystem::path& checkpoint,
        void* hwnd, uint32_t width, uint32_t height, char* error, uint32_t capacity) noexcept;
    GameEditModule(const GameEditModule&) = delete;
    GameEditModule& operator=(const GameEditModule&) = delete;
    const EngineEditSessionApi& Api() const noexcept { return api_; }
    const EngineEditorCameraApi& EditorCameraApi() const noexcept { return camera_; }
    uint32_t PreferredSceneVersion() const noexcept { return preferredSceneVersion_; }
    const EngineProjectDataApi& ProjectDataApi() const noexcept { return projectData_; }
    const EngineViewportInputApi& ViewportInputApi() const noexcept { return viewportInput_; }
    const EngineSelectionApi& SelectionApi() const noexcept { return selection_; }
private:
    std::filesystem::path engineContent_, projectContent_, loadedModule_;
    EngineEditorCameraApi camera_{};
    void* library_ = nullptr;
    EngineEditModuleApi factory_{};
    EngineEditSessionApi api_{};
    EngineProjectDataApi projectData_{};
    EngineSelectionApi selection_{};
    EngineViewportInputApi viewportInput_{};
    EngineLegacyV4::Factory legacyFactory_{};
    EngineLegacyV4::EditApi legacy_{};
    uint32_t preferredSceneVersion_ = 1;
};
