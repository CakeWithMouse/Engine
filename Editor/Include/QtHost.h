#pragma once
#include <Engine/Runtime/ModuleApi.h>
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
    };

    Type type = None;
    float right = 0, up = 0, forward = 0, yaw = 0, pitch = 0, zoom = 0;
    QString path;
    void* hwnd = nullptr;
    uint32_t w = 0, h = 0;
};

// Qt-часть редактора: отдельный поток со своим QApplication.
// Движок -> UI: queued-слоты/атомики. UI -> движок: очередь команд (дрейнится в Update).
class EditorQtHost
{
public:
    explicit EditorQtHost(const EngineModuleHost& host);
    ~EditorQtHost();
    EditorQtHost(const EditorQtHost&) = delete;
    EditorQtHost& operator=(const EditorQtHost&) = delete;

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

    bool HasViewport() const { return host_.viewportAttach != nullptr; }
    bool ViewportActive() const { return viewportActive_.load(); }
    int PlayState() const { return playState_.load(); }

private:
    void UiThreadMain();
    void DrainCommands();

    EngineModuleHost host_;
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
    std::atomic<int> playState_{0};
};
