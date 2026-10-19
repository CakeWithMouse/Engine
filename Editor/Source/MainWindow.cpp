#include "MainWindow.h"
#include "QtHost.h"
#include "FileBrowser.h"
#include "ViewportWidget.h"

#include <QDockWidget>
#include <QCloseEvent>
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QToolBar>
#include <QProcess>

EditorMainWindow::EditorMainWindow(EditorQtHost* host, QWidget* parent)
    : QMainWindow(parent), host_(host)
{
    BuildUi();
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &EditorMainWindow::RefreshStats);
    timer->start(250);
}

void EditorMainWindow::BuildUi()
{
    setWindowTitle(QStringLiteral("Engine Editor — %1").arg(QString::fromUtf8(host_->host().projectName)));
    resize(1280, 720);

    viewport_ = new ViewportWidget(host_, this);
    setCentralWidget(viewport_);

    console_ = new QPlainTextEdit(this);
    console_->setReadOnly(true);
    console_->setMaximumBlockCount(2000);

    consoleDock_ = new QDockWidget(QStringLiteral("Console"), this);
    consoleDock_->setWidget(console_);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock_);

    // Файл-менеджер: вторая нижняя панель справа от консоли.
    // Хотите вкладки вместо бок-о-бок: tabifyDockWidget(consoleDock_, fileDock_);
    fileDock_ = new FileBrowserDock(host_, this);
    addDockWidget(Qt::BottomDockWidgetArea, fileDock_);
    resizeDocks({consoleDock_, fileDock_}, {420, 420}, Qt::Horizontal);

    statsLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(statsLabel_);

    QMenu* file = menuBar()->addMenu(QStringLiteral("&File"));
    file->addAction(QStringLiteral("Request engine exit"), this, [this]
    {
        EditorCommand cmd;
        cmd.type = EditorCommand::RequestExit;
        host_->PostCommand(cmd);
    });
    file->addAction(QStringLiteral("Reload project (stub)"), this, [this]
    {
        EditorCommand cmd;
        cmd.type = EditorCommand::ReloadProject;
        host_->PostCommand(cmd);
    });
    file->addSeparator();
    file->addAction(QStringLiteral("Close editor"), this, [this] { close(); });

    BuildToolBar();
    playStateLabel_ = new QLabel(QStringLiteral("EDIT"), this);
    statusBar()->addPermanentWidget(playStateLabel_);
    UpdatePlayButtons();

    const EngineModuleHost& h = host_->host();
    statusBar()->addWidget(new QLabel(
        QStringLiteral("%1 | %2").arg(
            QString::fromUtf8(h.projectName ? h.projectName : "-"),
            QString::fromUtf8(h.projectFile ? h.projectFile : "-")), this));
}

void EditorMainWindow::AppendLine(const QString& line) { console_->appendPlainText(line); }

void EditorMainWindow::closeEvent(QCloseEvent* event)
{
    // Keep the HWND alive and Qt pumping messages until the render thread releases DXGI.
    // Stop() then quits Qt and destroys the window. Never join the render thread from here.
    event->ignore();
    EditorCommand command;
    command.type = EditorCommand::RequestExit;
    host_->PostCommand(command);
}

void EditorMainWindow::BuildToolBar()
{
    toolBar_ = addToolBar(QStringLiteral("Play"));
    toolBar_->setMovable(false);

    actPlay_ = toolBar_->addAction(QStringLiteral("▶ Play"));
    actPause_ = toolBar_->addAction(QStringLiteral("⏸ Pause"));
    actStop_ = toolBar_->addAction(QStringLiteral("⏹ Stop"));
    toolBar_->addSeparator();
    actExternal_ = toolBar_->addAction(QStringLiteral("🚀 External"));

    connect(actPlay_, &QAction::triggered, this, [this]
    {
        EditorCommand c;
        const int m = host_->PlayState();
        c.type = m == 0 ? EditorCommand::TogglePlay : EditorCommand::TogglePause;
        host_->PostCommand(c);
    });
    connect(actPause_, &QAction::triggered, this, [this]
    {
        // toggle Pause/Resume
        EditorCommand c;
        c.type = EditorCommand::TogglePause;
        host_->PostCommand(c);
    });
    connect(actStop_, &QAction::triggered, this, [this]
    {
        EditorCommand c;
        c.type = EditorCommand::TogglePlay;
        host_->PostCommand(c);
    });
    connect(actExternal_, &QAction::triggered, this, &EditorMainWindow::StartExternalGame);

    // режимы доступны только если движок поддерживает setPlayMode
    const bool canPlay = host_->PlayState() == 1;
    actPlay_->setEnabled(canPlay);
    actPause_->setEnabled(canPlay);
    actStop_->setEnabled(canPlay);
}

void EditorMainWindow::UpdatePlayButtons()
{
    const int m = host_->PlayState();
    actPlay_->setEnabled(m == 0 || m != 1);
    actPause_->setEnabled(m == 1);
    actStop_->setEnabled(m != 0);
}

void EditorMainWindow::StartExternalGame()
{
    if (gameProc_ && gameProc_->state() != QProcess::NotRunning)
        return;
    if (!gameProc_)
    {
        gameProc_ = new QProcess(this);
        // EngineRuntime — консольное приложение (std::cin/std::puts), поэтому MergedChannels
        // реально ловит его stdout/stderr. Лаунчер — GUI (wWinMain), у него вывода нет:
        // ещё одна причина не запускать его отсюда.
        gameProc_->setProcessChannelMode(QProcess::MergedChannels);
        connect(gameProc_, &QProcess::readyReadStandardOutput, this, [this]
        {
            host_->Log(gameProc_->readAllStandardOutput().trimmed().constData());
        });
        connect(gameProc_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st)
        {
            const QByteArray tail = gameProc_->readAllStandardOutput();
            if (!tail.isEmpty()) host_->Log(tail.trimmed().constData());
            host_->Log(("ExternalGame exited code=" + std::to_string(code)
                + (st == QProcess::CrashExit ? " (crash)" : "")).c_str());
            actExternal_->setText(QStringLiteral("🚀 External"));
        });
        connect(gameProc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e)
        {
            host_->Log(("ExternalGame error=" + std::to_string(e)).c_str());
            actExternal_->setText(QStringLiteral("🚀 External"));
        });
    }

    const QString runtimeDir = QCoreApplication::applicationDirPath(); // .../Runtime
    // Запускаем РАНТАЙМ, а не лаунчер. Именно его порождает лаунчер в Launch() для editor=false.
    const QString runtime = QDir(runtimeDir).absoluteFilePath(QStringLiteral("EngineRuntime.exe"));
    const QString project = QString::fromUtf8(host_->host().projectFile ? host_->host().projectFile : "");

    if (!QFileInfo::exists(runtime))
    {
        host_->Log(("ExternalGame: runtime not found: " + runtime.toStdString()).c_str());
        return;
    }
    if (project.isEmpty())
    {
        host_->Log("ExternalGame: empty projectFile in host");
        return;
    }

    gameProc_->setProgram(runtime);
    // Ровно та строка, что лаунчер собирает для кнопки «Играть»: --mode game, без --no-editor.
    gameProc_->setArguments({QStringLiteral("--project"), project, QStringLiteral("--mode"), QStringLiteral("game")});
    gameProc_->setWorkingDirectory(runtimeDir);

    host_->Log(("ExternalGame start: " + runtime.toStdString()
        + " --project " + project.toStdString() + " --mode game").c_str());

    gameProc_->start();
    if (!gameProc_->waitForStarted(2000))
        host_->Log("ExternalGame: failed to start (check EngineRuntime.exe path)");
    else
        actExternal_->setText(QStringLiteral("🛑 Stop external"));
}

void EditorMainWindow::StopExternalGame()
{
    if (gameProc_ && gameProc_->state() != QProcess::NotRunning)
        gameProc_->kill();
}

void EditorMainWindow::RefreshStats()
{
    const double dt = host_->LastDt();
    UpdatePlayButtons();
    statsLabel_->setText(QStringLiteral("%4 | Space: Play/Stop | P: Pause/Resume | updates: %1 | fps: %2 | vp: %3")
                         .arg(host_->UpdateCount())
                         .arg(dt > 0.0 ? 1.0 / dt : 0.0, 0, 'f', 1)
                         .arg(host_->ViewportActive() ? QStringLiteral("on") : QStringLiteral("off"))
                         .arg(host_->PlayState() == 0
                                  ? QStringLiteral("Edit")
                                  : host_->PlayState() == 1
                                  ? QStringLiteral("Play")
                                  : QStringLiteral("Pause")));
}
