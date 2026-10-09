#include "MainWindow.h"
#include "QtHost.h"
#include "FileBrowser.h"
#include <QDockWidget>
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
    setWindowTitle(QStringLiteral("Engine Editor"));
    resize(1280, 720);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    const EngineModuleHost& h = host_->host();
    layout->addWidget(new QLabel(
        QStringLiteral("Project: %1\nFile: %2")
            .arg(QString::fromUtf8(h.projectName ? h.projectName : "-"),
                 QString::fromUtf8(h.projectFile ? h.projectFile : "-")), central));

    auto* exitBtn = new QPushButton(QStringLiteral("Request engine exit (host.requestExit)"), central);
    connect(exitBtn, &QPushButton::clicked, this, [this] {
        EditorCommand cmd; cmd.type = EditorCommand::RequestExit;
        host_->PostCommand(cmd);
    });
    layout->addWidget(exitBtn);
    layout->addStretch(1);
    setCentralWidget(central);

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
    file->addAction(QStringLiteral("Reload project (stub)"), this, [this] {
        EditorCommand cmd; cmd.type = EditorCommand::ReloadProject;
        host_->PostCommand(cmd);
    });
    file->addSeparator();
    file->addAction(QStringLiteral("Close editor"), this, [this] { close(); });
}

void EditorMainWindow::AppendLine(const QString& line) { console_->appendPlainText(line); }

void EditorMainWindow::RefreshStats()
{
    const double dt = host_->LastDt();
    statsLabel_->setText(QStringLiteral("updates: %1 | fps: %2")
                             .arg(host_->UpdateCount())
                             .arg(dt > 0.0 ? 1.0 / dt : 0.0, 0, 'f', 1));
}