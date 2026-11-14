#pragma once
#include "IEditorHost.h"
#include "EditorHistory.h"
#include "ViewportCommandHandler.h"
#include "AssetCommandHandler.h"
#include "SceneCommandHandler.h"
#include <Engine/Runtime/EditExtensions.h>
#include <Engine/Runtime/PhysicsEditApi.h>
#include <Engine/Runtime/PhysicsObjectEditApi.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <memory>
#include <chrono>
#include <functional>
#include "RecoveryStore.h"

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

    bool GetCamera(EngineCameraInfo& info) const override;
    void SetOverlayLines(const std::vector<EngineOverlayLine>& lines, float thickness = 1.0f) override;
    void SetOverlayIcon(uint32_t iconId, uint32_t width, uint32_t height, const uint8_t* rgbaPixels) override;
    void SetOverlayBillboards(const std::vector<EngineOverlayBillboard>& billboards) override;
    void SetOverlayTriangles(const std::vector<EngineOverlayTriangle>& triangles) override;

    void RunOnUi(std::function<void()> action) override;
    void SetUiListener(IEditorUiListener* listener) override;

    // Configuration / extension APIs
    void SetModuleApi(const EngineEditSessionApi& scene);
    void SetProjectDataApi(const EngineProjectDataApi& api) { projectData_ = api; }
    void SetViewportCursorApi(const EngineViewportCursorApi& api){cursor_=api;}
    void SetViewportInputApi(const EngineViewportInputApi& api) {
        viewportInput_ = api;
        // A replacement session has no focus history. The widget can retain focus
        // across reload, so no new Qt focusIn event is guaranteed to arrive.
        if (api.focus) api.focus(api.context, viewportFocused_ ? 1u : 0u);
    }
    void SetSelectionApi(const EngineSelectionApi& api) { selection_ = api; }
    void SetAudioPreviewApi(const EngineAudioPreviewApi& api) { audio_ = api; }
    void SetPhysicsApi(const EnginePhysicsApi& api) { physics_ = api; }
    // Доступен только если игровой модуль реализует EnginePhysicsExtension.
    const EnginePhysicsApi* PhysicsApi() const { return physics_.read ? &physics_ : nullptr; }
    void SetPhysicsObjectApi(const EnginePhysicsObjectApi& api) { physicsObject_ = api; }
    // Доступен только если игровой модуль реализует EnginePhysicsObjectExtension.
    const EnginePhysicsObjectApi* PhysicsObjectApi() const { return physicsObject_.read ? &physicsObject_ : nullptr; }
    // Доступен только если игровой модуль реализует EngineAudioPreviewExtension.
    const EngineAudioPreviewApi* AudioApi() const { return audio_.play ? &audio_ : nullptr; }
    void SetAssetImportApi(const EngineAssetImportApi& api) { assetImport_ = api; }
    const EngineAssetImportApi* AssetImportApi() const { return assetImport_.importWithOptions ? &assetImport_ : nullptr; }
    void InitializeRecovery();
    void SetBatchApi(const EngineSceneBatchApi& api) { batch_ = api; }
    void SetAuthoringApi(const EngineAuthoringApi& api) { authoring_ = api; }
    void SetScriptApi(const EngineScriptApi& api) { script_ = api; }
    void SetGraphTraceApi(const EngineGraphTraceApi& api) { graphTrace_ = api; }
    void SetAiDiagnosticsApi(const EngineAiDiagnosticsApi& api) { aiDiagnostics_ = api; }
    const EngineAiDiagnosticsApi* AiDiagnosticsApi() const {
        return host_.getService ? static_cast<const EngineAiDiagnosticsApi*>(host_.getService(host_.user, EngineAiDiagnosticsService, 1)) : (aiDiagnostics_.inspect ? &aiDiagnostics_ : nullptr);
    }
    void SetAiMachineApi(const EngineAiMachineApi& api) { aiMachine_ = api; }
    // Автоматы ИИ (.fsm): каталог, проверка файла, состояние персонажей в Play — из сервиса runtime или заданные тестом.
    const EngineAiMachineApi* AiMachineApi() const {
        if (host_.getService) return static_cast<const EngineAiMachineApi*>(host_.getService(host_.user, EngineAiMachineService, 1));
        return aiMachine_.check ? &aiMachine_ : nullptr;
    }
    // Состояние нод графов в Play (отладка): из сервиса runtime или заданное тестом.
    const EngineGraphTraceApi* GraphTraceApi() const {
        if (host_.getService) return static_cast<const EngineGraphTraceApi*>(host_.getService(host_.user, EngineGraphTraceService, 1));
        return graphTrace_.read ? &graphTrace_ : nullptr;
    }
    // Визуальные скрипты: из сервиса runtime (перезапрашивается после hot reload) или заданные тестом.
    const EngineScriptApi* ScriptApi() const {
        if (host_.getService) return static_cast<const EngineScriptApi*>(host_.getService(host_.user, EngineScriptService, 1));
        return script_.set ? &script_ : nullptr;
    }
    void SetViewportOverlayApi(const EngineViewportOverlayApi& api);
    void SetAntialiasing(uint32_t mode);
    uint64_t ViewportResizeCount() const;

private:
    void UiThreadMain();
    void DrainCommands();
    void Complete(const EditorCommand& command, uint32_t status, const QString& error = {});

    EngineModuleHost host_;
    EngineEditSessionApi scene_{};
    EngineProjectDataApi projectData_{};
    EngineSelectionApi selection_{};
    EngineSceneBatchApi batch_{};
    EngineAuthoringApi authoring_{};
    EngineScriptApi script_{};
    EngineGraphTraceApi graphTrace_{};
    EngineAiMachineApi aiMachine_{};
    EngineAiDiagnosticsApi aiDiagnostics_{};
    const EngineSceneBatchApi* BatchApi() const {
        if (host_.getService) return static_cast<const EngineSceneBatchApi*>(host_.getService(host_.user, EngineEditorBatchService, 1));
        return batch_.apply ? &batch_ : nullptr;
    }
    const EngineAuthoringApi* AuthoringApi() const {
        if (host_.getService) return static_cast<const EngineAuthoringApi*>(host_.getService(host_.user, EngineAuthoringService, 1));
        return authoring_.describe ? &authoring_ : nullptr;
    }
    const EnginePointLightV2Api* PointLightV2Api() const {
        if (host_.getService) return static_cast<const EnginePointLightV2Api*>(host_.getService(host_.user, EnginePointLightV2Service, 1));
        return nullptr;
    }
    RecoveryStore recovery_;
    std::chrono::steady_clock::time_point recoveryTime_{};
    void WriteRecovery();
    EngineViewportCursorApi cursor_{};
    const EngineViewportCursorApi* CursorApi() const {
        if(host_.getService){const auto* api=static_cast<const EngineViewportCursorApi*>(host_.getService(host_.user,EngineViewportCursorService,1));return api&&api->version==1&&api->set?api:nullptr;}
        return cursor_.set?&cursor_:nullptr;
    }
    EngineViewportInputApi viewportInput_{};
    bool viewportFocused_ = false;
    EngineViewportOverlayApi overlay_{};
    EngineAudioPreviewApi audio_{};
    EnginePhysicsApi physics_{};
    EnginePhysicsObjectApi physicsObject_{};
    EngineAssetImportApi assetImport_{};
    uint32_t antialiasingMode_ = 1;

    struct PendingIcon
    {
        uint32_t id = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> pixels;
    };

    mutable std::mutex cameraMutex_;
    EngineCameraInfo lastCamera_{};
    bool lastCameraValid_ = false;

    std::mutex overlayMutex_;
    std::vector<EngineOverlayLine> pendingLines_;
    float pendingThickness_ = 1.0f;
    bool pendingLinesDirty_ = false;

    std::vector<EngineOverlayBillboard> pendingBillboards_;
    std::vector<EngineOverlayTriangle> pendingTriangles_;
    bool pendingBillboardsDirty_ = false;
    bool pendingTrianglesDirty_ = false;

    std::vector<PendingIcon> pendingIcons_;
    bool iconsDirty_ = false;
    std::atomic<bool> canUndo_{false}, canRedo_{false};
    void InvalidateOverlays();

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
