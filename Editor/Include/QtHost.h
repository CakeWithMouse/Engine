#pragma once
#include <Engine/Runtime/ModuleApi.h>
#include <Engine/Runtime/EditSessionApi.h>
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <QString>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>
class QTemporaryDir;

class EditorMainWindow;

struct EditorCommand
{
    enum Type : uint32_t
    {
        None = 0, RequestExit, ReloadProject, LoadAsset,
        ViewportAttach, ViewportResize, ViewportDetach, ViewportCamera,
        TogglePlay, TogglePause,
        FetchScene, UpdateObjectTransform, RefreshScene, SelectObject, CreateObject, RemoveObject,
        RenameObject, DuplicateObject, SaveScene, LoadScene, NewScene, Undo, Redo, PickObject, FocusObject,
        ImportModel, ImportTexture, CreateModel, SetTexture
    };

    Type type = None;
    float right = 0, up = 0, forward = 0, yaw = 0, pitch = 0, zoom = 0;
    QString path;
    void* hwnd = nullptr;
    uint32_t w = 0, h = 0;

    // Поля для манипуляций с объектами
    uint64_t objectId = 0;
    EngineSceneTransform transform{{0,0,0},{0,0,0,1},{1,1,1}};
    QString name;
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

    bool HasViewport() const { return HasScene() || host_.viewportAttach != nullptr; }
    bool ViewportActive() const { return viewportActive_.load(); }
    EditorState PlayState() const { return playState_.load(); }

    void SetModuleApi(const EngineEditSessionApi& scene) { scene_ = scene; sceneAvailable_.store(scene.context != nullptr); }
    bool HasScene() const { return sceneAvailable_.load(); }
    struct SceneObject
    {
        uint64_t id = 0, parent = 0;
        bool inheritScale = true;
        std::string name, type;
        EngineSceneTransform transform{};
        std::string modelAsset, textureAsset;
    };
    struct SceneType { std::string name, label; };
    struct Snapshot
    {
        std::vector<SceneObject> objects;
        std::vector<SceneType> types;
        uint64_t selected = 0, serial = 0;
        bool dirty = false, canUndo = false, canRedo = false;
    };
    Snapshot ReadSnapshot();

    // --- Методы для работы с объектами сцены ---
    void FetchSceneAndSelectNext();
    void MoveSelectedObject(float right, float up, float forward);
    bool HasSelection() const { return hasSelection_.load(); }
    std::string GetSelectedObjectName();

private:
    void UiThreadMain();
    void DrainCommands();

    void PublishScene();
    void Complete(const EditorCommand& command, uint32_t status, const QString& error = {});
    QString Checkpoint();
    void RecordChange(const QString& checkpoint);
    static void ENGINE_MODULE_CALL SceneVisitor(void* ctx, const EngineSceneObjectInfo* info);

    EngineModuleHost host_;
    EngineEditSessionApi scene_{};
    std::atomic<bool> sceneAvailable_{false};
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

    Snapshot snapshot_;
    uint64_t selectedId_ = 0;
    bool dirty_ = false, sceneViewportAttached_ = false;
    std::unique_ptr<QTemporaryDir> historyDirectory_;
    uint64_t historySerial_ = 0, lastTransformId_ = 0;
    std::chrono::steady_clock::time_point lastTransformTime_{};
    std::vector<QString> undo_, redo_;
    std::atomic<bool> hasSelection_{false};
    std::mutex sceneMutex_;
};
