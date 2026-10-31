#include "MainWindow.h"
#include "QtHost.h"
#include "FileBrowser.h"
#include "ViewportWidget.h"
#include "SceneObjectsPanel.h"
#include "ObjectInspectorPanel.h"
#include "UIEditorWindow.h"

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
#include <QInputDialog>
#include <QSettings>
#include <QCryptographicHash>
#include <QResizeEvent>

EditorMainWindow::EditorMainWindow(EditorQtHost* host, QWidget* parent)
    : QMainWindow(parent), host_(host)
{
    BuildUi();
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &EditorMainWindow::RefreshStats);
    timer->start(250);
    QTimer::singleShot(500, this, &EditorMainWindow::OfferRecovery);
    menuBar()->addAction(QStringLiteral("Экспорт игры…"), this, [this] {
        if (host_->PlayState() != EditorQtHost::Edit || !CloseMaterialEditors()) return;
        MaybeSave([this] { ExportProject(); });
    });
}

void EditorMainWindow::BuildUi()
{
    setWindowTitle(QStringLiteral("Engine Editor — %1")
        .arg(QString::fromUtf8(host_->host().projectName)));

    resize(1280, 720);

    setDockNestingEnabled(true);
    setDockOptions(AnimatedDocks | AllowNestedDocks | AllowTabbedDocks | GroupedDragging);
    setStyleSheet(QStringLiteral(
        "QMainWindow::separator { width: 9px; height: 9px; background: #424242; }"
        "QMainWindow::separator:hover { background: #b67a27; }"
        "QDockWidget::title { padding: 5px 8px; }"));

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
    console_->setObjectName(QStringLiteral("Console"));
    console_->setReadOnly(true);
    console_->setMaximumBlockCount(2000);

    consoleDock_ = new QDockWidget(QStringLiteral("Console"), this);
    consoleDock_->setObjectName(QStringLiteral("ConsoleDock"));
    consoleDock_->setWidget(console_);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock_);

    fileDock_ = new FileBrowserDock(host_, this);
    fileDock_->installEventFilter(this);
    addDockWidget(Qt::BottomDockWidgetArea, fileDock_);

    tabifyDockWidget(consoleDock_, fileDock_);

    resizeDocks({viewportDock_, consoleDock_}, {620, 100}, Qt::Vertical);

    BuildToolBar();

    BuildSceneDocks();

    resizeDocks({viewportDock_, consoleDock_}, {620, 100}, Qt::Vertical);

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("Вид"));

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
    viewMenu->addSeparator();
    viewMenu->addAction(QStringLiteral("Ресурсы: компактная панель"),this,&EditorMainWindow::CompactContentPanel);
    viewMenu->addAction(QStringLiteral("Ресурсы: отдельное окно"),this,[this]{fileDock_->show();fileDock_->setFloating(true);fileDock_->resize(800,450);fileDock_->raise();});
    viewMenu->addAction(QStringLiteral("Сбросить расположение панелей"),this,&EditorMainWindow::ResetPanelLayout);

    statsLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(statsLabel_);

    auto* snapBar = addToolBar(QStringLiteral("Привязки"));
    snapBar->setObjectName(QStringLiteral("SnapToolbar"));
    auto* moveStep = new QDoubleSpinBox(snapBar); moveStep->setObjectName("MoveSnap"); moveStep->setRange(0,1000); moveStep->setDecimals(3); moveStep->setSingleStep(0.5); moveStep->setSpecialValueText(QStringLiteral("Выкл."));
    auto* angleStep = new QDoubleSpinBox(snapBar); angleStep->setObjectName("AngleSnap"); angleStep->setRange(0,180); angleStep->setSingleStep(15); angleStep->setSpecialValueText(QStringLiteral("Выкл."));
    snapBar->addWidget(new QLabel(QStringLiteral("Шаг перемещения: "))); snapBar->addWidget(moveStep);
    snapBar->addWidget(new QLabel(QStringLiteral("Поворот, °: "))); snapBar->addWidget(angleStep);
    auto snap = [this,moveStep,angleStep] { viewport_->SetSnapping(moveStep->value(),angleStep->value()); };
    connect(moveStep,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[snap](double){snap();});
    connect(angleStep,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[snap](double){snap();});
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
    defaultPanelLayout_=saveState(1);
    QTimer::singleShot(0,this,[this]{
        const auto saved=QSettings("PainEngine","Editor").value(LayoutSettingsKey()).toByteArray();
        if(saved.isEmpty()||!restoreState(saved,1))ResetPanelLayout();
        panelLayoutReady_=true;
        QTimer::singleShot(0,this,[this]{panelWindowSize_=size();contentPanelHeight_=fileDock_->height();});
    });
}

void EditorMainWindow::resizeEvent(QResizeEvent* event) {
    const int bottom=panelLayoutReady_&&fileDock_&&!fileDock_->isFloating()&&dockWidgetArea(fileDock_)==Qt::BottomDockWidgetArea?contentPanelHeight_:0;
    QMainWindow::resizeEvent(event);
    // With an all-dock workspace Qt otherwise gives most newly available height
    // to the bottom tab group. Keep its chosen height when maximizing/resizing.
    if(bottom>0)QTimer::singleShot(0,this,[this,bottom]{
        if(fileDock_->isFloating()||dockWidgetArea(fileDock_)!=Qt::BottomDockWidgetArea)return;
        const int upper=qMax(320,height()-bottom-90);
        resizeDocks({sceneObjectsDock_,viewportDock_,inspectorDock_,consoleDock_},{upper,upper,upper,bottom+22},Qt::Vertical);
        QTimer::singleShot(0,this,[this]{panelWindowSize_=size();contentPanelHeight_=fileDock_->height();});
    });
}
bool EditorMainWindow::eventFilter(QObject* object,QEvent* event) {
    if(object==fileDock_&&event->type()==QEvent::Resize&&panelLayoutReady_&&size()==panelWindowSize_&&!fileDock_->isFloating()&&dockWidgetArea(fileDock_)==Qt::BottomDockWidgetArea)
        contentPanelHeight_=static_cast<QResizeEvent*>(event)->size().height();
    return QMainWindow::eventFilter(object,event);
}

QString EditorMainWindow::LayoutSettingsKey() const {
    const auto path=QFileInfo(QString::fromUtf8(host_->host().projectFile?host_->host().projectFile:"")).absoluteFilePath().toLower().toUtf8();
    return "Panels/v1/"+QString::fromLatin1(QCryptographicHash::hash(path,QCryptographicHash::Sha256).toHex());
}
void EditorMainWindow::CompactContentPanel() {
    fileDock_->setFloating(false);consoleDock_->setFloating(false);
    addDockWidget(Qt::BottomDockWidgetArea,consoleDock_);
    addDockWidget(Qt::BottomDockWidgetArea,fileDock_);
    tabifyDockWidget(consoleDock_,fileDock_);fileDock_->show();fileDock_->raise();
    QTimer::singleShot(0,this,[this]{
        const int upper=qMax(320,height()-270);
        resizeDocks({sceneObjectsDock_,viewportDock_,inspectorDock_,consoleDock_},{upper,upper,upper,180},Qt::Vertical);
    });
}
void EditorMainWindow::ResetPanelLayout() {
    for(auto* dock:{viewportDock_,consoleDock_,static_cast<QDockWidget*>(fileDock_),sceneObjectsDock_,inspectorDock_})dock->setFloating(false);
    restoreState(defaultPanelLayout_,1);
    resizeDocks({sceneObjectsDock_,viewportDock_,inspectorDock_},{280,qMax(400,width()-620),320},Qt::Horizontal);
    CompactContentPanel();
}

void EditorMainWindow::AppendLine(const QString& line) { console_->appendPlainText(line); }

void EditorMainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    if (exportProcess_) { statusBar()->showMessage(QStringLiteral("Дождитесь завершения экспорта."),5000); return; }
    if (closing_ || sceneBusy_ || checkingChanges_) return;
    if (!CloseMaterialEditors([this] { close(); })) return;
    for (auto window : uiEditors_) if (window && !window->ConfirmClose()) { event->ignore(); return; }
    QSettings("PainEngine","Editor").setValue(LayoutSettingsKey(),saveState(1));
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
    auto* sunlight = menu->addAction(QStringLiteral("Направленный свет"), this, &EditorMainWindow::ShowDirectionalLight);
    sunlight->setObjectName("SceneDirectionalLight");
    sunlight->setToolTip(QStringLiteral("Выбрать единственный источник света сцены или добавить его, если он отсутствует"));
    menu->addSeparator();
    auto* playMenu = menuBar()->addMenu(QStringLiteral("Запуск"));
    auto* hudSettings = playMenu->addAction(QStringLiteral("HUD игры…"), this, &EditorMainWindow::ShowHudSettings);
    hudSettings->setObjectName("ProjectHudSettings");
    auto* uiMenu = menuBar()->addMenu(QStringLiteral("UI"));
    uiMenu->addAction(QStringLiteral("Создать интерфейс…"), this, [this]{ CreateUI(); });
    uiMenu->addAction(QStringLiteral("Открыть редактор UI…"), this, [this]{
        const auto file = QFileDialog::getOpenFileName(this, QStringLiteral("Интерфейс"), QString::fromUtf8(host_->host().contentRoot), "UI (*.ui)");
        if (!file.isEmpty()) OpenUIEditor(file);
    });
    uiMenu->addAction(QStringLiteral("HUD игры…"), this, &EditorMainWindow::ShowHudSettings);
    playMenu->addAction(actPlay_); playMenu->addAction(actPause_); playMenu->addAction(actStop_);
    playMenu->addSeparator(); playMenu->addAction(actExternal_);
    playMenu->addAction(QStringLiteral("Настроить Play…"),this,[this]{
        if(host_->PlayState()!=EditorQtHost::Edit)return;
        const auto snapshot=host_->ReadSnapshot();
        QJsonObject data{{"version",1},{"character",0},{"camera",0},{"standardControl",true},{"cameraDistance",5},{"cameraHeight",2},{"lookSensitivity",0.005}};
        uint64_t settingsId=0;
        for(const auto& o:snapshot.objects)if(o.gameplayKind==3){
            if(settingsId){QMessageBox::warning(this,QStringLiteral("Play"),QStringLiteral("Включите «Вид → Служебные объекты сцены» и удалите лишние записи «Настройки Play». Для сцены нужна одна запись."));return;}
            settingsId=o.id;data=QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object();
        }
        QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("Персонаж и камера Play"));
        auto* form=new QFormLayout(&dialog);auto* character=new QComboBox(&dialog);auto* camera=new QComboBox(&dialog);
        character->setObjectName("PlayCharacter");camera->setObjectName("PlayCamera");
        character->addItem(QStringLiteral("Без управления персонажем"),QVariant::fromValue<qulonglong>(0));
        camera->addItem(QStringLiteral("Следовать за персонажем"),QVariant::fromValue<qulonglong>(0));
        for(const auto& o:snapshot.objects){auto* box=o.gameplayKind==1?character:o.gameplayKind==2?camera:nullptr;if(box)box->addItem(QString::fromStdString(o.name),QVariant::fromValue<qulonglong>(o.id));}
        character->setCurrentIndex((std::max)(0,character->findData(QVariant::fromValue<qulonglong>(data["character"].toInteger()))));
        camera->setCurrentIndex((std::max)(0,camera->findData(QVariant::fromValue<qulonglong>(data["camera"].toInteger()))));
        auto* standard=new QCheckBox(QStringLiteral("WASD: движение, ПКМ + мышь: обзор"),&dialog);standard->setChecked(data["standardControl"].toBool());
        auto* distance=new QDoubleSpinBox(&dialog);distance->setRange(0,10000);distance->setValue(data["cameraDistance"].toDouble());
        auto* height=new QDoubleSpinBox(&dialog);height->setRange(-10000,10000);height->setValue(data["cameraHeight"].toDouble());
        auto* sensitivity=new QDoubleSpinBox(&dialog);sensitivity->setDecimals(4);sensitivity->setSingleStep(0.001);sensitivity->setRange(0,1);sensitivity->setValue(data["lookSensitivity"].toDouble());
        form->addRow(QStringLiteral("Персонаж"),character);form->addRow(QStringLiteral("Камера"),camera);form->addRow(standard);
        form->addRow(QStringLiteral("Расстояние камеры"),distance);form->addRow(QStringLiteral("Высота камеры"),height);form->addRow(QStringLiteral("Чувствительность обзора"),sensitivity);
        form->addRow(new QLabel(QStringLiteral("Добавьте персонажа в категории Персонажи.\nФиксированная камера использует трансформацию объекта.\nSpace: Stop, P: Pause. Для управления щёлкните viewport."),&dialog));
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted)return;
        data["character"]=static_cast<qint64>(character->currentData().toULongLong());data["camera"]=static_cast<qint64>(camera->currentData().toULongLong());
        data["standardControl"]=standard->isChecked();data["cameraDistance"]=distance->value();data["cameraHeight"]=height->value();data["lookSensitivity"]=sensitivity->value();
        EditorCommand c;c.type=EditorCommand::ConfigurePlay;c.objectId=settingsId;c.path=QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));host_->PostCommand(c);
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

    saveAction_ = menu->addAction(QStringLiteral("Сохранить сцену"), this, [this]
    {
        SaveScene();
    }, QKeySequence::Save);

    menu->addAction(QStringLiteral("Сохранить как…"), this, [this]
    {
        SaveScene(true);
    }, QKeySequence::SaveAs);

    menu->addAction(QStringLiteral("Сделать стартовой сценой"), this, [this] {
        SaveScene(false, [this] { SetStartupScene(); });
    });
    menu->addAction(QStringLiteral("Восстановить несохранённую сцену…"), this, &EditorMainWindow::OfferRecovery);

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
    toolBar_ = addToolBar(QStringLiteral("Play"));
    toolBar_->setObjectName(QStringLiteral("PlayToolbar"));
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
    gameProc_->setArguments({QStringLiteral("--project"), project, QStringLiteral("--mode"), QStringLiteral("game"), QStringLiteral("--compatible-engine")});
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
    objectInspectorPanel_->setProperty("contentRoot",QString::fromUtf8(host_->host().contentRoot));

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

void EditorMainWindow::ShowDirectionalLight()
{
    if (sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    for (const auto& object : host_->ReadSnapshot().objects)
        if (object.type == "Light.Directional") {
            EditorCommand c; c.type = EditorCommand::SelectObject; c.objectId = object.id;
            host_->PostCommand(c); return;
        }
    EditorCommand c; c.type = EditorCommand::CreateObject;
    c.path = "Light.Directional"; c.name = QStringLiteral("Направленный свет");
    const auto rotation = QQuaternion::fromEulerAngles(-55, -30, 0);
    c.transform.rotation[0] = rotation.x(); c.transform.rotation[1] = rotation.y();
    c.transform.rotation[2] = rotation.z(); c.transform.rotation[3] = rotation.scalar();
    const auto position = sceneObjectsPanel_->SpawnPosition();
    for (int i=0;i<3;++i) c.transform.position[i] = static_cast<float>(position[i]);
    host_->PostCommand(c);
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
                if (typeName == "Light.Directional") { ShowDirectionalLight(); return; }

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

    connect(sceneObjectsPanel_, &SceneObjectsPanel::SelectionChanged, this, [this](quint64 objectId, const QVector<quint64>& ids)
    {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
            return;

        EditorCommand c;
        c.type = EditorCommand::SelectObject;
        c.objectIds.assign(ids.begin(), ids.end());
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

    connect(objectInspectorPanel_, &ObjectInspectorPanel::PropertyEditingFinished,this,[this](quint64 id,const QString& path,const QString& value) {
        if(sceneBusy_ || host_->PlayState()!=EditorQtHost::Edit)return;
        EditorCommand c;c.type=EditorCommand::SetProperty;c.objectId=id;c.path=path;c.name=value;host_->PostCommand(c);
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
    if (auto* action = findChild<QAction*>("SceneDirectionalLight"))
        action->setEnabled(edit && std::any_of(view.types.begin(), view.types.end(), [](const auto& type) { return type.name == "Light.Directional"; }));

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
    if (view.selectedIds.size() > 1) inspectorDock_->setWindowTitle(QStringLiteral("Группа: %1 • трансформация относительно активного объекта").arg(view.selectedIds.size()));

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
            QStringLiteral("Сохранить сцену"),
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
        else if (choice == QMessageBox::Discard) {
            EditorCommand discard; discard.type = EditorCommand::DiscardRecovery; host_->PostCommand(discard);
            continuation();
        }

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
        command.type == EditorCommand::NewScene || command.type == EditorCommand::UpdateAsset)
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
        scenePath_ = command.name == "recovery" ? QString{} : command.path;
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

void EditorMainWindow::SetStartupScene()
{
    const QString project = QString::fromUtf8(host_->host().projectFile);
    auto json = RecoveryStore::Read(project);
    const auto relative = QFileInfo(project).dir().relativeFilePath(scenePath_);
    if (json.isEmpty() || scenePath_.isEmpty() || !QDir::isRelativePath(relative)) {
        QMessageBox::warning(this, QStringLiteral("Стартовая сцена"), QStringLiteral("Сохраните сцену на том же диске, что и проект.")); return;
    }
    json["startupScene"] = relative;
    QSaveFile file(project); const auto bytes = QJsonDocument(json).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        QMessageBox::warning(this, QStringLiteral("Стартовая сцена"), QStringLiteral("Не удалось сохранить Project.json.")); return;
    }
    statusBar()->showMessage(QStringLiteral("Стартовая сцена: %1").arg(relative), 8000);
}
void EditorMainWindow::OfferRecovery()
{
    if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    const auto candidates = RecoveryStore::Candidates(QString::fromUtf8(host_->host().projectFile));
    if (candidates.isEmpty()) return;
    const auto record = candidates.front(); const auto info = RecoveryStore::Read(record);
    const auto answer = QMessageBox::question(this, QStringLiteral("Восстановление сцены"),
        QStringLiteral("Найдена несохранённая сцена от %1. Исходная сцена: %2. Движок: %3.\nОткрыть восстановленную копию? Правки материалов в неё не входят.")
        .arg(info["savedUtc"].toString(), info["sourceScene"].toString(), info["engineVersion"].toString()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return;
    MaybeSave([this, record, info] {
        EditorCommand c; c.type = EditorCommand::LoadScene; c.name = "recovery";
        c.path = QFileInfo(record).dir().filePath(info["snapshot"].toString());
        sceneBusy_ = true; host_->PostCommand(c);
    });
}

void EditorMainWindow::ExportProject()
{
    if (exportProcess_) return;
    const auto parent = QFileDialog::getExistingDirectory(this, QStringLiteral("Каталог для готовой игры"));
    if (parent.isEmpty()) return;
    bool ok = false;
    const auto name = QInputDialog::getText(this, QStringLiteral("Экспорт игры"), QStringLiteral("Имя новой папки"), QLineEdit::Normal, "GameExport", &ok);
    if (!ok || name.isEmpty()) return;
    if (name == "." || name == ".." || name.contains('/') || name.contains('\\') || name.contains(':')) {
        QMessageBox::warning(this, QStringLiteral("Экспорт"), QStringLiteral("Укажите имя папки без пути.")); return;
    }
    exportProcess_ = new QProcess(this);
    exportProcess_->setProcessChannelMode(QProcess::MergedChannels);
    connect(exportProcess_, &QProcess::readyReadStandardOutput, this, [this] { AppendLine(QString::fromUtf8(exportProcess_->readAllStandardOutput())); });
    connect(exportProcess_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus state) {
        AppendLine(QString::fromUtf8(exportProcess_->readAllStandardOutput()));
        QMessageBox::information(this, QStringLiteral("Экспорт игры"), code == 0 && state == QProcess::NormalExit
            ? QStringLiteral("Готово. Запускайте Start.cmd в папке экспорта.") : QStringLiteral("Экспорт не завершён. Подробности в журнале и Validation.log временной папки."));
        exportProcess_->deleteLater(); exportProcess_ = nullptr;
    });
    connect(exportProcess_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        AppendLine(exportProcess_->errorString()); exportProcess_->deleteLater(); exportProcess_ = nullptr;
    });
    exportProcess_->start(QDir(QCoreApplication::applicationDirPath()).filePath("EngineRuntime.exe"),
        {"--export-project", QString::fromUtf8(host_->host().projectFile), "--destination", QDir(parent).filePath(name)});
}
