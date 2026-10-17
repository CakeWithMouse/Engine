#include "MainWindow.h"
#include "QtHost.h"
#include "FileBrowser.h"
#include "ViewportWidget.h" 

#include <QDockWidget>
#include <QCloseEvent>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

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
    file->addAction(QStringLiteral("Request engine exit"), this, [this] {
        EditorCommand cmd; cmd.type = EditorCommand::RequestExit; host_->PostCommand(cmd);
    });
    file->addAction(QStringLiteral("Reload project (stub)"), this, [this] {
        EditorCommand cmd; cmd.type = EditorCommand::ReloadProject; host_->PostCommand(cmd);
    });
    file->addSeparator();
    file->addAction(QStringLiteral("Close editor"), this, [this] { close(); });

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

void EditorMainWindow::RefreshStats()
{
    const double dt = host_->LastDt();
    statsLabel_->setText(QStringLiteral("%4 | Space: Play/Stop | P: Pause/Resume | updates: %1 | fps: %2 | vp: %3")
                             .arg(host_->UpdateCount())
                             .arg(dt > 0.0 ? 1.0 / dt : 0.0, 0, 'f', 1)
                             .arg(host_->ViewportActive() ? QStringLiteral("on") : QStringLiteral("off"))
                             .arg(host_->PlayState() == 0 ? QStringLiteral("Edit") : host_->PlayState() == 1 ? QStringLiteral("Play") : QStringLiteral("Pause")));
}
