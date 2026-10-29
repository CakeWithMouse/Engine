#include "MainWindow.h"
#include "EditorLocale.h"
#include "EditorTheme.h"
#include "EditorDialogs.h"
#include <QSettings>
#include "QtHost.h"
#include "FileBrowser.h"
#include "ViewportWidget.h"
#include "SceneObjectsPanel.h"
#include "ObjectInspectorPanel.h"

#include <QDockWidget>
#include <QAction>
#include <QActionGroup>
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
#include <QToolButton>
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
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });
    themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
        if (actThemeDark_) actThemeDark_->setChecked(EditorTheme::Instance().Mode() == ThemeMode::Dark);
        if (actThemeLight_) actThemeLight_->setChecked(EditorTheme::Instance().Mode() == ThemeMode::Light);
        if (actPlay_) actPlay_->setIcon(EditorTheme::CreatePlayIcon());
        if (actPause_) actPause_->setIcon(EditorTheme::CreatePauseIcon());
        if (actStop_) actStop_->setIcon(EditorTheme::CreateStopIcon());
        if (actExternal_) actExternal_->setIcon(EditorTheme::CreateExternalIcon());
    });
}

EditorMainWindow::~EditorMainWindow()
{
    if (langSubId_) {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
    if (themeSubId_) {
        EditorTheme::Instance().UnsubscribeThemeChanged(themeSubId_);
        themeSubId_ = 0;
    }
}

void EditorMainWindow::BuildUi()
{
    setWindowTitle(QStringLiteral("%1 — %2")
        .arg(Tr("Редактор PainEngine", "PainEngine Editor"), QString::fromUtf8(host_->host().projectName)));

    resize(1280, 720);

    setDockNestingEnabled(true);
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AnimatedDocks | QMainWindow::GroupedDragging);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);

    viewport_ = new ViewportWidget(host_, this);

    viewportDock_ = new QDockWidget(Tr("Вьюпорт", "Viewport"), this);
    viewportDock_->setObjectName(QStringLiteral("ViewportDock"));
    viewportDock_->setWidget(viewport_);
    viewportDock_->setFocusProxy(viewport_);
    viewportDock_->setAllowedAreas(Qt::AllDockWidgetAreas);
    viewportDock_->setFeatures(
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable |
        QDockWidget::DockWidgetClosable
    );

    viewportDock_->setMinimumSize(80, 60);
    addDockWidget(Qt::TopDockWidgetArea, viewportDock_);
    setCentralWidget(nullptr);

    console_ = new QPlainTextEdit(this);
    console_->setObjectName(QStringLiteral("Console"));
    console_->setReadOnly(true);
    console_->setMaximumBlockCount(2000);
    console_->setMinimumSize(60, 50);
    console_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);

    consoleDock_ = new QDockWidget(Tr("Консоль", "Console"), this);
    consoleDock_->setWidget(console_);
    consoleDock_->setMinimumSize(80, 60);
    if (consoleDock_->widget()) {
        consoleDock_->widget()->setMinimumSize(80, 60);
        consoleDock_->widget()->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock_);

    fileDock_ = new FileBrowserDock(host_, this);
    fileDock_->setMinimumSize(80, 60);
    if (fileDock_->widget()) {
        fileDock_->widget()->setMinimumSize(80, 60);
        fileDock_->widget()->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }
    addDockWidget(Qt::BottomDockWidgetArea, fileDock_);

    tabifyDockWidget(consoleDock_, fileDock_);

    resizeDocks({viewportDock_, consoleDock_}, {620, 100}, Qt::Vertical);

    BuildToolBar();

    BuildSceneDocks();

    viewMenu_ = menuBar()->addMenu(Tr("Вид", "View"));

    if (viewportDock_)
        viewMenu_->addAction(viewportDock_->toggleViewAction());

    if (consoleDock_)
        viewMenu_->addAction(consoleDock_->toggleViewAction());

    if (fileDock_)
        viewMenu_->addAction(fileDock_->toggleViewAction());

    if (sceneObjectsDock_)
        viewMenu_->addAction(sceneObjectsDock_->toggleViewAction());

    if (inspectorDock_)
        viewMenu_->addAction(inspectorDock_->toggleViewAction());

    viewMenu_->addSeparator();
    langMenu_ = viewMenu_->addMenu(Tr("Язык", "Language"));
    actLangRu_ = langMenu_->addAction(QStringLiteral("Русский (RU)"), this, [this] {
        EditorLocale::Instance().SetLanguage(EditorLanguage::Ru);
    });
    actLangRu_->setCheckable(true);
    actLangEn_ = langMenu_->addAction(QStringLiteral("English (EN)"), this, [this] {
        EditorLocale::Instance().SetLanguage(EditorLanguage::En);
    });
    actLangEn_->setCheckable(true);
    auto* langGroup = new QActionGroup(this);
    langGroup->addAction(actLangRu_);
    langGroup->addAction(actLangEn_);
    actLangRu_->setChecked(EditorLocale::Instance().Language() == EditorLanguage::Ru);
    actLangEn_->setChecked(EditorLocale::Instance().Language() == EditorLanguage::En);

    themeMenu_ = viewMenu_->addMenu(Tr("Тема оформления", "Theme"));
    actThemeDark_ = themeMenu_->addAction(Tr("Тёмная (Dark)", "Dark"), this, [this] {
        EditorTheme::Instance().SetMode(ThemeMode::Dark);
    });
    actThemeDark_->setCheckable(true);
    actThemeLight_ = themeMenu_->addAction(Tr("Светлая (Light)", "Light"), this, [this] {
        EditorTheme::Instance().SetMode(ThemeMode::Light);
    });
    actThemeLight_->setCheckable(true);
    auto* themeModeGroup = new QActionGroup(this);
    themeModeGroup->addAction(actThemeDark_);
    themeModeGroup->addAction(actThemeLight_);
    actThemeDark_->setChecked(EditorTheme::Instance().Mode() == ThemeMode::Dark);
    actThemeLight_->setChecked(EditorTheme::Instance().Mode() == ThemeMode::Light);

    themeMenu_->addSeparator();
    accentMenu_ = themeMenu_->addMenu(Tr("Акцентный цвет", "Accent Color"));
    auto* accentGroup = new QActionGroup(this);

    auto addAccent = [this, accentGroup](const QString& title, const QString& hex) {
        auto* act = accentMenu_->addAction(title, this, [hex] {
            EditorTheme::Instance().SetAccentColor(QColor(hex));
        });
        act->setCheckable(true);
        accentGroup->addAction(act);
        const QString curHex = EditorTheme::Instance().AccentColor().name(QColor::HexRgb).toUpper();
        if (curHex == hex.toUpper()) act->setChecked(true);
        return act;
    };

    actAccentMaxRed_ = addAccent(QStringLiteral("3ds Max Red (#E53935)"), QStringLiteral("#E53935"));
    actAccentBlenderOrange_ = addAccent(QStringLiteral("Blender Orange (#E07A1F)"), QStringLiteral("#E07A1F"));
    actAccentUnrealBlue_ = addAccent(QStringLiteral("Unreal Blue (#3B82F6)"), QStringLiteral("#3B82F6"));
    actAccentUnityCyan_ = addAccent(QStringLiteral("Unity Cyan (#00BCD4)"), QStringLiteral("#00BCD4"));

    accentMenu_->addSeparator();
    actAccentCustom_ = accentMenu_->addAction(Tr("Выбрать свой цвет…", "Custom Color…"), this, [this] {
        const QColor cur = EditorTheme::Instance().AccentColor();
        const QColor chosen = QColorDialog::getColor(cur, this, Tr("Выберите акцентный цвет", "Select Accent Color"));
        if (chosen.isValid()) {
            EditorTheme::Instance().SetAccentColor(chosen);
        }
    });

    viewMenu_->addSeparator();
    actResetLayout_ = viewMenu_->addAction(Tr("Сбросить раскладку окон", "Reset Window Layout"), this, [this] {
        QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
        settings.remove(QStringLiteral("window/state"));
        settings.remove(QStringLiteral("window/geometry"));
        ApplyDefaultLayout();
    });

    statsLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(statsLabel_);

    BuildSceneMenu();
    BuildCodeTools();
    addMenu_ = menuBar()->addMenu(Tr("Добавить", "Add"));
    connect(addMenu_, &QMenu::aboutToShow, this, &EditorMainWindow::RebuildAddMenu);
    RebuildAddMenu();

    helpMenu_ = menuBar()->addMenu(Tr("Справка", "Help"));
    actDoc_ = helpMenu_->addAction(Tr("Документация (F1)", "Documentation (F1)"), this, [this] {
        ShowDocumentationDialog(this, scenePath_.isEmpty() ? QDir::currentPath() : QFileInfo(scenePath_).absolutePath());
    });
    actDoc_->setShortcut(QKeySequence(Qt::Key_F1));

    for (auto* menu : {sceneMenu_, editMenu_, addMenu_, viewMenu_, playMenu_, projectMenu_, helpMenu_}) {
        if (menu && menu->menuAction()) {
            menuBar()->removeAction(menu->menuAction());
            menuBar()->addAction(menu->menuAction());
        }
    }

    connect(viewport_, &ViewportWidget::contextMenuRequested,
            this, &EditorMainWindow::ShowViewportContextMenu);
    connect(viewport_, &ViewportWidget::addMenuRequested,
            this, &EditorMainWindow::ShowViewportAddMenu);
    connect(viewport_, &ViewportWidget::addPrimitiveRequested,
            this, [this](const QString& typeName, const QString& typeLabel, const std::array<double, 3>& position)
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
                if (viewport_) viewport_->ShowAdjustLastOperation(0, typeLabel, c.transform);
            });

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

    ApplySavedOrInitialLayout();

    if (host_->HasScene())
        RefreshSceneUi();
}

void EditorMainWindow::AppendLine(const QString& line) { console_->appendPlainText(line); }

void EditorMainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    if (closing_ || sceneBusy_ || checkingChanges_) return;
    if (!CloseMaterialEditors([this] { close(); })) return;

    const bool isTest = qEnvironmentVariableIsSet("PAIN_EDITOR_TEST_MODE") ||
        qApp->applicationFilePath().contains(QStringLiteral("Tests"), Qt::CaseInsensitive) ||
        (host_ && host_->host().projectName && std::string(host_->host().projectName).find("UI") != std::string::npos);
    if (!isTest) {
        QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
        settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
        settings.setValue(QStringLiteral("window/state"), saveState());
    }

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
    sceneMenu_ = menuBar()->addMenu(Tr("Сцена", "Scene"));

    if (viewMenu_) {
        actServiceObjects_ = viewMenu_->addAction(Tr("Служебные объекты сцены", "Scene Service Objects"));
        actServiceObjects_->setCheckable(true);
        connect(actServiceObjects_, &QAction::toggled, this, [this](bool visible){
            if (sceneObjectsPanel_) sceneObjectsPanel_->SetShowServiceObjects(visible);
            if(!visible)for(const auto& o:host_->ReadSnapshot().objects)if(o.id==selectedId_&&o.type=="Gameplay.PlaySettings"){
                EditorCommand c;c.type=EditorCommand::SelectObject;host_->PostCommand(c);
            }
            RefreshSceneUi();
        });
    }
    const bool available = host_->HasScene();
    sceneMenu_->setEnabled(available);
    playMenu_ = menuBar()->addMenu(Tr("Запуск", "Play"));
    playMenu_->addAction(actPlay_); playMenu_->addAction(actPause_); playMenu_->addAction(actStop_);
    playMenu_->addSeparator(); playMenu_->addAction(actExternal_);
    actConfigurePlay_ = playMenu_->addAction(Tr("Настроить Play…", "Configure Play…"), this, [this]{
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
    actNewScene_ = sceneMenu_->addAction(Tr("Новая сцена", "New Scene"), QKeySequence::New, this, [this]
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
    });

    actOpenScene_ = sceneMenu_->addAction(Tr("Открыть сцену…", "Open Scene…"), QKeySequence::Open, this, [this]
    {
        OpenScene();
    });

    saveAction_ = sceneMenu_->addAction(Tr("Сохранить и сделать стартовой", "Save and Set as Startup"), QKeySequence::Save, this, [this]
    {
        SaveScene();
    });

    actSaveAs_ = sceneMenu_->addAction(Tr("Сохранить как…", "Save As…"), QKeySequence::SaveAs, this, [this]
    {
        SaveScene(true);
    });

    sceneMenu_->addSeparator();

    resourcesMenu_ = sceneMenu_->addMenu(Tr("Ресурсы", "Assets"));
    actAddModel_ = resourcesMenu_->addAction(Tr("Добавить модель…", "Add Model…"), this, [this] { AddModel(); });
    actImportModel_ = resourcesMenu_->addAction(Tr("Импорт модели…", "Import Model…"), this, [this] { ImportAsset(false); });
    actImportTexture_ = resourcesMenu_->addAction(Tr("Импорт текстуры…", "Import Texture…"), this, [this] { ImportAsset(true); });
    // F6 работает и в окнах материалов.
    actReloadShaders_ = sceneMenu_->addAction(Tr("Перекомпилировать шейдеры", "Recompile Shaders"), QKeySequence(Qt::Key_F6), this, [this]
    {
        ReloadShaders();
    });
    actReloadShaders_->setShortcutContext(Qt::ApplicationShortcut);
    actReloadShaders_->setEnabled(host_->HasMaterials());

    sceneMenu_->addSeparator();

    editMenu_ = menuBar()->addMenu(Tr("Правка", "Edit"));
    undoAction_ = editMenu_->addAction(Tr("Отменить", "Undo"), QKeySequence::Undo, this, [this]
    {
        SubmitObjectCommand(EditorCommand::Undo);
    });

    redoAction_ = editMenu_->addAction(Tr("Повторить", "Redo"), QKeySequence::Redo, this, [this]
    {
        SubmitObjectCommand(EditorCommand::Redo);
    });

    duplicateAction_ = editMenu_->addAction(Tr("Дублировать объект", "Duplicate Object"), QKeySequence(QStringLiteral("Ctrl+D")), this, [this]
    {
        SubmitObjectCommand(EditorCommand::DuplicateObject);
    });

    deleteAction_ = editMenu_->addAction(Tr("Удалить объект", "Remove Object"), this, [this]
    {
        SubmitObjectCommand(EditorCommand::RemoveObject);
    });

    editMenu_->addSeparator();
    controlSchemeMenu_ = editMenu_->addMenu(Tr("Схема управления", "Control Scheme"));
    schemeGroup_ = new QActionGroup(this);
    schemeGroup_->setExclusive(true);

    actSchemeUnrealUnity_ = controlSchemeMenu_->addAction(Tr("Unreal / Unity (ПКМ + WASD, W/E/R)", "Unreal / Unity (RMB + WASD, W/E/R)"));
    actSchemeUnrealUnity_->setCheckable(true);
    schemeGroup_->addAction(actSchemeUnrealUnity_);

    actSchemeBlender_ = controlSchemeMenu_->addAction(Tr("Blender (СКМ Orbit, Shift+СКМ Pan, G/R/S)", "Blender (MMB Orbit, Shift+MMB Pan, G/R/S)"));
    actSchemeBlender_->setCheckable(true);
    schemeGroup_->addAction(actSchemeBlender_);

    if (EditorSettings::GetControlScheme() == ControlScheme::Blender) {
        actSchemeBlender_->setChecked(true);
    } else {
        actSchemeUnrealUnity_->setChecked(true);
    }

    connect(actSchemeUnrealUnity_, &QAction::triggered, this, [] {
        EditorSettings::SetControlScheme(ControlScheme::UnrealUnity);
    });
    connect(actSchemeBlender_, &QAction::triggered, this, [] {
        EditorSettings::SetControlScheme(ControlScheme::Blender);
    });

    sceneMenu_->addSeparator();
    actCloseEditor_ = sceneMenu_->addAction(Tr("Закрыть редактор", "Close Editor"), this, [this] { close(); });


}

void EditorMainWindow::BuildToolBar()
{
    toolBar_ = addToolBar(Tr("Запуск", "Play"));
    toolBar_->setMovable(false);

    toolBar_->setObjectName(QStringLiteral("MainToolBar"));
    auto* leftSpacer = new QWidget(toolBar_);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar_->addWidget(leftSpacer);

    actPlay_ = toolBar_->addAction(EditorTheme::CreatePlayIcon(), QStringLiteral("▶ Play"));
    actPause_ = toolBar_->addAction(EditorTheme::CreatePauseIcon(), QStringLiteral("⏸ Pause"));
    actStop_ = toolBar_->addAction(EditorTheme::CreateStopIcon(), QStringLiteral("⏹ Stop"));
    actExternal_ = toolBar_->addAction(EditorTheme::CreateExternalIcon(), Tr("Отдельное окно", "External"));

    toolBar_->addSeparator();

    cameraModeCombo_ = new QComboBox(toolBar_);
    cameraModeCombo_->setObjectName(QStringLiteral("PlayCameraMode"));
    cameraModeCombo_->addItem(Tr("Свободная камера", "Free Camera"), 0);
    cameraModeCombo_->addItem(Tr("Камера игрока", "Main Camera"), 1);
    cameraModeCombo_->setToolTip(Tr("Режим камеры во время Play: свободный полёт или следование за персонажем",
                                    "Camera mode during Play: free flight or follow character"));
    connect(cameraModeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (host_->PlayState() != EditorQtHost::Edit) return;
        const auto snapshot = host_->ReadSnapshot();
        uint64_t settingsId = 0;
        QJsonObject data{{"version", 1}, {"character", 0}, {"camera", 0}, {"standardControl", true}, {"cameraDistance", 5}, {"cameraHeight", 2}, {"lookSensitivity", 0.005}};
        for (const auto& o : snapshot.objects) {
            if (o.gameplayKind == 3 || o.type == "Gameplay.PlaySettings") {
                settingsId = o.id;
                data = QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object();
                break;
            }
        }
        if (index == 0) {
            data["character"] = 0;
            data["standardControl"] = false;
        } else {
            uint64_t charId = 0;
            for (const auto& o : snapshot.objects) {
                if (o.gameplayKind == 1) { charId = o.id; break; }
            }
            data["character"] = static_cast<qint64>(charId);
            data["camera"] = 0;
            data["standardControl"] = true;
        }
        EditorCommand c;
        c.type = EditorCommand::ConfigurePlay;
        c.objectId = settingsId;
        c.path = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
        host_->PostCommand(c);
    });
    toolBar_->addWidget(cameraModeCombo_);

    auto* configPlayBtn = new QPushButton(toolBar_);
    configPlayBtn->setText(QStringLiteral("⚙"));
    configPlayBtn->setToolTip(Tr("Детальные настройки Play…", "Detailed Play Settings…"));
    connect(configPlayBtn, &QPushButton::clicked, this, [this] {
        if (actConfigurePlay_) actConfigurePlay_->trigger();
    });
    toolBar_->addWidget(configPlayBtn);
    auto* rightSpacer = new QWidget(toolBar_);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar_->addWidget(rightSpacer);

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

    const bool canPlay = host_->PlayState() == 1;
    actPlay_->setEnabled(canPlay);
    actPause_->setEnabled(canPlay);
    actStop_->setEnabled(canPlay);

    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar_->addWidget(spacer);

    langButton_ = new QPushButton(this);
    langButton_->setObjectName(QStringLiteral("LangToggleBtn"));
    langButton_->setFixedWidth(40);
    langButton_->setText(EditorLocale::Instance().Language() == EditorLanguage::Ru ? QStringLiteral("RU") : QStringLiteral("EN"));
    langButton_->setToolTip(Tr("Переключить язык интерфейса (RU/EN)", "Toggle UI language (RU/EN)"));
    connect(langButton_, &QPushButton::clicked, this, [this] {
        if (EditorLocale::Instance().Language() == EditorLanguage::Ru) {
            EditorLocale::Instance().SetLanguage(EditorLanguage::En);
        } else {
            EditorLocale::Instance().SetLanguage(EditorLanguage::Ru);
        }
    });
    toolBar_->addWidget(langButton_);
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

QMenu* EditorMainWindow::BuildAddObjectMenu(QWidget* parent, const std::array<double, 3>& spawnPos)
{
    auto* menu = new QMenu(parent);

    // Spawn Location Mode toggle
    auto* locMenu = menu->addMenu(Tr("Точка спавна", "Spawn Location"));
    auto* locGroup = new QActionGroup(menu);
    locGroup->setExclusive(true);

    auto* actCursor = locMenu->addAction(Tr("На проекции курсора", "Under Cursor"));
    actCursor->setCheckable(true);
    locGroup->addAction(actCursor);

    auto* actOrigin = locMenu->addAction(Tr("В центре координат (0, 0, 0)", "World Origin (0,0,0)"));
    actOrigin->setCheckable(true);
    locGroup->addAction(actOrigin);

    if (EditorSettings::GetSpawnLocationMode() == SpawnLocationMode::WorldOrigin) {
        actOrigin->setChecked(true);
    } else {
        actCursor->setChecked(true);
    }

    connect(actCursor, &QAction::triggered, [] {
        EditorSettings::SetSpawnLocationMode(SpawnLocationMode::UnderCursor);
    });
    connect(actOrigin, &QAction::triggered, [] {
        EditorSettings::SetSpawnLocationMode(SpawnLocationMode::WorldOrigin);
    });

    menu->addSeparator();

    const auto snapshot = host_->ReadSnapshot();

    auto* mPrim = menu->addMenu(Tr("Примитивы", "Primitives"));
    auto* mLight = menu->addMenu(Tr("Свет", "Lights"));
    auto* mParticle = menu->addMenu(Tr("Частицы", "Particles"));
    auto* mEnv = menu->addMenu(Tr("Окружение", "Environment"));
    auto* mChar = menu->addMenu(Tr("Персонажи", "Characters"));
    auto* mCam = menu->addMenu(Tr("Камеры", "Cameras"));
    auto* mProj = menu->addMenu(Tr("Классы проекта", "Project Classes"));

    auto addItem = [this, spawnPos](QMenu* target, const QString& label, const QString& typeName) {
        target->addAction(label, this, [this, typeName, label, spawnPos] {
            if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit) return;
            EditorCommand c;
            c.type = EditorCommand::CreateObject;
            c.path = typeName;
            c.name = label;
            for (int i = 0; i < 3; ++i) c.transform.position[i] = static_cast<float>(spawnPos[i]);
            host_->PostCommand(c);
            if (viewport_) viewport_->ShowAdjustLastOperation(0, label, c.transform);
        });
    };

    int projCount = 0;
    for (const auto& t : snapshot.types) {
        const QString name = QString::fromStdString(t.name);
        const QString label = QString::fromStdString(t.label);
        if (name == QStringLiteral("Gameplay.PlaySettings")) continue;

        if (name.startsWith(QStringLiteral("Primitive."))) addItem(mPrim, label, name);
        else if (name.startsWith(QStringLiteral("Light."))) addItem(mLight, label, name);
        else if (name.startsWith(QStringLiteral("Particle."))) addItem(mParticle, label, name);
        else if (name.startsWith(QStringLiteral("Environment."))) addItem(mEnv, label, name);
        else if (name.startsWith(QStringLiteral("Gameplay.Character"))) addItem(mChar, label, name);
        else if (name.startsWith(QStringLiteral("Gameplay.Camera"))) addItem(mCam, label, name);
        else {
            addItem(mProj, label, name);
            projCount++;
        }
    }

    if (mPrim->isEmpty()) {
        addItem(mPrim, Tr("Куб", "Cube"), QStringLiteral("Primitive.Cube"));
        addItem(mPrim, Tr("Сфера", "Sphere"), QStringLiteral("Primitive.Sphere"));
        addItem(mPrim, Tr("Плоскость", "Plane"), QStringLiteral("Primitive.Plane"));
    }
    if (mLight->isEmpty()) {
        addItem(mLight, Tr("Точечный свет", "Point Light"), QStringLiteral("Light.Point"));
    }
    if (projCount == 0) {
        mProj->setEnabled(false);
    }

    return menu;
}

void EditorMainWindow::ShowViewportAddMenu(const QPoint& globalPos)
{
    auto* menu = BuildAddObjectMenu(this, viewport_ ? viewport_->lastSpawnPos() : std::array<double, 3>{0, 0, 0});
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->exec(globalPos);
}

void EditorMainWindow::ShowViewportContextMenu(const QPoint& globalPos)
{
    UpdatePlayButtons();

    QMenu menu(this);

    auto* addSubMenu = BuildAddObjectMenu(&menu, viewport_ ? viewport_->lastSpawnPos() : std::array<double, 3>{0, 0, 0});
    addSubMenu->setTitle(Tr("+ Добавить объект", "+ Add Object"));
    menu.addMenu(addSubMenu);

    menu.addSeparator();

    if (duplicateAction_)
        menu.addAction(duplicateAction_);

    if (deleteAction_)
        menu.addAction(deleteAction_);

    auto* actFocus = menu.addAction(Tr("Сфокусировать камеру (F)", "Focus Selected (F)"), this, [this] {
        if (host_->HasSelection()) {
            EditorCommand c; c.type = EditorCommand::FocusObject; host_->PostCommand(c);
        }
    });
    actFocus->setEnabled(host_->HasSelection());

    menu.addSeparator();

    auto* gizmoMenu = menu.addMenu(Tr("Режим трансформации", "Gizmo Mode"));
    gizmoMenu->addAction(Tr("Перемещение (1 / W / G)", "Translate (1 / W / G)"), this, [this] {
        if (viewport_) viewport_->setGizmoMode(EditorGizmoTranslate);
    });
    gizmoMenu->addAction(Tr("Вращение (2 / E / R)", "Rotate (2 / E / R)"), this, [this] {
        if (viewport_) viewport_->setGizmoMode(EditorGizmoRotate);
    });
    gizmoMenu->addAction(Tr("Масштаб (3 / R / S)", "Scale (3 / R / S)"), this, [this] {
        if (viewport_) viewport_->setGizmoMode(EditorGizmoScale);
    });

    menu.addSeparator();

    if (actPlay_)
        menu.addAction(actPlay_);

    if (actPause_)
        menu.addAction(actPause_);

    if (actStop_)
        menu.addAction(actStop_);

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
    sceneObjectsDock_ = new QDockWidget(Tr("Сцена / объекты", "Scene / Objects"), this);
    sceneObjectsDock_->setObjectName(QStringLiteral("SceneObjectsDock"));
    sceneObjectsDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    sceneObjectsDock_->setFeatures(
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable |
        QDockWidget::DockWidgetClosable
    );

    sceneObjectsDock_->setMinimumSize(80, 60);

    inspectorDock_ = new QDockWidget(Tr("Свойства объекта", "Object Properties"), this);
    inspectorDock_->setObjectName(QStringLiteral("InspectorDock"));
    inspectorDock_->setMinimumSize(80, 60);
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
    if (sceneObjectsPanel_) {
        sceneObjectsPanel_->setMinimumSize(80, 60);
        sceneObjectsPanel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }
    if (objectInspectorPanel_) {
        objectInspectorPanel_->setMinimumSize(80, 60);
        objectInspectorPanel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }

    ConnectPanels();
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

    connect(sceneObjectsPanel_, &SceneObjectsPanel::SetParentRequested, this,
            [this](quint64 childId, quint64 parentId)
            {
                if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                    return;
                host_->SetParent(childId, parentId);
            });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::AddChildPrimitiveRequested, this,
            [this](quint64 parentId, const QString& typeName, const QString& typeLabel, const std::array<double, 3>& position)
            {
                if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                    return;

                EditorCommand c;
                c.type = EditorCommand::CreateObject;
                c.path = typeName;
                c.name = typeLabel;
                c.objectId = parentId;
                c.parentId = parentId;

                for (int i = 0; i < 3; ++i)
                    c.transform.position[i] = static_cast<float>(position[i]);

                host_->PostCommand(c);
                if (viewport_) viewport_->ShowAdjustLastOperation(0, typeLabel, c.transform);
            });

    connect(sceneObjectsPanel_, &SceneObjectsPanel::RenameRequested, this,
            [this](quint64 id, const QString& newName)
            {
                if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                    return;
                if (newName.trimmed().isEmpty())
                    return;

                EditorCommand c;
                c.type = EditorCommand::RenameObject;
                c.objectId = id;
                c.name = newName.trimmed();
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

    connect(objectInspectorPanel_, &ObjectInspectorPanel::FocusRequested, this, [this](quint64 id) {
        if (!id || !host_->HasScene()) return;
        EditorCommand c; c.type = EditorCommand::FocusObject; host_->PostCommand(c);
    });
    connect(objectInspectorPanel_, &ObjectInspectorPanel::DuplicateRequested, this, [this](quint64 id) {
        if (!id || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        EditorCommand c; c.type = EditorCommand::DuplicateObject; c.objectId = id; host_->PostCommand(c);
    });
    connect(objectInspectorPanel_, &ObjectInspectorPanel::DeleteRequested, this, [this](quint64 id) {
        if (!id || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        EditorCommand c; c.type = EditorCommand::RemoveObject; c.objectId = id; host_->PostCommand(c);
    });
    connect(objectInspectorPanel_, &ObjectInspectorPanel::ClearModelRequested, this, [this](quint64 id) {
        if (!id || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        EditorCommand c; c.type = EditorCommand::SetModel; c.objectId = id; c.path.clear(); host_->PostCommand(c);
    });
    connect(objectInspectorPanel_, &ObjectInspectorPanel::CreateClonesRequested, this,
        [this](quint64 sourceId, int count, double ox, double oy, double oz) {
            if (!sourceId || count < 2 || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
            for (int i = 1; i < count; ++i) {
                EditorCommand c;
                c.type = EditorCommand::DuplicateObject;
                c.objectId = sourceId;
                host_->PostCommand(c);
            }
        });
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
        if (viewport_ && view.selected) viewport_->SetAdjustLastOperationObjectId(view.selected);
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
    setWindowTitle(QStringLiteral("%1 — %2")
        .arg(Tr("Редактор PainEngine", "PainEngine Editor"), QString::fromUtf8(host_->host().projectName)));

    if (viewportDock_) viewportDock_->setWindowTitle(Tr("Вьюпорт", "Viewport"));
    if (viewport_) viewport_->RetranslateUi();
    if (consoleDock_) consoleDock_->setWindowTitle(Tr("Консоль", "Console"));
    if (fileDock_) fileDock_->setWindowTitle(Tr("Контент", "Content Browser"));
    if (sceneObjectsDock_) sceneObjectsDock_->setWindowTitle(Tr("Сцена / объекты", "Scene / Objects"));
    if (inspectorDock_) inspectorDock_->setWindowTitle(Tr("Свойства объекта", "Object Properties"));

    if (sceneMenu_) sceneMenu_->setTitle(Tr("Сцена", "Scene"));
    if (editMenu_) editMenu_->setTitle(Tr("Правка", "Edit"));
    if (controlSchemeMenu_) controlSchemeMenu_->setTitle(Tr("Схема управления", "Control Scheme"));
    if (actSchemeUnrealUnity_) actSchemeUnrealUnity_->setText(Tr("Unreal / Unity (ПКМ + WASD, W/E/R)", "Unreal / Unity (RMB + WASD, W/E/R)"));
    if (actSchemeBlender_) actSchemeBlender_->setText(Tr("Blender (СКМ Orbit, Shift+СКМ Pan, G/R/S)", "Blender (MMB Orbit, Shift+MMB Pan, G/R/S)"));
    if (addMenu_) addMenu_->setTitle(Tr("Добавить", "Add"));
    if (playMenu_) playMenu_->setTitle(Tr("Запуск", "Play"));
    if (helpMenu_) helpMenu_->setTitle(Tr("Справка", "Help"));
    if (actDoc_) actDoc_->setText(Tr("Документация (F1)", "Documentation (F1)"));
    if (cameraModeCombo_) {
        const int cur = cameraModeCombo_->currentIndex();
        cameraModeCombo_->setItemText(0, Tr("Свободная камера", "Free Camera"));
        cameraModeCombo_->setItemText(1, Tr("Камера игрока", "Main Camera"));
        cameraModeCombo_->setCurrentIndex(cur);
    }
    if (viewMenu_) viewMenu_->setTitle(Tr("Вид", "View"));
    if (langMenu_) langMenu_->setTitle(Tr("Язык", "Language"));
    if (themeMenu_) themeMenu_->setTitle(Tr("Тема оформления", "Theme"));
    if (actThemeDark_) actThemeDark_->setText(Tr("Тёмная (Dark)", "Dark"));
    if (actThemeLight_) actThemeLight_->setText(Tr("Светлая (Light)", "Light"));
    if (accentMenu_) accentMenu_->setTitle(Tr("Акцентный цвет", "Accent Color"));
    if (actAccentCustom_) actAccentCustom_->setText(Tr("Выбрать свой цвет…", "Custom Color…"));
    if (resourcesMenu_) resourcesMenu_->setTitle(Tr("Ресурсы", "Assets"));
    if (projectMenu_) projectMenu_->setTitle(Tr("Проект", "Project"));

    if (actNewScene_) actNewScene_->setText(Tr("Новая сцена", "New Scene"));
    if (actOpenScene_) actOpenScene_->setText(Tr("Открыть сцену…", "Open Scene…"));
    if (saveAction_) saveAction_->setText(Tr("Сохранить и сделать стартовой", "Save and Set as Startup"));
    if (actSaveAs_) actSaveAs_->setText(Tr("Сохранить как…", "Save As…"));
    if (actAddModel_) actAddModel_->setText(Tr("Добавить модель…", "Add Model…"));
    if (actImportModel_) actImportModel_->setText(Tr("Импорт модели…", "Import Model…"));
    if (actImportTexture_) actImportTexture_->setText(Tr("Импорт текстуры…", "Import Texture…"));
    if (actReloadShaders_) actReloadShaders_->setText(Tr("Перекомпилировать шейдеры", "Recompile Shaders"));
    if (actCloseEditor_) actCloseEditor_->setText(Tr("Закрыть редактор", "Close Editor"));

    if (undoAction_) undoAction_->setText(Tr("Отменить", "Undo"));
    if (redoAction_) redoAction_->setText(Tr("Повторить", "Redo"));
    if (duplicateAction_) duplicateAction_->setText(Tr("Дублировать объект", "Duplicate Object"));
    if (deleteAction_) deleteAction_->setText(Tr("Удалить объект", "Remove Object"));

    if (actServiceObjects_) actServiceObjects_->setText(Tr("Служебные объекты сцены", "Scene Service Objects"));
    if (actConfigurePlay_) actConfigurePlay_->setText(Tr("Настроить Play…", "Configure Play…"));
    if (actExternal_) {
        const bool running = (gameProc_ && gameProc_->state() != QProcess::NotRunning);
        actExternal_->setText(running ? Tr("Остановить отдельную игру", "Stop External Game") : Tr("Отдельное окно", "External"));
    }

    if (langButton_) {
        langButton_->setText(EditorLocale::Instance().Language() == EditorLanguage::Ru ? QStringLiteral("RU") : QStringLiteral("EN"));
        langButton_->setToolTip(Tr("Переключить язык интерфейса (RU/EN)", "Toggle UI language (RU/EN)"));
    }
    if (actLangRu_) actLangRu_->setChecked(EditorLocale::Instance().Language() == EditorLanguage::Ru);
    if (actLangEn_) actLangEn_->setChecked(EditorLocale::Instance().Language() == EditorLanguage::En);
    if (actResetLayout_) actResetLayout_->setText(Tr("Сбросить раскладку окон", "Reset Window Layout"));
}

void EditorMainWindow::ApplyDefaultLayout()
{
    if (viewportDock_ && consoleDock_ && fileDock_) {
        addDockWidget(Qt::TopDockWidgetArea, viewportDock_);
        addDockWidget(Qt::BottomDockWidgetArea, consoleDock_);
        addDockWidget(Qt::BottomDockWidgetArea, fileDock_);
        tabifyDockWidget(consoleDock_, fileDock_);
        fileDock_->raise();
    }

    if (sceneObjectsDock_ && inspectorDock_) {
        addDockWidget(Qt::LeftDockWidgetArea, sceneObjectsDock_);
        addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
        if (viewportDock_) {
            splitDockWidget(sceneObjectsDock_, viewportDock_, Qt::Horizontal);
            splitDockWidget(viewportDock_, inspectorDock_, Qt::Horizontal);
            resizeDocks({sceneObjectsDock_, viewportDock_, inspectorDock_}, {250, 700, 320}, Qt::Horizontal);
        } else {
            resizeDocks({sceneObjectsDock_, inspectorDock_}, {280, 300}, Qt::Horizontal);
        }
    }
    if (viewportDock_ && consoleDock_) {
        resizeDocks({viewportDock_, consoleDock_}, {620, 100}, Qt::Vertical);
    }
}

void EditorMainWindow::ApplySavedOrInitialLayout()
{
    const bool isTest = qEnvironmentVariableIsSet("PAIN_EDITOR_TEST_MODE") ||
        qApp->applicationFilePath().contains(QStringLiteral("Tests"), Qt::CaseInsensitive) ||
        (host_ && host_->host().projectName && std::string(host_->host().projectName).find("UI") != std::string::npos);
    if (isTest) {
        return;
    }

    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    const QByteArray savedState = settings.value(QStringLiteral("window/state")).toByteArray();
    const QByteArray savedGeom = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!savedState.isEmpty() && restoreState(savedState)) {
        if (!savedGeom.isEmpty()) {
            restoreGeometry(savedGeom);
        }
    } else {
        ApplyDefaultLayout();
    }
}


void EditorMainWindow::SpawnObjectByType(const QString& typeName, const QString& typeLabel)
{
    if (sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit) return;
    EditorCommand c;
    c.type = EditorCommand::CreateObject;
    c.path = typeName;
    c.name = typeLabel;
    if (EditorSettings::GetSpawnLocationMode() == SpawnLocationMode::UnderCursor && viewport_) {
        const auto pos = viewport_->lastSpawnPos();
        for (int i = 0; i < 3; ++i) c.transform.position[i] = static_cast<float>(pos[i]);
    } else if (sceneObjectsPanel_) {
        const auto pos = sceneObjectsPanel_->SpawnPosition();
        for (int i = 0; i < 3; ++i) c.transform.position[i] = static_cast<float>(pos[i]);
    } else {
        c.transform.position[0] = 0.0f;
        c.transform.position[1] = 0.0f;
        c.transform.position[2] = 0.0f;
    }
    host_->PostCommand(c);
    if (viewport_) viewport_->ShowAdjustLastOperation(0, typeLabel, c.transform);
}

void EditorMainWindow::RebuildAddMenu()
{
    if (!addMenu_) return;
    addMenu_->clear();

    auto* locMenu = addMenu_->addMenu(Tr("Точка спавна", "Spawn Location"));
    auto* locGroup = new QActionGroup(addMenu_);
    locGroup->setExclusive(true);
    auto* actCursor = locMenu->addAction(Tr("На проекции курсора", "Under Cursor"));
    actCursor->setCheckable(true);
    locGroup->addAction(actCursor);
    auto* actOrigin = locMenu->addAction(Tr("В центре координат (0, 0, 0)", "World Origin (0,0,0)"));
    actOrigin->setCheckable(true);
    locGroup->addAction(actOrigin);
    if (EditorSettings::GetSpawnLocationMode() == SpawnLocationMode::WorldOrigin) actOrigin->setChecked(true);
    else actCursor->setChecked(true);
    connect(actCursor, &QAction::triggered, [] { EditorSettings::SetSpawnLocationMode(SpawnLocationMode::UnderCursor); });
    connect(actOrigin, &QAction::triggered, [] { EditorSettings::SetSpawnLocationMode(SpawnLocationMode::WorldOrigin); });

    addMenu_->addSeparator();

    const auto snapshot = host_->ReadSnapshot();

    auto* mPrim = addMenu_->addMenu(Tr("Примитивы", "Primitives"));
    auto* mLight = addMenu_->addMenu(Tr("Свет", "Lights"));
    auto* mParticle = addMenu_->addMenu(Tr("Частицы", "Particles"));
    auto* mEnv = addMenu_->addMenu(Tr("Окружение", "Environment"));
    auto* mChar = addMenu_->addMenu(Tr("Персонажи", "Characters"));
    auto* mCam = addMenu_->addMenu(Tr("Камеры", "Cameras"));
    auto* mProj = addMenu_->addMenu(Tr("Классы проекта", "Project Classes"));

    auto addItem = [this](QMenu* target, const QString& label, const QString& typeName) {
        target->addAction(label, this, [this, typeName, label] {
            SpawnObjectByType(typeName, label);
        });
    };

    int projCount = 0;
    for (const auto& t : snapshot.types) {
        const QString name = QString::fromStdString(t.name);
        const QString label = QString::fromStdString(t.label);
        if (name == QStringLiteral("Gameplay.PlaySettings")) continue;

        if (name.startsWith(QStringLiteral("Primitive."))) addItem(mPrim, label, name);
        else if (name.startsWith(QStringLiteral("Light."))) addItem(mLight, label, name);
        else if (name.startsWith(QStringLiteral("Particle."))) addItem(mParticle, label, name);
        else if (name.startsWith(QStringLiteral("Environment."))) addItem(mEnv, label, name);
        else if (name.startsWith(QStringLiteral("Gameplay.Character"))) addItem(mChar, label, name);
        else if (name.startsWith(QStringLiteral("Gameplay.Camera"))) addItem(mCam, label, name);
        else {
            addItem(mProj, label, name);
            projCount++;
        }
    }

    if (mPrim->isEmpty()) {
        addItem(mPrim, Tr("Куб", "Cube"), QStringLiteral("Primitive.Cube"));
        addItem(mPrim, Tr("Сфера", "Sphere"), QStringLiteral("Primitive.Sphere"));
        addItem(mPrim, Tr("Плоскость", "Plane"), QStringLiteral("Primitive.Plane"));
    }
    if (mLight->isEmpty()) {
        addItem(mLight, Tr("Точечный свет", "Point Light"), QStringLiteral("Light.Point"));
    }
    if (projCount == 0) {
        mProj->setEnabled(false);
    }

    addMenu_->addSeparator();
    addMenu_->addAction(Tr("3D Модель из файла…", "3D Model from file…"), this, [this] {
        AddModel();
    });
}
