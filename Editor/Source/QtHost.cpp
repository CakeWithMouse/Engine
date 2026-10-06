#include "QtHost.h"
#include "MainWindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QMetaObject>
#include <QString>
#include <chrono>
#include <cstdio>

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
    if (uiReady_.load())
        QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
    uiThread_.join();
    char buf[96];
    std::snprintf(buf, sizeof(buf), "EditorUpdates=%llu", static_cast<unsigned long long>(updates_.load()));
    Log(buf);
    Log("EditorStop");
}

void EditorQtHost::UiThreadMain()
{
    QApplication app(g_argc, g_argv);
    app.setApplicationName(QStringLiteral("Engine Editor"));
    QApplication::setStyle(QStringLiteral("Fusion"));

    EditorMainWindow window(this);
    { std::lock_guard lk(windowMutex_); window_ = &window; }
    window.show();
    uiReady_.store(true);

    app.exec();

    { std::lock_guard lk(windowMutex_); window_ = nullptr; }
}

EditorMainWindow* EditorQtHost::GetWindow()
{
    std::lock_guard lk(windowMutex_);
    return window_;
}

void EditorQtHost::Log(const char* text)
{
    {
        std::lock_guard lk(logMutex_);
        if (host_.log) host_.log(host_.user, text);
    }

    if (EditorMainWindow* w = GetWindow())
    {
        const QString line = QString::fromUtf8(text);
        // типобезопасная форма: аргументы передаются как есть, без Q_ARG
        QMetaObject::invokeMethod(w, &EditorMainWindow::AppendLine, Qt::QueuedConnection, line);
    }
}

void EditorQtHost::PostCommand(EditorCommand cmd)
{
    std::lock_guard lk(commandMutex_);
    commands_.push_back(cmd);
}

void EditorQtHost::DrainCommands()
{
    std::deque<EditorCommand> pending;
    { std::lock_guard lk(commandMutex_); pending.swap(commands_); }
    for (EditorCommand c : pending)
    {
        switch (c)
        {
        case EditorCommand::RequestExit:
            Log("EditorRequestExit");
            if (host_.requestExit) host_.requestExit(host_.user);
            break;
        case EditorCommand::ReloadProject:
            Log("EditorReloadProject (stub)");
            break;
        default: break;
        }
    }
}