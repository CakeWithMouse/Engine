#include "QtHost.h"
#include "MainWindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QMetaObject>
#include <QString>
#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace
{
// argc/argv должны жить не меньше QApplication — статического хранения достаточно.
int g_argc = 1;
char g_arg0[] = "Editor.dll";
char* g_argv[] = {g_arg0, nullptr};
}

EditorQtHost::EditorQtHost(const EngineModuleHost& host) : host_(host) {}
EditorQtHost::~EditorQtHost() { Stop(); }

bool EditorQtHost::Start()
{
    if (uiThread_.joinable()) return false;
    uiReady_.store(false);
    uiThread_ = std::thread([this] { UiThreadMain(); });
    for (int i = 0; i < 200 && !uiReady_.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    if (!uiReady_.load()) { Stop(); return false; }
    Log("EditorStart");
    Log(host_.projectName ? host_.projectName : "(no project)");
    Log(host_.projectFile ? host_.projectFile : "(no project file)");
    return true;
}

void EditorQtHost::Update(double dt)
{
    updates_.fetch_add(1);
    lastDt_.store(dt);
    DrainCommands();
}

void EditorQtHost::Stop()
{
    if (!uiThread_.joinable()) return;
    if (viewportActive_.exchange(false) && host_.viewportDetach)
        host_.viewportDetach(host_.user);      // поток движка, до уничтожения виджетов
    if (uiReady_.load())
        // quit() asks windows to close and can be vetoed by our closeEvent guard.
        // GPU resources are gone now: exit the UI loop directly, on its own thread.
        QMetaObject::invokeMethod(QCoreApplication::instance(), [] { QCoreApplication::exit(0); }, Qt::QueuedConnection);
    uiThread_.join();
    char buf[96];
    std::snprintf(buf, sizeof(buf), "EditorUpdates=%llu", static_cast<unsigned long long>(updates_.load()));
    Log(buf);
    Log("EditorStop");
}

void EditorQtHost::UiThreadMain()
{
    QApplication app(g_argc, g_argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName(QStringLiteral("Engine Editor"));
    QApplication::setStyle(QStringLiteral("Fusion"));

    EditorMainWindow window(this);
    { std::lock_guard lk(windowMutex_); window_ = &window; }
    window.show();
    uiReady_.store(true);

    app.exec();

    { std::lock_guard lk(windowMutex_); window_ = nullptr; }
}

void EditorQtHost::Log(const char* text)
{
    {
        std::lock_guard lk(logMutex_);
        if (host_.log) host_.log(host_.user, text);
    }

    // Keep the window alive until invokeMethod has queued the call.
    std::lock_guard windowLock(windowMutex_);
    if (EditorMainWindow* w = window_)
    {
        const QString line = QString::fromUtf8(text);
        // типобезопасная форма: аргументы передаются как есть, без Q_ARG
        QMetaObject::invokeMethod(w, &EditorMainWindow::AppendLine, Qt::QueuedConnection, line);
    }
}

void EditorQtHost::PostCommand(const EditorCommand& cmd)
{
    std::lock_guard lk(commandMutex_);
    commands_.push_back(cmd);
}

void EditorQtHost::DrainCommands()
{
    std::deque<EditorCommand> pending;
    {
        std::lock_guard lk(commandMutex_);
        pending.swap(commands_);
    }
    
    int lastResize = -1;
    for (int i = (int)pending.size() - 1; i >= 0; --i)
        if (pending[i].type == EditorCommand::ViewportResize) { lastResize = i; break; }


    for (size_t i = 0; i < pending.size(); ++i)
    {
        const EditorCommand& cmd = pending[i];
        if (cmd.type == EditorCommand::ViewportResize && (int)i != lastResize)
            continue; 
        switch (cmd.type)
        {
        case EditorCommand::TogglePlay:
        case EditorCommand::TogglePause:
            if (viewportActive_.load() && host_.viewportControl)
            {
                const int state = host_.viewportControl(host_.user, cmd.type == EditorCommand::TogglePlay ? 0 : 1);
                if (state >= 0) playState_.store(state);
                else Log("Play control failed; see engine log");
            }
            break;
        case EditorCommand::RequestExit:
            Log("EditorRequestExit");
            if (host_.requestExit) host_.requestExit(host_.user);
            break;
        case EditorCommand::ReloadProject:
            Log("EditorReloadProject (stub)");
            break;
        case EditorCommand::LoadAsset:
            {
                // QString -> UTF-8 уже здесь, на потоке движка; в host.* wchar_t не уходит
                const QByteArray utf8 = cmd.path.toUtf8();
                const std::string msg = "EditorLoadAsset (stub): " + std::string(utf8.constData(), utf8.size());
                Log(msg.c_str());
                break;
            }
        case EditorCommand::ViewportAttach:
            if (host_.viewportAttach && host_.viewportAttach(host_.user, cmd.hwnd, cmd.w, cmd.h))
            { viewportActive_.store(true); playState_.store(0); Log("EditorViewportAttach ok"); }
            else
            {
                Log("EditorViewportAttach failed");
                throw std::runtime_error("Cannot initialize editor viewport");
            }
            break;
        case EditorCommand::ViewportResize:
            if (viewportActive_.load() && host_.viewportResize)
                host_.viewportResize(host_.user, cmd.w, cmd.h);
            break;
        case EditorCommand::ViewportCamera:
            if (viewportActive_.load() && host_.viewportCamera)
                host_.viewportCamera(host_.user, cmd.right, cmd.up, cmd.forward, cmd.yaw, cmd.pitch, cmd.zoom);
            break;
        case EditorCommand::ViewportDetach:
            playState_.store(0);
            if (viewportActive_.exchange(false) && host_.viewportDetach)
                host_.viewportDetach(host_.user);
            Log("EditorViewportDetach");
            break;
        default:
            break;
        }
    }
}
