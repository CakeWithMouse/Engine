#pragma once
#include "IEditorHost.h"
#include "EditorHistory.h"
#include "ViewportCommandHandler.h"
#include "AssetCommandHandler.h"
#include "SceneCommandHandler.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <memory>
#include <chrono>
#include <functional>

class EditorQtHost : public IEditorHost
{
public:
    using UiLauncher = std::function<void(IEditorHost*, std::atomic<bool>*)>;

    explicit EditorQtHost(const EngineModuleHost& host, UiLauncher uiLauncher = nullptr);
    ~EditorQtHost() override;
    EditorQtHost(const EditorQtHost&) = delete;
    EditorQtHost& operator=(const EditorQtHost&) = delete;

    bool Start(); // поток движка
    void Update(double dt); // поток движка
    void Overlay() {} // поток движка
    void Stop(); // поток движка

    // IEditorHost methods
    void PostCommand(const EditorCommand& c) override;
    Snapshot ReadSnapshot() override;
    void Log(const char* text) override;

    bool HasScene() const override { return sceneAvailable_.load(); }
    bool HasMaterials() const override { return materialsAvailable_.load(); }
    bool HasViewport() const override { return HasScene() || host_.viewportAttach != nullptr; }
    bool ViewportActive() const override { return viewportActive_.load(); }
    EditorState PlayState() const override { return playState_.load(); }

    double LastDt() const override { return lastDt_.load(); }
    uint64_t UpdateCount() const override { return updates_.load(); }
    const EngineModuleHost& host() const override { return host_; }
    const EngineEditSessionApi& scene() const override { return scene_; }

    void MoveSelectedObject(float right, float up, float forward) override;
    void FetchSceneAndSelectNext() override;
    bool HasSelection() const override;
    std::string GetSelectedObjectName() override;

    void SetGizmoMode(EditorGizmoMode mode) override;
    EditorGizmoMode GizmoMode() const override { return static_cast<EditorGizmoMode>(gizmoMode_.load()); }
    void SetParent(uint64_t childId, uint64_t parentId) override;

    void RunOnUi(std::function<void()> action) override;
    void SetUiListener(IEditorUiListener* listener) override;

    // Configuration / extension APIs
    void SetModuleApi(const EngineEditSessionApi& scene);
    void SetProjectDataApi(const EngineProjectDataApi& api) { projectData_ = api; }
    void SetViewportInputApi(const EngineViewportInputApi& api) { viewportInput_ = api; }
    void SetSelectionApi(const EngineSelectionApi& api) { selection_ = api; }

private:
    void UiThreadMain();
    void DrainCommands();
    void Complete(const EditorCommand& command, uint32_t status, const QString& error = {});

    EngineModuleHost host_;
    EngineEditSessionApi scene_{};
    EngineProjectDataApi projectData_{};
    EngineSelectionApi selection_{};
    EngineViewportInputApi viewportInput_{};

    std::atomic<bool> materialsAvailable_{false};
    std::atomic<bool> sceneAvailable_{false};
    std::atomic<bool> viewportActive_{false};
    std::atomic<EditorState> playState_{Edit};
    std::atomic<uint32_t> gizmoMode_{EditorGizmoTranslate};

    std::atomic<double> lastDt_{0.0};
    std::atomic<uint64_t> updates_{0};

    std::mutex logMutex_;
    std::mutex listenerMutex_;
    IEditorUiListener* uiListener_ = nullptr;

    std::thread uiThread_;
    std::atomic<bool> uiReady_{false};
    UiLauncher uiLauncher_;

    std::mutex commandMutex_;
    std::deque<EditorCommand> commands_;

    EditorHistory history_;
    ViewportCommandHandler viewportHandler_;
    AssetCommandHandler assetHandler_;
    SceneCommandHandler sceneHandler_;
};
