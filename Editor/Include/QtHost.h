#pragma once
#include <Engine/Runtime/ModuleApi.h>
#include <Engine/Runtime/EditSessionApi.h>
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <QString>
#include <thread>

class EditorMainWindow;

struct EditorCommand
{
    enum Type : uint32_t
    {
        None = 0, RequestExit, ReloadProject, LoadAsset,
        ViewportAttach, ViewportResize, ViewportDetach, ViewportCamera,
        TogglePlay, TogglePause,
        FetchScene, UpdateObjectTransform // Новые команды
    };

    Type type = None;
    float right = 0, up = 0, forward = 0, yaw = 0, pitch = 0, zoom = 0;
    QString path;
    void* hwnd = nullptr;
    uint32_t w = 0, h = 0;

    // Поля для манипуляций с объектами
    uint64_t objectId = 0;
    EngineSceneTransform transform;
};

class EditorQtHost
{
public:
    explicit EditorQtHost(const EngineModuleHost& host);
    ~EditorQtHost();
    EditorQtHost(const EditorQtHost&) = delete;
    EditorQtHost& operator=(const EditorQtHost&) = delete;

    enum EditorState:int
    {
        Edit = 0,
        Play = 1,
        Pause = 2
    };

    bool Start(); // поток движка
    void Update(double dt); // поток движка
    void Overlay()
    {
    } // поток движка
    void Stop(); // поток движка

    void Log(const char* text); // любой поток
    void PostCommand(const EditorCommand& c); // поток UI
    double LastDt() const { return lastDt_.load(); }
    uint64_t UpdateCount() const { return updates_.load(); }
    const EngineModuleHost& host() const { return host_; }
    const EngineEditSessionApi& scene() const { return scene_; }

    bool HasViewport() const { return host_.viewportAttach != nullptr; }
    bool ViewportActive() const { return viewportActive_.load(); }
    EditorState PlayState() const { return playState_.load(); }

    void SetModuleApi(const EngineEditSessionApi& scene) { scene_ = scene; }

    // --- Методы для работы с объектами сцены ---
    void FetchSceneAndSelectNext();
    void MoveSelectedObject(float right, float up, float forward);
    bool HasSelection() const { return hasSelection_.load(); }
    std::string GetSelectedObjectName();

private:
    void UiThreadMain();
    void DrainCommands();

    // Визитор для C-API (статический метод для совместимости с calling convention)
    struct SceneObject
    {
        uint64_t id;
        std::string name;
        EngineSceneTransform transform;
    };

    static void ENGINE_MODULE_CALL SceneVisitor(void* ctx, const EngineSceneObjectInfo* info);

    EngineModuleHost host_;
    EngineEditSessionApi scene_;
    std::mutex logMutex_;
    std::thread uiThread_;
    std::atomic<bool> uiReady_{false};
    std::mutex windowMutex_;
    EditorMainWindow* window_ = nullptr;
    std::mutex commandMutex_;
    std::deque<EditorCommand> commands_;
    std::atomic<double> lastDt_{0.0};
    std::atomic<uint64_t> updates_{0};
    std::atomic<bool> viewportActive_{false};
    std::atomic<EditorState> playState_{Edit};

    // Состояние сцены
    std::vector<SceneObject> sceneObjects_;
    int selectedObjectIndex_ = -1;
    std::atomic<bool> hasSelection_{false};
    std::mutex sceneMutex_;
};
