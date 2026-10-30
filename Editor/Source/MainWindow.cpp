#include "MainWindow.h"
#include "QtHost.h"
#include "FileBrowser.h"
#include "ViewportWidget.h"
#include "SceneObjectsPanel.h"
#include "ObjectInspectorPanel.h"
#include "MainToolBar.h"
#include "DockLayoutManager.h"
#include "PlaySettingsDialog.h"
#include "ExternalGameRunner.h"
#include "EditorTheme.h"
#include "EditorLocale.h"
#include "ViewportCommands.h"

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
#include <QJsonArray>
#include <QJsonDocument>
#include <QQuaternion>
#include <QSaveFile>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>

EditorMainWindow::EditorMainWindow(EditorQtHost* host, QWidget* parent)
    : QMainWindow(parent), host_(host)
{
    BuildUi();
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &EditorMainWindow::RefreshStats);
    timer->start(250);

    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] { RetranslateUi(); });
    themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
        if (actThemeDark_) actThemeDark_->setChecked(EditorTheme::Instance().Mode() == ThemeMode::Dark);
        if (actThemeLight_) actThemeLight_->setChecked(EditorTheme::Instance().Mode() == ThemeMode::Light);
        if (mainToolBar_) mainToolBar_->RefreshIcons();
    });
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
    RegisterViewportCommands(viewport_, host_);

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
    console_->setObjectName(QStringLiteral("Console"));
    console_->setReadOnly(true);
    console_->setMaximumBlockCount(2000);

    consoleDock_ = new QDockWidget(QStringLiteral("Console"), this);
    consoleDock_->setWidget(console_);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock_);

    fileDock_ = new FileBrowserDock(host_, this);
    addDockWidget(Qt::BottomDockWidgetArea, fileDock_);
    tabifyDockWidget(consoleDock_, fileDock_);

    BuildToolBar();
    BuildSceneDocks();

    DockLayoutManager::UnlockDockResizing(viewportDock_);
    DockLayoutManager::UnlockDockResizing(consoleDock_);
    DockLayoutManager::UnlockDockResizing(fileDock_);
    DockLayoutManager::UnlockDockResizing(sceneObjectsDock_);
    DockLayoutManager::UnlockDockResizing(inspectorDock_);
    DockLayoutManager::RestoreLayout(this);

    viewMenu_ = menuBar()->addMenu(Tr("Вид", "View"));
    langMenu_ = viewMenu_->addMenu(Tr("Язык", "Language"));
    langMenu_->addAction(QStringLiteral("Русский"), this, [] { EditorLocale::Instance().SetLanguage(EditorLanguage::Ru); });
    langMenu_->addAction(QStringLiteral("English"), this, [] { EditorLocale::Instance().SetLanguage(EditorLanguage::En); });

    themeMenu_ = viewMenu_->addMenu(Tr("Тема оформления", "Theme"));
    actThemeDark_ = themeMenu_->addAction(Tr("Тёмная (Dark)", "Dark"), this, [] { EditorTheme::Instance().SetMode(ThemeMode::Dark); });
    actThemeDark_->setCheckable(true);
    actThemeLight_ = themeMenu_->addAction(Tr("Светлая (Light)", "Light"), this, [] { EditorTheme::Instance().SetMode(ThemeMode::Light); });
    actThemeLight_->setCheckable(true);
    actThemeDark_->setChecked(EditorTheme::Instance().Mode() == ThemeMode::Dark);
    actThemeLight_->setChecked(EditorTheme::Instance().Mode() == ThemeMode::Light);

    auto* accentMenu = themeMenu_->addMenu(Tr("Акцентный цвет", "Accent Color"));
    accentMenu->addAction(QStringLiteral("Pain Red (#DC2626)"), [] { EditorTheme::Instance().SetAccentColor(QColor(QStringLiteral("#DC2626"))); });
    accentMenu->addAction(QStringLiteral("3ds Max Red (#E53935)"), [] { EditorTheme::Instance().SetAccentColor(QColor(QStringLiteral("#E53935"))); });
    accentMenu->addAction(QStringLiteral("Blender Orange (#E07A1F)"), [] { EditorTheme::Instance().SetAccentColor(QColor(QStringLiteral("#E07A1F"))); });
    accentMenu->addAction(QStringLiteral("Unreal Blue (#3B82F6)"), [] { EditorTheme::Instance().SetAccentColor(QColor(QStringLiteral("#3B82F6"))); });
    accentMenu->addAction(QStringLiteral("Unity Cyan (#00BCD4)"), [] { EditorTheme::Instance().SetAccentColor(QColor(QStringLiteral("#00BCD4"))); });

    viewMenu_->addSeparator();
    actResetLayout_ = viewMenu_->addAction(Tr("Сбросить раскладку окон", "Reset Window Layout"), this, [this] {
        DockLayoutManager::ResetLayout(this);
    });
    viewMenu_->addSeparator();

    QMenu* viewMenu = viewMenu_;

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

    BuildSceneMenu();
    BuildCodeTools();
    for (const auto& title : {QStringLiteral("Сцена"), QStringLiteral("Правка"), QStringLiteral("Запуск"), QStringLiteral("Проект"), QStringLiteral("Вид")})
        for (auto* action : menuBar()->actions()) if (action->text() == title) { menuBar()->removeAction(action); menuBar()->addAction(action); break; }

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
    if (!CloseMaterialEditors([this] { close(); })) return;
    DockLayoutManager::SaveLayout(this);
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

    for(auto* action : menuBar()->actions()) if(action->text()==QStringLiteral("Вид") && action->menu()) {
        auto* service=action->menu()->addAction(QStringLiteral("Служебные объекты сцены"));service->setCheckable(true);
        connect(service,&QAction::toggled,this,[this](bool visible){
            if (sceneObjectsPanel_) sceneObjectsPanel_->SetShowServiceObjects(visible);
            if(!visible)for(const auto& o:host_->ReadSnapshot().objects)if(o.id==selectedId_&&o.type=="Gameplay.PlaySettings"){
                EditorCommand c;c.type=EditorCommand::SelectObject;host_->PostCommand(c);
            }
            RefreshSceneUi();
        });
    }
    const bool available = host_->HasScene();
    menu->setEnabled(available);
    auto* playMenu = menuBar()->addMenu(QStringLiteral("Запуск"));
    playMenu->addAction(actPlay_); playMenu->addAction(actPause_); playMenu->addAction(actStop_);
    playMenu->addSeparator(); playMenu->addAction(actExternal_);
    actConfigurePlay_ = playMenu->addAction(QStringLiteral("Настроить Play…"), this, [this] {
        if (host_->PlayState() != EditorQtHost::Edit) return;
        const auto snapshot = host_->ReadSnapshot();
        std::vector<PlaySettingsObjectView> views;
        for (const auto& o : snapshot.objects) views.push_back({o.id, o.gameplayKind, o.name, o.projectData});
        PlaySettingsDialog dialog(views, this);
        uint64_t settingsId = 0; QString configJson;
        if (dialog.Execute(settingsId, configJson)) {
            EditorCommand c; c.type = EditorCommand::ConfigurePlay; c.objectId = settingsId; c.path = configJson;
            host_->PostCommand(c);
        }
    });
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

    auto* resources = menu->addMenu(QStringLiteral("Ресурсы"));
    resources->addAction(QStringLiteral("Добавить модель…"), this, [this] { AddModel(); });
    resources->addAction(QStringLiteral("Импорт модели…"), this, [this] { ImportAsset(false); });
    resources->addAction(QStringLiteral("Импорт текстуры…"), this, [this] { ImportAsset(true); });
    // F6 работает и в окнах материалов.
    auto* reloadShaders = menu->addAction(QStringLiteral("Перекомпилировать шейдеры"), this, [this]
    {
        ReloadShaders();
    }, QKeySequence(Qt::Key_F6));
    reloadShaders->setShortcutContext(Qt::ApplicationShortcut);
    reloadShaders->setEnabled(host_->HasMaterials());

    menu->addSeparator();

    auto* editMenu = menuBar()->addMenu(QStringLiteral("Правка"));
    undoAction_ = editMenu->addAction(QStringLiteral("Отменить"), this, [this]
    {
        SubmitObjectCommand(EditorCommand::Undo);
    }, QKeySequence::Undo);

    redoAction_ = editMenu->addAction(QStringLiteral("Повторить"), this, [this]
    {
        SubmitObjectCommand(EditorCommand::Redo);
    }, QKeySequence::Redo);

    duplicateAction_ = editMenu->addAction(QStringLiteral("Дублировать объект"), this, [this]
    {
        SubmitObjectCommand(EditorCommand::DuplicateObject);
    }, QKeySequence(QStringLiteral("Ctrl+D")));

    deleteAction_ = editMenu->addAction(QStringLiteral("Удалить объект"), this, [this]
    {
        SubmitObjectCommand(EditorCommand::RemoveObject);
    });

    menu->addSeparator();
    menu->addAction(QStringLiteral("Закрыть редактор"), this, [this] { close(); });


}

void EditorMainWindow::BuildToolBar()
{
    mainToolBar_ = new MainToolBar(this);
    toolBar_ = mainToolBar_;
    addToolBar(mainToolBar_);

    actPlay_ = mainToolBar_->ActPlay();
    actPause_ = mainToolBar_->ActPause();
    actStop_ = mainToolBar_->ActStop();
    actExternal_ = mainToolBar_->ActExternal();

    connect(mainToolBar_, &MainToolBar::PlayTriggered, this, [this] {
        EditorCommand c; c.type = host_->PlayState() == EditorQtHost::Edit ? EditorCommand::TogglePlay : EditorCommand::TogglePause;
        host_->PostCommand(c);
    });
    connect(mainToolBar_, &MainToolBar::PauseTriggered, this, [this] {
        EditorCommand c; c.type = EditorCommand::TogglePause; host_->PostCommand(c);
    });
    connect(mainToolBar_, &MainToolBar::StopTriggered, this, [this] {
        EditorCommand c; c.type = EditorCommand::TogglePlay; host_->PostCommand(c);
    });
    connect(mainToolBar_, &MainToolBar::ExternalTriggered, this, [this] {
        if (externalGameRunner_ && externalGameRunner_->IsRunning()) { StopExternalGame(); return; }
        if (host_->HasScene()) SaveScene(false, [this] { StartExternalGame(); });
        else StartExternalGame();
    });
    connect(mainToolBar_, &MainToolBar::ConfigPlayTriggered, this, [this] {
        if (actConfigurePlay_) actConfigurePlay_->trigger();
    });
    connect(mainToolBar_, &MainToolBar::CameraModeChanged, this, [this](int mode) {
        if (host_->PlayState() != EditorQtHost::Edit) return;
        const auto snapshot = host_->ReadSnapshot();
        uint64_t settingsId = 0;
        QJsonObject data{{"version", 1}, {"character", 0}, {"camera", 0}, {"standardControl", true}};
        for (const auto& o : snapshot.objects) if (o.gameplayKind == 3) { settingsId = o.id; data = QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object(); break; }
        if (mode == 0) { data["character"] = 0; data["standardControl"] = false; }
        else {
            uint64_t charId = 0;
            for (const auto& o : snapshot.objects) if (o.gameplayKind == 1) { charId = o.id; break; }
            data["character"] = static_cast<qint64>(charId); data["standardControl"] = true;
        }
        EditorCommand c; c.type = EditorCommand::ConfigurePlay; c.objectId = settingsId;
        c.path = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
        host_->PostCommand(c);
    });
    connect(mainToolBar_, &MainToolBar::LangToggleTriggered, this, &EditorMainWindow::RetranslateUi);

    UpdatePlayButtons();
}

void EditorMainWindow::UpdatePlayButtons()
{
    if (mainToolBar_) mainToolBar_->UpdatePlayState(static_cast<int>(host_->PlayState()), sceneBusy_);
}

void EditorMainWindow::StartExternalGame()
{
    if (!externalGameRunner_) externalGameRunner_ = new ExternalGameRunner(host_, actExternal_, this);
    externalGameRunner_->Start();
}

void EditorMainWindow::StopExternalGame()
{
    if (externalGameRunner_) externalGameRunner_->Stop();
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
    inspectorDock_->setMinimumWidth(300);
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
            {250, 700, 320},
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

    connect(sceneObjectsPanel_, &SceneObjectsPanel::AddModelRequested, this, [this] { AddModel(); });
    connect(sceneObjectsPanel_, &SceneObjectsPanel::ImportModelRequested, this, [this] { ImportAsset(false); });
    connect(sceneObjectsPanel_, &SceneObjectsPanel::DuplicateRequested, this, [this] { SubmitObjectCommand(EditorCommand::DuplicateObject); });
    connect(sceneObjectsPanel_, &SceneObjectsPanel::RemoveRequested, this, [this] { SubmitObjectCommand(EditorCommand::RemoveObject); });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::SelectionChanged, this, [this](quint64 objectId) {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        EditorCommand c; c.type = EditorCommand::SelectObject; c.objectId = objectId; host_->PostCommand(c);
    });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::FocusRequested, this, [this] {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        EditorCommand c; c.type = EditorCommand::FocusObject; host_->PostCommand(c);
    });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::SetParentRequested, this, [this](quint64 childId, quint64 parentId) {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        host_->SetParent(childId, parentId);
    });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::AddChildPrimitiveRequested, this,
            [this](quint64 parentId, const QString& typeName, const QString& typeLabel, const std::array<double, 3>& pos) {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        EditorCommand c; c.type = EditorCommand::CreateObject; c.path = typeName; c.name = typeLabel; c.parentId = parentId;
        c.transform.position[0] = static_cast<float>(pos[0]);
        c.transform.position[1] = static_cast<float>(pos[1]);
        c.transform.position[2] = static_cast<float>(pos[2]);
        host_->PostCommand(c);
    });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::RenameRequested, this, [this](quint64 objectId, const QString& name) {
        if (!objectId || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        EditorCommand c; c.type = EditorCommand::RenameObject; c.objectId = objectId; c.name = name.trimmed();
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

    connect(objectInspectorPanel_, &ObjectInspectorPanel::ChooseModelRequested, this, [this] {
        if (!selectedId_ || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Модель из проекта"),
            QString::fromUtf8(host_->host().contentRoot), QStringLiteral("Модели (*.glb *.gltf *.obj *.fbx)"));
        if (path.isEmpty()) return;
        const auto relative = QDir(QString::fromUtf8(host_->host().contentRoot)).relativeFilePath(path);
        if (relative == ".." || relative.startsWith("../") || QDir::isAbsolutePath(relative)) {
            QMessageBox::warning(this, QStringLiteral("Ресурс вне проекта"), QStringLiteral("Сначала импортируйте модель через меню «Импорт».")); return;
        }
        EditorCommand c; c.type = EditorCommand::SetModel; c.objectId = selectedId_;
        c.path = "/Game/" + QDir::fromNativeSeparators(relative); host_->PostCommand(c);
    });
    connect(objectInspectorPanel_, &ObjectInspectorPanel::CharacterSpeedEditingFinished, this, [this](quint64 id, double speed) {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        for (const auto& o : host_->ReadSnapshot().objects) if (o.id == id && o.gameplayKind == 1) {
            auto data = QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object();
            if (data["moveSpeed"].toDouble(5) == speed) return;
            data["moveSpeed"] = speed;
            EditorCommand c; c.type = EditorCommand::SetProjectData; c.objectId = id;
            c.path = QString::fromUtf8(QJsonDocument(data).toJson()); host_->PostCommand(c);
        }
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
    inspectorDock_->setWindowTitle(QStringLiteral("Свойства — нет выбора"));
    for (const auto& o : view.objects) if (o.id == selectedId_)
        inspectorDock_->setWindowTitle(QStringLiteral("Свойства — %1").arg(QString::fromStdString(o.name)));

    if (objectInspectorPanel_)
    {
        objectInspectorPanel_->SetEnabled(edit && selectedId_);
        objectInspectorPanel_->Refresh(view, selectedId_, selectionChanged);
        objectInspectorPanel_->SetMaterialsAvailable(host_->HasMaterials());
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
    if (command.type == EditorCommand::ReloadProject) {
        sceneBusy_ = false; setEnabled(true);
        if (status != EngineSceneOk && !command.name.isEmpty()) {
            // Roll back only our candidate module field; preserve concurrent edits to other fields.
            const auto project=QString::fromUtf8(host_->host().projectFile);
            QFile input(project);
            if(input.open(QIODevice::ReadOnly)) {
                auto data=QJsonDocument::fromJson(input.readAll()).object(); input.close();
                const auto current=QFileInfo(project).dir().absoluteFilePath(data["module"].toString());
                if(QDir::cleanPath(current).compare(QDir::cleanPath(command.path),Qt::CaseInsensitive)==0) {
                    data["module"]=command.name; QSaveFile output(project); const auto bytes=QJsonDocument(data).toJson();
                    if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit())
                        AppendLine(QStringLiteral("Не удалось вернуть путь прежней DLL в Project.json. Работающая сцена сохранена в памяти."));
                    else {
                        QFile types(QFileInfo(project).dir().filePath("ProjectTypes.tsv"));
                        if(types.open(QIODevice::ReadOnly)) {
                            const bool unchanged=types.readAll()==reloadTypesBuilt_; types.close();
                            if(unchanged) {
                                if(!reloadTypesExisted_) types.remove();
                                else { QSaveFile restore(types.fileName()); if(restore.open(QIODevice::WriteOnly) && restore.write(reloadTypesBefore_)==reloadTypesBefore_.size()) restore.commit(); }
                            }
                        }
                    }
                }
            }
        }
        if (status == EngineSceneOk && command.name == "unchanged") { AppendLine(QStringLiteral("C++: код уже актуален. Сцена и история отмены сохранены.")); return; }
        if (status == EngineSceneOk) {
            RefreshSceneUi();
            AppendLine(QStringLiteral("Hot reload: DLL заменена. Сцена, выбор и камера сохранены; история отмены очищена."));
        }
    }
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

        if (command.type == EditorCommand::ImportModel && command.path.startsWith(QStringLiteral("/Game/")))
        {
            QFile metadata(QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(command.path.mid(6) + QStringLiteral(".import.json")));
            if (metadata.open(QIODevice::ReadOnly))
            {
                const auto imported = QJsonDocument::fromJson(metadata.readAll()).object();
                const auto material = imported.value(QStringLiteral("defaultMaterial")).toString();
                if (!material.isEmpty())
                    AppendLine(QStringLiteral("Создан базовый материал %1. При добавлении модели он назначится автоматически; BaseMap можно изменить в редакторе материала.").arg(material));
                const auto count = imported.value(QStringLiteral("materials")).toArray().size();
                if (count > 1)
                    AppendLine(QStringLiteral("В модели %1 материалов. Все сохранены, но объект пока использует только материал первого меша.").arg(count));
            }
        }

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

void EditorMainWindow::RetranslateUi()
{
    setWindowTitle(QStringLiteral("PainEngine Editor - %1").arg(QString::fromUtf8(host_->host().projectName)));
    if (mainToolBar_) mainToolBar_->RetranslateUi();
    if (sceneObjectsPanel_) sceneObjectsPanel_->RetranslateUi();
    if (viewMenu_) viewMenu_->setTitle(Tr("Вид", "View"));
    if (themeMenu_) themeMenu_->setTitle(Tr("Тема оформления", "Theme"));
    if (langMenu_) langMenu_->setTitle(Tr("Язык", "Language"));
    if (actThemeDark_) actThemeDark_->setText(Tr("Тёмная (Dark)", "Dark"));
    if (actThemeLight_) actThemeLight_->setText(Tr("Светлая (Light)", "Light"));
    if (actResetLayout_) actResetLayout_->setText(Tr("Сбросить раскладку окон", "Reset Window Layout"));
}
