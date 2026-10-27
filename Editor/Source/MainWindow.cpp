#include "MainWindow.h"
#include "QtHost.h"
#include "FileBrowser.h"
#include "ViewportWidget.h"
#include "SceneObjectsPanel.h"
#include "ObjectInspectorPanel.h"

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
#include <QFileDialog>
#include <QLineEdit>
#include <QTreeWidget>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonDocument>
#include <QQuaternion>
#include <QSaveFile>
#include <QColorDialog>

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

    setDockNestingEnabled(true);

    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);

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

    BuildSceneDocks();

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));

    if (viewportDock_)
        viewMenu->addAction(viewportDock_->toggleViewAction());

    if (consoleDock_)
        viewMenu->addAction(consoleDock_->toggleViewAction());

    if (fileDock_)
        viewMenu->addAction(fileDock_->toggleViewAction());

    if (sceneObjectsDock_)
        viewMenu->addAction(sceneObjectsDock_->toggleViewAction());

    if (inspectorDock_)
        viewMenu->addAction(inspectorDock_->toggleViewAction());

    statsLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(statsLabel_);

    QMenu* file = menuBar()->addMenu(QStringLiteral("&File"));
    file->addSeparator();
    file->addAction(QStringLiteral("Close editor"), this, [this]
    {
        close();
    });

    BuildSceneMenu();

    connect(viewport_, &ViewportWidget::contextMenuRequested,
            this, &EditorMainWindow::ShowViewportContextMenu);

    playStateLabel_ = new QLabel(QStringLiteral("EDIT"), this);
    statusBar()->addPermanentWidget(playStateLabel_);

    UpdatePlayButtons();

    const EngineModuleHost& h = host_->host();

    statusBar()->addWidget(new QLabel(
        QStringLiteral("%1 | %2").arg(
            QString::fromUtf8(h.projectName ? h.projectName : "-"),
            QString::fromUtf8(h.projectFile ? h.projectFile : "-")
        ),
        this
    ));

    if (host_->HasScene())
        RefreshSceneUi();
}

void EditorMainWindow::AppendLine(const QString& line) { console_->appendPlainText(line); }

void EditorMainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    if (closing_ || sceneBusy_ || checkingChanges_) return;
    if (!CloseMaterialEditors()) return;
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

void EditorMainWindow::BuildSceneMenu()
{
    auto* menu = menuBar()->addMenu(QStringLiteral("Сцена"));

    const bool available = host_->HasScene();
    menu->setEnabled(available);

    menu->addAction(QStringLiteral("Новая сцена"), this, [this]
    {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
            return;

        MaybeSave([this]
        {
            sceneBusy_ = true;

            EditorCommand c;
            c.type = EditorCommand::NewScene;
            host_->PostCommand(c);
        });
    }, QKeySequence::New);

    menu->addAction(QStringLiteral("Открыть сцену…"), this, [this]
    {
        OpenScene();
    }, QKeySequence::Open);

    saveAction_ = menu->addAction(QStringLiteral("Сохранить и сделать стартовой"), this, [this]
    {
        SaveScene();
    }, QKeySequence::Save);

    menu->addAction(QStringLiteral("Сохранить как…"), this, [this]
    {
        SaveScene(true);
    }, QKeySequence::SaveAs);

    menu->addSeparator();

    menu->addAction(QStringLiteral("Добавить модель…"), this, [this]
    {
        AddModel();
    });

    menu->addAction(QStringLiteral("Импорт модели…"), this, [this]
    {
        ImportAsset(false);
    });

    menu->addAction(QStringLiteral("Импорт текстуры…"), this, [this]
    {
        ImportAsset(true);
    });

    menu->addSeparator();

    undoAction_ = menu->addAction(QStringLiteral("Отменить"), this, [this]
    {
        SubmitObjectCommand(EditorCommand::Undo);
    }, QKeySequence::Undo);

    redoAction_ = menu->addAction(QStringLiteral("Повторить"), this, [this]
    {
        SubmitObjectCommand(EditorCommand::Redo);
    }, QKeySequence::Redo);

    duplicateAction_ = menu->addAction(QStringLiteral("Дублировать объект"), this, [this]
    {
        SubmitObjectCommand(EditorCommand::DuplicateObject);
    }, QKeySequence(QStringLiteral("Ctrl+D")));

    deleteAction_ = menu->addAction(QStringLiteral("Удалить объект"), this, [this]
    {
        SubmitObjectCommand(EditorCommand::RemoveObject);
    });
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
            actExternal_->setText(QStringLiteral("🚀 External"));
        });
        connect(gameProc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e)
        {
            host_->Log(("ExternalGame error=" + std::to_string(e)).c_str());
            actExternal_->setText(QStringLiteral("🚀 External"));
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
        actExternal_->setText(QStringLiteral("🛑 Stop external"));
}

void EditorMainWindow::StopExternalGame()
{
    if (gameProc_ && gameProc_->state() != QProcess::NotRunning)
        gameProc_->kill();
}

void EditorMainWindow::RefreshStats()
{
    RefreshSceneUi();

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
                                  : QStringLiteral("Pause"))
    );
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

    if (sceneObjectsDock_)
        menu.addAction(sceneObjectsDock_->toggleViewAction());

    if (inspectorDock_)
        menu.addAction(inspectorDock_->toggleViewAction());

    menu.exec(globalPos);
}

void EditorMainWindow::BuildSceneDocks()
{
    sceneObjectsDock_ = new QDockWidget(QStringLiteral("Сцена / объекты"), this);
    sceneObjectsDock_->setObjectName(QStringLiteral("SceneObjectsDock"));
    sceneObjectsDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    sceneObjectsDock_->setFeatures(
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable |
        QDockWidget::DockWidgetClosable
    );

    inspectorDock_ = new QDockWidget(QStringLiteral("Свойства объекта"), this);
    inspectorDock_->setObjectName(QStringLiteral("InspectorDock"));
    inspectorDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    inspectorDock_->setFeatures(
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable |
        QDockWidget::DockWidgetClosable
    );

    addDockWidget(Qt::LeftDockWidgetArea, sceneObjectsDock_);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);

    if (!host_->HasScene())
    {
        AppendLine(QStringLiteral(
            "Preview проекта. Создайте свой проект в лаунчере для редактирования сцены."
        ));

        sceneObjectsDock_->hide();
        inspectorDock_->hide();
        return;
    }

    QFile manifest(QString::fromUtf8(host_->host().projectFile));

    if (manifest.open(QIODevice::ReadOnly))
    {
        const auto json = QJsonDocument::fromJson(manifest.readAll()).object();
        const auto relative = json.value(QStringLiteral("startupScene")).toString();

        if (!relative.isEmpty())
            scenePath_ = QFileInfo(manifest).dir().absoluteFilePath(relative);
    }

    sceneObjectsPanel_ = new SceneObjectsPanel(sceneObjectsDock_);
    objectInspectorPanel_ = new ObjectInspectorPanel(inspectorDock_);

    sceneObjectsDock_->setWidget(sceneObjectsPanel_);
    inspectorDock_->setWidget(objectInspectorPanel_);

    ConnectPanels();

    if (viewportDock_)
    {
        splitDockWidget(sceneObjectsDock_, viewportDock_, Qt::Horizontal);
        splitDockWidget(viewportDock_, inspectorDock_, Qt::Horizontal);

        resizeDocks(
            {sceneObjectsDock_, viewportDock_, inspectorDock_},
            {200, 880, 200},
            Qt::Horizontal
        );
    }
    else
    {
        resizeDocks(
            {sceneObjectsDock_, inspectorDock_},
            {280, 300},
            Qt::Horizontal
        );
    }
}

void EditorMainWindow::ConnectPanels()
{
    if (!sceneObjectsPanel_ || !objectInspectorPanel_)
        return;

    connect(sceneObjectsPanel_, &SceneObjectsPanel::AddPrimitiveRequested, this,
            [this](const QString& typeName, const QString& typeLabel, const std::array<double, 3>& position)
            {
                if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                    return;

                EditorCommand c;
                c.type = EditorCommand::CreateObject;
                c.path = typeName;
                c.name = typeLabel;

                for (int i = 0; i < 3; ++i)
                    c.transform.position[i] = static_cast<float>(position[i]);

                host_->PostCommand(c);
            });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::AddModelRequested, this, [this]
    {
        AddModel();
    });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::ImportModelRequested, this, [this]
    {
        ImportAsset(false);
    });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::DuplicateRequested, this, [this]
    {
        SubmitObjectCommand(EditorCommand::DuplicateObject);
    });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::RemoveRequested, this, [this]
    {
        SubmitObjectCommand(EditorCommand::RemoveObject);
    });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::SelectionChanged, this, [this](quint64 objectId)
    {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
            return;

        EditorCommand c;
        c.type = EditorCommand::SelectObject;
        c.objectId = objectId;
        host_->PostCommand(c);
    });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::FocusRequested, this, [this]
    {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
            return;

        EditorCommand c;
        c.type = EditorCommand::FocusObject;
        host_->PostCommand(c);
    });

    connect(objectInspectorPanel_, &ObjectInspectorPanel::NameEditingFinished, this,
            [this](quint64 objectId, const QString& name)
            {
                if (!objectId || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                    return;

                const QString trimmed = name.trimmed();

                for (const auto& object : host_->ReadSnapshot().objects)
                {
                    if (object.id == objectId && trimmed == QString::fromStdString(object.name))
                        return;
                }

                EditorCommand c;
                c.type = EditorCommand::RenameObject;
                c.objectId = objectId;
                c.name = trimmed;

                host_->PostCommand(c);
            });

    connect(objectInspectorPanel_, &ObjectInspectorPanel::ChooseTextureRequested, this, [this]
    {
        if (!host_->HasScene() ||
            sceneBusy_ ||
            host_->PlayState() != EditorQtHost::EditorState::Edit)
        {
            return;
        }

        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Текстура из проекта"),
            QString::fromUtf8(host_->host().contentRoot),
            QStringLiteral("Текстуры (*.png *.jpg *.jpeg *.bmp *.tga)")
        );

        if (!path.isEmpty())
            UseAsset(path);
    });

    connect(objectInspectorPanel_, &ObjectInspectorPanel::ClearTextureRequested, this, [this]
    {
        if (!selectedId_ ||
            sceneBusy_ ||
            host_->PlayState() != EditorQtHost::EditorState::Edit)
        {
            return;
        }

        EditorCommand c;
        c.type = EditorCommand::SetTexture;
        c.objectId = selectedId_;

        host_->PostCommand(c);
    });

    connect(objectInspectorPanel_, &ObjectInspectorPanel::ChooseMaterialRequested, this, [this]
    {
        if (!host_->HasScene() ||
            sceneBusy_ ||
            host_->PlayState() != EditorQtHost::EditorState::Edit)
        {
            return;
        }

        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Материал из проекта"),
            QString::fromUtf8(host_->host().contentRoot),
            QStringLiteral("Материалы (*.mat)")
        );

        if (!path.isEmpty())
            UseAsset(path);
    });

    connect(objectInspectorPanel_, &ObjectInspectorPanel::EditMaterialRequested, this, [this](const QString& material)
    {
        if (!material.isEmpty())
            OpenMaterialEditor(material);
        else
            CreateMaterialForSelection();
    });

    connect(objectInspectorPanel_, &ObjectInspectorPanel::ClearMaterialRequested, this, [this]
    {
        if (!selectedId_ ||
            sceneBusy_ ||
            host_->PlayState() != EditorQtHost::EditorState::Edit)
        {
            return;
        }

        EditorCommand c;
        c.type = EditorCommand::SetMaterial;
        c.objectId = selectedId_;

        host_->PostCommand(c);
    });

    connect(objectInspectorPanel_, &ObjectInspectorPanel::TransformEditingFinished,
            this, &EditorMainWindow::OnTransformEdited);

    connect(objectInspectorPanel_, &ObjectInspectorPanel::PointLightEditingFinished,
            this, &EditorMainWindow::OnPointLightEdited);

    connect(objectInspectorPanel_, &ObjectInspectorPanel::LightColorRequested,
            this, &EditorMainWindow::OnLightColorRequested);
}

void EditorMainWindow::RefreshSceneUi()
{
    if (!host_->HasScene())
        return;

    const auto view = host_->ReadSnapshot();

    const bool edit =
        host_->PlayState() == EditorQtHost::EditorState::Edit &&
        !sceneBusy_;

    if (sceneObjectsPanel_)
    {
        QVector<std::pair<QString, QString>> primitiveTypes;
        primitiveTypes.reserve(static_cast<int>(view.types.size()));

        for (const auto& type : view.types)
        {
            primitiveTypes.push_back({
                QString::fromStdString(type.label),
                QString::fromStdString(type.name)
            });
        }

        sceneObjectsPanel_->SetPrimitiveTypes(primitiveTypes);
        sceneObjectsPanel_->SetEnabled(edit);
        sceneObjectsPanel_->RefreshHierarchy(view);
    }

    if (undoAction_)
        undoAction_->setEnabled(edit && view.canUndo);

    if (redoAction_)
        redoAction_->setEnabled(edit && view.canRedo);

    if (duplicateAction_)
        duplicateAction_->setEnabled(edit && view.selected);

    if (deleteAction_)
        deleteAction_->setEnabled(edit && view.selected);

    if (saveAction_)
        saveAction_->setEnabled(!sceneBusy_);

    setWindowTitle(QStringLiteral("%1%2 — %3 — Engine Editor")
        .arg(
            view.dirty ? QStringLiteral("* ") : QString(),
            scenePath_.isEmpty()
                ? QStringLiteral("Новая сцена")
                : QFileInfo(scenePath_).fileName(),
            QString::fromUtf8(host_->host().projectName)
        )
    );

    const bool selectionChanged = selectedId_ != view.selected;
    selectedId_ = view.selected;

    if (objectInspectorPanel_)
    {
        objectInspectorPanel_->SetEnabled(edit && selectedId_);
        objectInspectorPanel_->Refresh(view, selectedId_, selectionChanged);
    }

    EditorCommand c;
    c.type = EditorCommand::RefreshScene;
    host_->PostCommand(c);
}

void EditorMainWindow::MaybeSave(std::function<void()> after)
{
    if (checkingChanges_)
        return;

    if (objectInspectorPanel_)
        objectInspectorPanel_->ClearFocusIfInside();

    checkingChanges_ = true;
    afterCheck_ = std::move(after);

    EditorCommand c;
    c.type = EditorCommand::RefreshScene;
    c.name = QStringLiteral("confirm");

    host_->PostCommand(c);
}

void EditorMainWindow::SaveScene(bool saveAs, std::function<void()> after)
{
    if (!host_->HasScene() || sceneBusy_)
        return;

    if (objectInspectorPanel_)
        objectInspectorPanel_->ClearFocusIfInside();

    QString path = scenePath_;

    if (saveAs || path.isEmpty())
    {
        path = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Сохранить стартовую сцену"),
            path.isEmpty()
                ? QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(
                    QStringLiteral("Scenes/New.scene.json"))
                : path,
            QStringLiteral("Scene (*.scene.json)")
        );

        if (path.isEmpty())
            return;

        if (!path.endsWith(QStringLiteral(".scene.json"), Qt::CaseInsensitive))
        {
            path += QStringLiteral(".scene.json");

            if (QFileInfo::exists(path) &&
                QMessageBox::question(
                    this,
                    QStringLiteral("Заменить файл?"),
                    path,
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                ) != QMessageBox::Yes)
            {
                return;
            }
        }
    }

    sceneBusy_ = true;
    afterSave_ = std::move(after);

    EditorCommand c;
    c.type = EditorCommand::SaveScene;
    c.path = QFileInfo(path).absoluteFilePath();

    host_->PostCommand(c);
}

void EditorMainWindow::OnTransformEdited(
    quint64 objectId,
    const InspectorTransformValue& value
)
{
    if (!objectId || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
        return;

    EditorCommand c;
    c.type = EditorCommand::UpdateObjectTransform;
    c.objectId = objectId;

    for (int i = 0; i < 3; ++i)
    {
        c.transform.position[i] = static_cast<float>(value.position[i]);
        c.transform.scale[i] = static_cast<float>(value.scale[i]);
    }

    const auto q = QQuaternion::fromEulerAngles(
        static_cast<float>(value.euler[0]),
        static_cast<float>(value.euler[1]),
        static_cast<float>(value.euler[2])
    );

    c.transform.rotation[0] = q.x();
    c.transform.rotation[1] = q.y();
    c.transform.rotation[2] = q.z();
    c.transform.rotation[3] = q.scalar();

    bool found = false;

    for (const auto& object : host_->ReadSnapshot().objects)
    {
        if (object.id != objectId)
            continue;

        found = true;

        bool changed = false;

        for (int i = 0; i < 3; ++i)
        {
            changed |= std::abs(c.transform.position[i] - object.transform.position[i]) > 0.0006f;
            changed |= std::abs(c.transform.scale[i] - object.transform.scale[i]) > 0.0006f;
        }

        const auto& r = object.transform.rotation;

        changed |= std::abs(QQuaternion::dotProduct(
            q,
            QQuaternion(r[3], r[0], r[1], r[2]).normalized()
        )) < 0.999999f;

        if (!changed)
            return;

        break;
    }

    if (!found)
        return;

    host_->PostCommand(c);
}

void EditorMainWindow::OnPointLightEdited(
    quint64 objectId,
    const InspectorPointLightValue& value
)
{
    if (!objectId || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
        return;

    for (const auto& object : host_->ReadSnapshot().objects)
    {
        if (object.id != objectId || !object.hasPointLight)
            continue;

        EditorCommand c;
        c.type = EditorCommand::SetPointLight;
        c.objectId = objectId;
        c.pointLight = object.pointLight;

        bool changed = false;

        for (int i = 0; i < 3; ++i)
        {
            c.pointLight.color[i] = static_cast<float>(value.color[i]);
            changed |= std::abs(c.pointLight.color[i] - object.pointLight.color[i]) > 0.0006f;
        }

        c.pointLight.intensity = static_cast<float>(value.intensity);
        c.pointLight.radius = static_cast<float>(value.radius);
        c.pointLight.enabled = value.enabled ? 1u : 0u;

        changed |=
            std::abs(c.pointLight.intensity - object.pointLight.intensity) > 0.0006f ||
            std::abs(c.pointLight.radius - object.pointLight.radius) > 0.0006f ||
            c.pointLight.enabled != object.pointLight.enabled;

        if (changed)
            host_->PostCommand(c);

        return;
    }
}

void EditorMainWindow::OnLightColorRequested(quint64 objectId, const QColor& current)
{
    if (!objectId || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
        return;

    auto* dialog = new QColorDialog(current, this);
    dialog->setWindowTitle(QStringLiteral("Цвет источника света"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setOption(QColorDialog::DontUseNativeDialog);

    connect(dialog, &QColorDialog::colorSelected, this, [this, objectId](const QColor& selected)
    {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
            return;

        for (const auto& object : host_->ReadSnapshot().objects)
        {
            if (object.id != objectId || !object.hasPointLight)
                continue;

            EditorCommand c;
            c.type = EditorCommand::SetPointLight;
            c.objectId = objectId;
            c.pointLight = object.pointLight;

            c.pointLight.color[0] = selected.redF();
            c.pointLight.color[1] = selected.greenF();
            c.pointLight.color[2] = selected.blueF();

            host_->PostCommand(c);
            break;
        }
    });

    dialog->open();
}

void EditorMainWindow::CommandFinished(
    const EditorCommand& command,
    uint32_t status,
    const QString& error
)
{
    if (command.type == EditorCommand::RefreshScene &&
        command.name == QStringLiteral("confirm"))
    {
        auto continuation = std::move(afterCheck_);
        afterCheck_ = {};
        checkingChanges_ = false;

        if (!continuation)
            return;

        if (!host_->ReadSnapshot().dirty)
        {
            continuation();
            return;
        }

        const auto choice = QMessageBox::question(
            this,
            QStringLiteral("Несохранённая сцена"),
            QStringLiteral("Сохранить изменения сцены?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (choice == QMessageBox::Save)
            SaveScene(false, std::move(continuation));
        else if (choice == QMessageBox::Discard)
            continuation();

        return;
    }

    if (command.type == EditorCommand::RefreshScene &&
        command.name == QStringLiteral("close"))
    {
        closing_ = false;

        MaybeSave([this]
        {
            closing_ = true;

            EditorCommand c;
            c.type = EditorCommand::RequestExit;
            host_->PostCommand(c);
        });

        return;
    }

    if (command.type == EditorCommand::SaveScene ||
        command.type == EditorCommand::LoadScene ||
        command.type == EditorCommand::NewScene)
    {
        sceneBusy_ = false;
    }

    if (status != EngineSceneOk)
    {
        afterSave_ = {};
        closing_ = false;

        const QString message = error.isEmpty()
                                    ? QStringLiteral(
                                        "Операция не выполнена (код %1). Проверьте имя объекта, остановите Play и повторите."
                                    ).arg(status)
                                    : error;

        AppendLine(message);

        QMessageBox::warning(
            this,
            QStringLiteral("Ошибка сцены"),
            message
        );

        return;
    }

    if (command.type == EditorCommand::ImportModel ||
        command.type == EditorCommand::ImportTexture)
    {
        AppendLine(QStringLiteral(
            "Импортировано: %1. Двойной щелчок в Files: добавить модель или назначить текстуру выбранному объекту."
        ).arg(command.path));

        statusBar()->showMessage(
            QStringLiteral("Импортировано: %1").arg(command.path),
            8000
        );
    }
    else if (command.type == EditorCommand::SaveScene)
    {
        scenePath_ = command.path;

        QFile file(QString::fromUtf8(host_->host().projectFile));
        bool committed = false;

        if (file.open(QIODevice::ReadOnly))
        {
            QJsonParseError parse{};
            auto document = QJsonDocument::fromJson(file.readAll(), &parse);
            file.close();

            if (parse.error == QJsonParseError::NoError && document.isObject())
            {
                auto json = document.object();

                const auto relative = QFileInfo(file).dir().relativeFilePath(scenePath_);

                if (QDir::isRelativePath(relative))
                {
                    json.insert(QStringLiteral("startupScene"), relative);

                    QSaveFile output(file.fileName());
                    const auto bytes = QJsonDocument(json).toJson();

                    committed =
                        output.open(QIODevice::WriteOnly) &&
                        output.write(bytes) == bytes.size() &&
                        output.commit();
                }
            }
        }

        if (!committed)
        {
            afterSave_ = {};
            closing_ = false;

            QMessageBox::warning(
                this,
                QStringLiteral("Стартовая сцена не изменена"),
                QStringLiteral(
                    "Сцена сохранена, но Project.json не обновлён. Проверьте права записи и сохраните сцену на том же диске, что и проект. Запуск отменён."
                )
            );

            return;
        }

        statusBar()->showMessage(
            QStringLiteral("Сохранено: %1").arg(scenePath_),
            5000
        );

        auto continuation = std::move(afterSave_);
        afterSave_ = {};

        if (continuation)
            continuation();
    }
    else if (command.type == EditorCommand::LoadScene)
    {
        scenePath_ = command.path;
    }
    else if (command.type == EditorCommand::NewScene)
    {
        scenePath_.clear();
    }

    RefreshSceneUi();
}

void EditorMainWindow::SubmitObjectCommand(int type)
{
    if (!host_ ||
        !host_->HasScene() ||
        sceneBusy_ ||
        host_->PlayState() != EditorQtHost::EditorState::Edit)
    {
        return;
    }

    EditorCommand c;
    c.type = static_cast<EditorCommand::Type>(type);
    c.objectId = selectedId_;

    if (!c.objectId &&
        (c.type == EditorCommand::DuplicateObject ||
            c.type == EditorCommand::RemoveObject))
    {
        return;
    }

    host_->PostCommand(c);
}

void EditorMainWindow::OpenScene(const QString& supplied)
{
    if (!host_ ||
        !host_->HasScene() ||
        sceneBusy_ ||
        host_->PlayState() != EditorQtHost::EditorState::Edit)
    {
        return;
    }

    MaybeSave([this, supplied]
    {
        const QString path = supplied.isEmpty()
                                 ? QFileDialog::getOpenFileName(
                                     this,
                                     QStringLiteral("Открыть сцену"),
                                     QString::fromUtf8(host_->host().contentRoot),
                                     QStringLiteral("Scene (*.scene.json)")
                                 )
                                 : supplied;

        if (path.isEmpty())
            return;

        sceneBusy_ = true;

        EditorCommand c;
        c.type = EditorCommand::LoadScene;
        c.path = path;

        host_->PostCommand(c);
    });
}
