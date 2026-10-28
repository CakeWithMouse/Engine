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
#include <QPoint>
#include <QFileInfo>
#include <QMessageBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTreeWidget>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>

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
    setWindowTitle(QStringLiteral("Engine Editor — %1")
        .arg(QString::fromUtf8(host_->host().projectName)));

    resize(1280, 720);

    // Чтобы доки могли нормально вкладываться и располагаться рядом
    setDockNestingEnabled(true);

    // Желательно для устойчивого расположения слева/справа
    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    // setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    // setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    viewport_ = new ViewportWidget(host_, this);

    viewportDock_ = new QDockWidget(QStringLiteral("Viewport"), this);
    viewportDock_->setObjectName(QStringLiteral("ViewportDock"));
    viewportDock_->setWidget(viewport_);
    viewportDock_->setFocusProxy(viewport_);
    viewportDock_->setAllowedAreas(Qt::AllDockWidgetAreas);
    viewportDock_->setFeatures(
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable |
        QDockWidget::DockWidgetClosable
    );

    addDockWidget(Qt::TopDockWidgetArea, viewportDock_);
    setCentralWidget(nullptr);

    console_ = new QPlainTextEdit(this);
    console_->setReadOnly(true);
    console_->setMaximumBlockCount(2000);

    consoleDock_ = new QDockWidget(QStringLiteral("Console"), this);
    consoleDock_->setWidget(console_);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock_);

    fileDock_ = new FileBrowserDock(host_, this);
    addDockWidget(Qt::BottomDockWidgetArea, fileDock_);
    tabifyDockWidget(consoleDock_, fileDock_);

    resizeDocks({viewportDock_, consoleDock_}, {620, 100}, Qt::Vertical);

    BuildToolBar();

    // Создаём пустые доки "Сцена / примитивы" и "Свойства объекта"
    BuildSceneDocks();

    resizeDocks({viewportDock_, consoleDock_}, {620, 100}, Qt::Vertical);

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("Вид"));

    if (viewportDock_)
        viewMenu->addAction(viewportDock_->toggleViewAction());

    if (consoleDock_)
        viewMenu->addAction(consoleDock_->toggleViewAction());

    if (fileDock_)
        viewMenu->addAction(fileDock_->toggleViewAction());

    if (sceneToolsDock_)
        viewMenu->addAction(sceneToolsDock_->toggleViewAction());

    if (inspectorDock_)
        viewMenu->addAction(inspectorDock_->toggleViewAction());

    statsLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(statsLabel_);

    // Создаёт меню "Сцена", читает startupScene, создаёт содержимое панелей
    BuildSceneTools();
    BuildCodeTools();
    for (const auto& title : {QStringLiteral("Сцена"),QStringLiteral("Правка"),QStringLiteral("Запуск"),QStringLiteral("Проект"),QStringLiteral("Вид")})
        for (auto* action : menuBar()->actions()) if (action->text() == title) { menuBar()->removeAction(action); menuBar()->addAction(action); break; }

    // Подключаем сигналы уже в MainWindow.cpp
    ConnectSceneTools();

    connect(viewport_, &ViewportWidget::contextMenuRequested,
            this, &EditorMainWindow::ShowViewportContextMenu);

    playStateLabel_ = new QLabel(QStringLiteral("EDIT"), this);
    statusBar()->addPermanentWidget(playStateLabel_);

    UpdatePlayButtons();

    const EngineModuleHost& h = host_->host();

    statusBar()->addWidget(new QLabel(
        QStringLiteral("%1 | %2").arg(
            QString::fromUtf8(h.projectName ? h.projectName : "-"),
            QString::fromUtf8(h.projectFile ? h.projectFile : "-")), this));

    if (host_->HasScene())
        RefreshSceneTools();
}

void EditorMainWindow::AppendLine(const QString& line) { console_->appendPlainText(line); }

void EditorMainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    if (closing_ || sceneBusy_ || checkingChanges_) return;
    closing_ = true;
    // Queue a barrier after viewport edits, then inspect the authoritative dirty state.
    if (host_->HasScene())
    {
        EditorCommand c;
        c.type = EditorCommand::RefreshScene;
        c.name = QStringLiteral("close");
        host_->PostCommand(c);
    }
    else
    {
        EditorCommand c;
        c.type = EditorCommand::RequestExit;
        host_->PostCommand(c);
    }
}

void EditorMainWindow::BuildToolBar()
{
    toolBar_ = addToolBar(QStringLiteral("Play"));
    toolBar_->setMovable(false);

    actPlay_ = toolBar_->addAction(QStringLiteral("▶ Play"));
    actPause_ = toolBar_->addAction(QStringLiteral("⏸ Pause"));
    actStop_ = toolBar_->addAction(QStringLiteral("⏹ Stop"));
    actExternal_ = new QAction(QStringLiteral("Запустить отдельным окном"), this);

    connect(actPlay_, &QAction::triggered, this, [this]
    {
        EditorCommand c;
        EditorQtHost::EditorState m = host_->PlayState();
        c.type = m == EditorQtHost::EditorState::Edit ? EditorCommand::TogglePlay : EditorCommand::TogglePause;
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
    connect(actExternal_, &QAction::triggered, this, [this]
    {
        if (gameProc_ && gameProc_->state() != QProcess::NotRunning)
        {
            StopExternalGame();
            return;
        }
        if (host_->HasScene()) SaveScene(false, [this] { StartExternalGame(); });
        else StartExternalGame();
    });

    // режимы доступны только если движок поддерживает setPlayMode
    const bool canPlay = host_->PlayState() == 1;
    actPlay_->setEnabled(canPlay);
    actPause_->setEnabled(canPlay);
    actStop_->setEnabled(canPlay);
}

void EditorMainWindow::UpdatePlayButtons()
{
    const EditorQtHost::EditorState m = host_->PlayState();
    actExternal_->setEnabled(!sceneBusy_);
    actPlay_->setEnabled(
        !sceneBusy_ && (m == EditorQtHost::EditorState::Edit || m == EditorQtHost::EditorState::Pause));
    actPause_->setEnabled(m == EditorQtHost::EditorState::Play);
    actStop_->setEnabled(m != EditorQtHost::EditorState::Edit);
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
            actExternal_->setText(QStringLiteral("Запустить отдельным окном"));
        });
        connect(gameProc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e)
        {
            host_->Log(("ExternalGame error=" + std::to_string(e)).c_str());
            actExternal_->setText(QStringLiteral("Запустить отдельным окном"));
        });
    }

    const QString runtimeDir = QCoreApplication::applicationDirPath(); // .../Runtime
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
        actExternal_->setText(QStringLiteral("Остановить отдельную игру"));
}

void EditorMainWindow::StopExternalGame()
{
    if (gameProc_ && gameProc_->state() != QProcess::NotRunning)
        gameProc_->kill();
}

void EditorMainWindow::RefreshStats()
{
    RefreshSceneTools();
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

void EditorMainWindow::ShowViewportContextMenu(const QPoint& globalPos)
{
    UpdatePlayButtons();

    QMenu menu(this);

    if (actPlay_)
        menu.addAction(actPlay_);

    if (actPause_)
        menu.addAction(actPause_);

    if (actStop_)
        menu.addAction(actStop_);

    menu.addSeparator();

    if (duplicateAction_)
        menu.addAction(duplicateAction_);

    if (deleteAction_)
        menu.addAction(deleteAction_);

    if (saveAction_)
        menu.addAction(saveAction_);

    if (actExternal_)
        menu.addAction(actExternal_);

    menu.addSeparator();

    if (viewportDock_)
        menu.addAction(viewportDock_->toggleViewAction());

    if (consoleDock_)
        menu.addAction(consoleDock_->toggleViewAction());

    if (fileDock_)
        menu.addAction(fileDock_->toggleViewAction());

    if (sceneToolsDock_)
        menu.addAction(sceneToolsDock_->toggleViewAction());

    if (inspectorDock_)
        menu.addAction(inspectorDock_->toggleViewAction());

    menu.exec(globalPos);
}

void EditorMainWindow::BuildSceneDocks()
{
    sceneToolsDock_ = new QDockWidget(QStringLiteral("Сцена / объекты"), this);
    sceneToolsDock_->setObjectName(QStringLiteral("SceneToolsDock"));
    sceneToolsDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    sceneToolsDock_->setFeatures(
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable |
        QDockWidget::DockWidgetClosable
    );

    inspectorDock_ = new QDockWidget(QStringLiteral("Свойства объекта"), this);
    inspectorDock_->setObjectName(QStringLiteral("InspectorDock"));
    inspectorDock_->setMinimumWidth(300);
    inspectorDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    inspectorDock_->setFeatures(
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable |
        QDockWidget::DockWidgetClosable
    );

    addDockWidget(Qt::LeftDockWidgetArea, sceneToolsDock_);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);

    // Если нужно явно разложить их по горизонтали рядом с вьюпортом:
    if (viewportDock_)
    {
        // sceneToolsDock_ слева, viewportDock_ по центру/сверху,
        // inspectorDock_ справа.
        splitDockWidget(sceneToolsDock_, viewportDock_, Qt::Horizontal);
        splitDockWidget(viewportDock_, inspectorDock_, Qt::Horizontal);

        resizeDocks(
            {sceneToolsDock_, viewportDock_, inspectorDock_},
            {250, 700, 320},
            Qt::Horizontal
        );
    }
    else
    {
        resizeDocks(
            {sceneToolsDock_, inspectorDock_},
            {280, 300},
            Qt::Horizontal
        );
    }
}

void EditorMainWindow::ConnectSceneTools()
{
    if (!scenePanel_ || !inspector_)
        return;

    if (addPrimitiveButton_ && primitive_)
    {
        connect(addPrimitiveButton_, &QPushButton::clicked, this, [this]
        {
            if (sceneBusy_ ||
                host_->PlayState() != EditorQtHost::EditorState::Edit ||
                primitive_->currentIndex() < 0)
            {
                return;
            }

            EditorCommand c;
            c.type = EditorCommand::CreateObject;
            c.path = primitive_->currentData().toString();
            c.name = primitive_->currentText();

            for (int i = 0; i < 3; ++i)
                c.transform.position[i] = static_cast<float>(spawn_[i]->value());

            host_->PostCommand(c);
        });
    }

    if (hierarchy_)
    {
        connect(hierarchy_, &QTreeWidget::itemSelectionChanged, this, [this]
        {
            if (refreshing_)
                return;

            const auto items = hierarchy_->selectedItems();

            EditorCommand c;
            c.type = EditorCommand::SelectObject;
            c.objectId = items.isEmpty()
                             ? 0
                             : items.front()->data(0, Qt::UserRole).toULongLong();

            host_->PostCommand(c);
        });

        connect(hierarchy_, &QTreeWidget::itemDoubleClicked, this,
                [this](QTreeWidgetItem*, int)
                {
                    EditorCommand c;
                    c.type = EditorCommand::FocusObject;
                    host_->PostCommand(c);
                });
    }

    if (duplicateObjectButton_)
    {
        connect(duplicateObjectButton_, &QPushButton::clicked, this, [this]
        {
            SubmitObjectCommand(EditorCommand::DuplicateObject);
        });
    }

    if (removeObjectButton_)
    {
        connect(removeObjectButton_, &QPushButton::clicked, this, [this]
        {
            SubmitObjectCommand(EditorCommand::RemoveObject);
        });
    }

    if (removeShortcut_)
    {
        connect(removeShortcut_, &QAction::triggered, this, [this]
        {
            SubmitObjectCommand(EditorCommand::RemoveObject);
        });
    }

    if (objectName_)
    {
        connect(objectName_, &QLineEdit::editingFinished, this, [this]
        {
            if (refreshing_ ||
                !selectedId_ ||
                sceneBusy_ ||
                host_->PlayState() != EditorQtHost::EditorState::Edit)
            {
                return;
            }

            const auto name = objectName_->text().trimmed();

            for (const auto& o : host_->ReadSnapshot().objects)
            {
                if (o.id == selectedId_ && name == QString::fromStdString(o.name))
                    return;
            }

            EditorCommand c;
            c.type = EditorCommand::RenameObject;
            c.objectId = selectedId_;
            c.name = name;

            host_->PostCommand(c);
        });
    }

    for (int i = 0; i < 9; ++i)
    {
        if (transform_[i])
        {
            connect(transform_[i], &QDoubleSpinBox::editingFinished,
                    this, &EditorMainWindow::ApplyTransform);
        }
    }
}
