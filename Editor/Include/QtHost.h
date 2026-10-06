#pragma once
#include <Engine/Runtime/ModuleApi.h>
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

class EditorMainWindow;

enum class EditorCommand : uint32_t { None = 0, RequestExit, ReloadProject };

// Qt-часть редактора: отдельный поток со своим QApplication.
// Движок -> UI: queued-слоты/атомики. UI -> движок: очередь команд (дрейнится в Update).
class EditorQtHost
{
public:
    explicit EditorQtHost(const EngineModuleHost& host);
    ~EditorQtHost();
    EditorQtHost(const EditorQtHost&) = delete;
    EditorQtHost& operator=(const EditorQtHost&) = delete;

    bool Start();               // поток движка
    void Update(double dt);     // поток движка
    void Overlay() {}           // поток движка
    void Stop();                // поток движка

    void Log(const char* text);         // любой поток
    void PostCommand(EditorCommand c);  // поток UI
    double LastDt() const { return lastDt_.load(); }
    uint64_t UpdateCount() const { return updates_.load(); }
    const EngineModuleHost& host() const { return host_; }

private:
    void UiThreadMain();
    void DrainCommands();
    EditorMainWindow* GetWindow();

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
};