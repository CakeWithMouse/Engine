#include "MainWindow.h"
#include "QtHost.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QGroupBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QQuaternion>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <map>

void EditorMainWindow::BuildSceneTools()
{
    auto* menu = menuBar()->addMenu(QStringLiteral("Сцена"));

    for(auto* action : menuBar()->actions()) if(action->text()==QStringLiteral("Вид") && action->menu()) {
        auto* service=action->menu()->addAction(QStringLiteral("Служебные объекты сцены"));service->setCheckable(true);
        connect(service,&QAction::toggled,this,[this](bool visible){
            showServiceObjects_=visible;hierarchyKey_.clear();
            if(!visible)for(const auto& o:host_->ReadSnapshot().objects)if(o.id==selectedId_&&o.type=="Gameplay.PlaySettings"){
                EditorCommand c;c.type=EditorCommand::SelectObject;host_->PostCommand(c);
            }
            RefreshSceneTools();
        });
    }
    const bool available = host_->HasScene();
    menu->setEnabled(available);
    auto* playMenu = menuBar()->addMenu(QStringLiteral("Запуск"));
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

    // Delete работает только в иерархии, чтобы не мешать вводу текста в полях.
    removeShortcut_ = new QAction(this);
    removeShortcut_->setShortcut(QKeySequence::Delete);
    removeShortcut_->setShortcutContext(Qt::WidgetShortcut);

    if (!available)
    {
        AppendLine(QStringLiteral(
            "Preview проекта. Создайте свой проект в лаунчере для редактирования сцены."));

        if (sceneToolsDock_)
            sceneToolsDock_->hide();

        if (inspectorDock_)
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

    if (!sceneToolsDock_ || !inspectorDock_)
        return;

    ///////////////////////////////////////////////////////////////////
    // Левая панель: сцена / примитивы / иерархия
    ///////////////////////////////////////////////////////////////////

    scenePanel_ = new QWidget(sceneToolsDock_);
    auto* layout = new QVBoxLayout(scenePanel_);

    auto* placement = new QGroupBox(QStringLiteral("Добавление объекта"), scenePanel_);
    auto* placementLayout = new QVBoxLayout(placement);
    layout->addWidget(placement);
    primitive_ = new QComboBox(scenePanel_);
    primitive_->setObjectName(QStringLiteral("PrimitiveType"));
    typeCategory_ = new QComboBox(scenePanel_);
    typeCategory_->setObjectName(QStringLiteral("ObjectCategory"));
    typeCategory_->addItem(QStringLiteral("Примитивы"), "Primitive.");
    typeCategory_->addItem(QStringLiteral("Свет"), "Light.");
    typeCategory_->addItem(QStringLiteral("Частицы"), "Particle.");
    typeCategory_->addItem(QStringLiteral("Окружение"), "Environment.");
    typeCategory_->addItem(QStringLiteral("Персонажи"), "Gameplay.Character");
    typeCategory_->addItem(QStringLiteral("Камеры"), "Gameplay.Camera");
    typeCategory_->addItem(QStringLiteral("Классы проекта"), "project");
    typeCategory_->addItem(QStringLiteral("Все объекты"), "all");
    const auto fillTypes = [this] {
        primitive_->clear();
        const auto category=typeCategory_->currentData().toString();
        for (const auto& type : host_->ReadSnapshot().types) {
            const auto name=QString::fromStdString(type.name);
            if (name == "Gameplay.PlaySettings") continue;
            const bool standard=name.startsWith("Primitive.")||name.startsWith("Light.")||name.startsWith("Particle.")||name.startsWith("Environment.")||name.startsWith("Gameplay.");
            if (category=="all" || name.startsWith(category) || (category=="project"&&!standard))
                primitive_->addItem(QString::fromStdString(type.label),name);
        }
        if(addPrimitiveButton_)addPrimitiveButton_->setEnabled(primitive_->count()!=0);
    };
    connect(typeCategory_,qOverload<int>(&QComboBox::currentIndexChanged),this,[fillTypes](int){fillTypes();});
    fillTypes();
    placementLayout->addWidget(new QLabel(QStringLiteral("Категория"), placement));
    placementLayout->addWidget(typeCategory_);
    placementLayout->addWidget(new QLabel(QStringLiteral("Объект из категории"), placement));
    placementLayout->addWidget(primitive_);

    placementLayout->addWidget(new QLabel(QStringLiteral("Точка добавления — X / Y / Z"), scenePanel_));

    auto* spawnRow = new QHBoxLayout;

    for (int i = 0; i < 3; ++i)
    {
        spawn_[i] = new QDoubleSpinBox(scenePanel_);
        spawn_[i]->setRange(-10000, 10000);
        spawn_[i]->setDecimals(2);
        spawn_[i]->setSingleStep(0.5);

        spawnRow->addWidget(spawn_[i]);
    }

    spawn_[0]->setValue(2);

    placementLayout->addLayout(spawnRow);

    addPrimitiveButton_ = new QPushButton(QStringLiteral("+ Добавить объект"), scenePanel_);
    addPrimitiveButton_->setObjectName(QStringLiteral("AddPrimitive"));
    placementLayout->addWidget(addPrimitiveButton_);

    hierarchy_ = new QTreeWidget(scenePanel_);
    hierarchy_->setHeaderLabels({QStringLiteral("Объект"), QStringLiteral("Тип")});
    hierarchy_->setSelectionMode(QAbstractItemView::SingleSelection);
    hierarchy_->setStyleSheet(QStringLiteral("QTreeView::item:selected { background: #a66008; color: white; }"));

    if (removeShortcut_)
        hierarchy_->addAction(removeShortcut_);

    layout->addWidget(hierarchy_, 1);

    hierarchy_->setContextMenuPolicy(Qt::ActionsContextMenu);
    hierarchy_->addAction(duplicateAction_);
    hierarchy_->addAction(deleteAction_);
    placement->setToolTip(QStringLiteral("Категория меняет список объектов для добавления. Выбранный объект сцены не изменяется."));

    sceneToolsDock_->setWidget(scenePanel_);

    ///////////////////////////////////////////////////////////////////
    // Правая панель: свойства объекта
    ///////////////////////////////////////////////////////////////////

    inspector_ = new QWidget(inspectorDock_);
    auto* inspectorLayout = new QVBoxLayout(inspector_);
    auto group = [this, inspectorLayout](const QString& title) {
        auto* box = new QGroupBox(title, inspector_);
        inspectorLayout->addWidget(box);
        return new QFormLayout(box);
    };
    auto* form = group(QStringLiteral("Общие"));
    selectionLabel_=new QLabel(QStringLiteral("Объект не выбран"),inspector_);
    selectionLabel_->setObjectName(QStringLiteral("SelectedObjectLabel"));
    selectionLabel_->setWordWrap(true);
    selectionLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #efa52b;"));
    form->addRow(selectionLabel_);

    objectName_ = new QLineEdit(inspector_);
    objectName_->setMaxLength(120);
    form->addRow(QStringLiteral("Имя"), objectName_);

    const QString labels[] =
    {
        QStringLiteral("X"),
        QStringLiteral("Y"),
        QStringLiteral("Z"),
        QStringLiteral("X, °"),
        QStringLiteral("Y, °"),
        QStringLiteral("Z, °"),
        QStringLiteral("X"),
        QStringLiteral("Y"),
        QStringLiteral("Z")
    };

    for (int i = 0; i < 9; ++i)
    {
        if (i % 3 == 0) form = group(i == 0 ? QStringLiteral("Позиция") : i == 3 ? QStringLiteral("Поворот") : QStringLiteral("Масштаб"));
        auto* spin = transform_[i] = new QDoubleSpinBox(inspector_);

        spin->setObjectName(QStringLiteral("Transform%1").arg(i));
        spin->setRange(i >= 6 ? 0.001 : -10000, 10000);
        spin->setDecimals(3);
        spin->setSingleStep(i >= 3 && i < 6 ? 15 : 0.25);
        spin->setKeyboardTracking(false);

        form->addRow(labels[i], spin);
    }

    form = group(QStringLiteral("Визуал"));
    BuildAssetInspector(form);
    auto* lightGroup = new QGroupBox(QStringLiteral("Точечный свет"), inspector_);
    lightInspector_ = lightGroup;
    auto* lightForm = new QFormLayout(lightGroup);
    lightColor_ = new QPushButton(QStringLiteral("Выбрать цвет…"), lightGroup);
    lightColor_->setObjectName(QStringLiteral("LightColor"));
    lightForm->addRow(lightColor_);
    const QString lightLabels[] = {QStringLiteral("Красный"), QStringLiteral("Зелёный"),
        QStringLiteral("Синий"), QStringLiteral("Интенсивность"), QStringLiteral("Радиус")};
    for (int i = 0; i < 5; ++i)
    {
        auto* spin = lightValues_[i] = new QDoubleSpinBox(lightGroup);
        spin->setObjectName(QStringLiteral("LightValue%1").arg(i));
        spin->setRange(i == 4 ? 0.01 : 0, i < 3 ? 1 : 10000);
        spin->setDecimals(3);
        spin->setSingleStep(i < 3 ? 0.05 : 0.5);
        spin->setKeyboardTracking(false);
        lightForm->addRow(lightLabels[i], spin);
        connect(spin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::ApplyPointLight);
    }
    lightEnabled_ = new QCheckBox(QStringLiteral("Включён"), lightGroup);
    lightEnabled_->setObjectName(QStringLiteral("LightEnabled"));
    lightForm->addRow(lightEnabled_);
    connect(lightEnabled_, &QCheckBox::clicked, this, &EditorMainWindow::ApplyPointLight);
    connect(lightColor_, &QPushButton::clicked, this, [this] {
        if (!selectedId_ || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        const auto id = selectedId_;
        const auto color = QColor::fromRgbF(lightValues_[0]->value(), lightValues_[1]->value(), lightValues_[2]->value());
        auto* dialog = new QColorDialog(color, this);
        dialog->setWindowTitle(QStringLiteral("Цвет источника света"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setOption(QColorDialog::DontUseNativeDialog);
        connect(dialog, &QColorDialog::colorSelected, this, [this, id](const QColor& selected) {
            if (sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
            for (const auto& object : host_->ReadSnapshot().objects) if (object.id == id && object.hasPointLight)
            {
                EditorCommand c; c.type = EditorCommand::SetPointLight; c.objectId = id; c.pointLight = object.pointLight;
                c.pointLight.color[0] = selected.redF(); c.pointLight.color[1] = selected.greenF(); c.pointLight.color[2] = selected.blueF();
                host_->PostCommand(c);
                break;
            }
        });
        dialog->open();
    });
    form->addRow(lightGroup);
    lightGroup->hide();
    auto* characterGroup = new QGroupBox(QStringLiteral("Персонаж"), inspector_);
    characterInspector_ = characterGroup;
    auto* characterForm = new QFormLayout(characterGroup);
    characterSpeed_ = new QDoubleSpinBox(characterGroup);
    characterSpeed_->setObjectName("CharacterSpeed");
    characterSpeed_->setRange(0, 10000); characterSpeed_->setKeyboardTracking(false);
    characterForm->addRow(QStringLiteral("Скорость, ед/с"), characterSpeed_);
    inspectorLayout->addWidget(characterGroup);
    connect(characterSpeed_, &QDoubleSpinBox::editingFinished, this, [this] {
        if (refreshing_ || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        for (const auto& o : host_->ReadSnapshot().objects) if (o.id == selectedId_ && o.gameplayKind == 1) {
            auto data = QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object();
            if (data["moveSpeed"].toDouble(5) == characterSpeed_->value()) return;
            data["moveSpeed"] = characterSpeed_->value();
            EditorCommand c; c.type = EditorCommand::SetProjectData; c.objectId = o.id;
            c.path = QString::fromUtf8(QJsonDocument(data).toJson()); host_->PostCommand(c);
        }
    });
    inspectorLayout->addStretch();
    auto* scroll = new QScrollArea(inspectorDock_); scroll->setWidgetResizable(true);
    scroll->setWidget(inspector_); inspectorDock_->setWidget(scroll);
}

void EditorMainWindow::SubmitObjectCommand(int type)
{
    if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    EditorCommand c;
    c.type = static_cast<EditorCommand::Type>(type);
    c.objectId = selectedId_;
    if (!c.objectId && (c.type == EditorCommand::DuplicateObject || c.type == EditorCommand::RemoveObject)) return;
    host_->PostCommand(c);
}

void EditorMainWindow::ApplyTransform()
{
    if (refreshing_ || !selectedId_ || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    EditorCommand c;
    c.type = EditorCommand::UpdateObjectTransform;
    c.objectId = selectedId_;
    for (int i = 0; i < 3; ++i)
    {
        c.transform.position[i] = static_cast<float>(transform_[i]->value());
        c.transform.scale[i] = static_cast<float>(transform_[i + 6]->value());
    }
    const auto q = QQuaternion::fromEulerAngles(static_cast<float>(transform_[3]->value()),
                                                static_cast<float>(transform_[4]->value()),
                                                static_cast<float>(transform_[5]->value()));
    c.transform.rotation[0] = q.x();
    c.transform.rotation[1] = q.y();
    c.transform.rotation[2] = q.z();
    c.transform.rotation[3] = q.scalar();
    // Focus changes alone must not add an undo step or mark the document dirty.
    for (const auto& o : host_->ReadSnapshot().objects)
        if (o.id == selectedId_)
        {
            bool changed = false;
            for (int i = 0; i < 3; ++i) changed |= std::abs(c.transform.position[i] - o.transform.position[i]) > 0.0006f
                || std::abs(c.transform.scale[i] - o.transform.scale[i]) > 0.0006f;
            const auto& r = o.transform.rotation;
            changed |= std::abs(QQuaternion::dotProduct(q, QQuaternion(r[3], r[0], r[1], r[2]).normalized())) <
                0.999999f;
            if (!changed) return;
        }
    host_->PostCommand(c);
}

void EditorMainWindow::RefreshSceneTools()
{
    if (!hierarchy_ || !host_->HasScene()) return;
    const auto view = host_->ReadSnapshot();
    refreshing_ = true;
    const bool edit = host_->PlayState() == EditorQtHost::Edit && !sceneBusy_;
    scenePanel_->setEnabled(edit);
    undoAction_->setEnabled(edit && view.canUndo);
    redoAction_->setEnabled(edit && view.canRedo);
    duplicateAction_->setEnabled(edit && view.selected);
    deleteAction_->setEnabled(edit && view.selected);
    saveAction_->setEnabled(!sceneBusy_);
    setWindowTitle(QStringLiteral("%1%2 — %3 — Engine Editor").arg(view.dirty ? QStringLiteral("* ") : QString(),
                                                                   scenePath_.isEmpty()
                                                                       ? QStringLiteral("Новая сцена")
                                                                       : QFileInfo(scenePath_).fileName(),
                                                                   QString::fromUtf8(host_->host().projectName)));
    QString key;
    for (const auto& o : view.objects) key += QStringLiteral("%1/%2/%3:%4;").arg(o.id).arg(o.parent).arg(o.name.size()).
                                                                             arg(QString::fromStdString(o.name));
    if (key != hierarchyKey_)
    {
        hierarchy_->clear();
        std::map<uint64_t, QTreeWidgetItem*> items;
        for (const auto& o : view.objects)
        {
            if (!showServiceObjects_ && (o.gameplayKind == 3 || o.type == "Gameplay.PlaySettings")) continue;
            auto* item = new QTreeWidgetItem({QString::fromStdString(o.name), QString::fromStdString(o.type)});
            item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(o.id));
            items[o.id] = item;
        }
        for (const auto& o : view.objects)
        {
            if (!items.count(o.id)) continue;
            if (o.parent && items.count(o.parent)) items[o.parent]->addChild(items[o.id]);
            else hierarchy_->addTopLevelItem(items[o.id]);
        }
        hierarchy_->expandAll();
        hierarchyKey_ = key;
    }
    for (QTreeWidgetItemIterator it(hierarchy_); *it; ++it) (*it)->setSelected(
        (*it)->data(0, Qt::UserRole).toULongLong() == view.selected);
    const bool selectionChanged = selectedId_ != view.selected;
    selectedId_ = view.selected;
    selectionLabel_->setText(QStringLiteral("Объект не выбран"));
    inspectorDock_->setWindowTitle(QStringLiteral("Свойства — нет выбора"));
    const auto* focus = QApplication::focusWidget();
    inspector_->setEnabled(edit && selectedId_);
    modelAsset_->clear(); textureAsset_->clear();
    bool selectedLight = false;
    bool selectedCharacter = false;
    for (const auto& o : view.objects) if (o.id == selectedId_)
    {
        selectionLabel_->setText(QStringLiteral("Выбран: %1\n%2 · ID %3").arg(QString::fromStdString(o.name),QString::fromStdString(o.type)).arg(o.id));
        inspectorDock_->setWindowTitle(QStringLiteral("Свойства — %1").arg(QString::fromStdString(o.name)));
        selectedLight = o.hasPointLight;
        selectedCharacter = o.gameplayKind == 1;
        if (o.gameplayKind == 1 && (selectionChanged || !focus || !characterInspector_->isAncestorOf(focus)))
            characterSpeed_->setValue(QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object()["moveSpeed"].toDouble(5));
        if (o.hasPointLight)
        {
            for (int i = 0; i < 5; ++i)
                if (selectionChanged || !focus || (focus != lightValues_[i] && !lightValues_[i]->isAncestorOf(focus)))
                    lightValues_[i]->setValue(i < 3 ? o.pointLight.color[i] : i == 3 ? o.pointLight.intensity : o.pointLight.radius);
            lightEnabled_->setChecked(o.pointLight.enabled != 0);
            const auto& color = o.pointLight.color;
            lightColor_->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                .arg(QColor::fromRgbF(color[0], color[1], color[2]).name())
                .arg(color[0] * 0.299 + color[1] * 0.587 + color[2] * 0.114 > 0.5 ? "black" : "white"));
        }
        inspector_->findChild<QPushButton*>("ChooseModel")->setEnabled(o.type == "Engine.Model" || o.type.rfind("Primitive.", 0) == 0);
        modelAsset_->setText(QString::fromStdString(o.modelAsset));
        textureAsset_->setText(QString::fromStdString(o.textureAsset));
    }
    lightInspector_->setVisible(selectedLight);
    characterInspector_->setVisible(selectedCharacter);
    if (selectionChanged || !focus || !inspector_->isAncestorOf(focus))
    {
        for (const auto& o : view.objects)
            if (o.id == selectedId_)
            {
                objectName_->setText(QString::fromStdString(o.name));
                const auto& t = o.transform;
                const auto euler = QQuaternion(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]).
                    toEulerAngles();
                for (int i = 0; i < 3; ++i)
                {
                    transform_[i]->setValue(t.position[i]);
                    transform_[i + 3]->setValue(euler[i]);
                    transform_[i + 6]->setValue(t.scale[i]);
                }
            }
    }
    refreshing_ = false;
    EditorCommand c;
    c.type = EditorCommand::RefreshScene;
    host_->PostCommand(c);
}

void EditorMainWindow::ApplyPointLight()
{
    if (refreshing_ || !selectedId_ || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    for (const auto& o : host_->ReadSnapshot().objects) if (o.id == selectedId_ && o.hasPointLight)
    {
        EditorCommand c; c.type = EditorCommand::SetPointLight; c.objectId = selectedId_; c.pointLight = o.pointLight;
        bool changed = false;
        for (int i = 0; i < 3; ++i)
        {
            c.pointLight.color[i] = static_cast<float>(lightValues_[i]->value());
            changed |= std::abs(c.pointLight.color[i] - o.pointLight.color[i]) > 0.0006f;
        }
        c.pointLight.intensity = static_cast<float>(lightValues_[3]->value());
        c.pointLight.radius = static_cast<float>(lightValues_[4]->value());
        c.pointLight.enabled = lightEnabled_->isChecked() ? 1u : 0u;
        changed |= std::abs(c.pointLight.intensity - o.pointLight.intensity) > 0.0006f ||
            std::abs(c.pointLight.radius - o.pointLight.radius) > 0.0006f || c.pointLight.enabled != o.pointLight.enabled;
        if (changed) host_->PostCommand(c);
        return;
    }
}

void EditorMainWindow::MaybeSave(std::function<void()> after)
{
    if (checkingChanges_) return;
    // Commit the active text/number field, then wait behind every queued mutation.
    if (auto* focus = QApplication::focusWidget(); focus && inspector_ && inspector_->isAncestorOf(focus)) focus->
        clearFocus();
    checkingChanges_ = true;
    afterCheck_ = std::move(after);
    EditorCommand c;
    c.type = EditorCommand::RefreshScene;
    c.name = QStringLiteral("confirm");
    host_->PostCommand(c);
}

void EditorMainWindow::SaveScene(bool saveAs, std::function<void()> after)
{
    if (!host_->HasScene() || sceneBusy_) return;
    if (auto* focus = QApplication::focusWidget(); focus && inspector_ && inspector_->isAncestorOf(focus)) focus->
        clearFocus();
    QString path = scenePath_;
    if (saveAs || path.isEmpty())
    {
        path = QFileDialog::getSaveFileName(this, QStringLiteral("Сохранить стартовую сцену"),
                                            path.isEmpty()
                                                ? QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(
                                                    QStringLiteral("Scenes/New.scene.json"))
                                                : path,
                                            QStringLiteral("Scene (*.scene.json)"));
        if (path.isEmpty()) return;
        if (!path.endsWith(QStringLiteral(".scene.json"), Qt::CaseInsensitive))
        {
            path += QStringLiteral(".scene.json");
            if (QFileInfo::exists(path) && QMessageBox::question(this, QStringLiteral("Заменить файл?"), path,
                                                                 QMessageBox::Yes | QMessageBox::No,
                                                                 QMessageBox::No) != QMessageBox::Yes)
                return;
        }
    }
    sceneBusy_ = true;
    afterSave_ = std::move(after);
    EditorCommand c;
    c.type = EditorCommand::SaveScene;
    c.path = QFileInfo(path).absoluteFilePath();
    host_->PostCommand(c);
}

void EditorMainWindow::OpenScene(const QString& supplied)
{
    if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    MaybeSave([this, supplied]
    {
        const auto path = supplied.isEmpty()
                              ? QFileDialog::getOpenFileName(this, QStringLiteral("Открыть сцену"),
                                                             QString::fromUtf8(host_->host().contentRoot),
                                                             QStringLiteral("Scene (*.scene.json)"))
                              : supplied;
        if (path.isEmpty()) return;
        sceneBusy_ = true;
        EditorCommand c;
        c.type = EditorCommand::LoadScene;
        c.path = path;
        host_->PostCommand(c);
    });
}

void EditorMainWindow::CommandFinished(const EditorCommand& command, uint32_t status, const QString& error)
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
            hierarchyKey_.clear();
            const auto category = typeCategory_->currentIndex();
            typeCategory_->setCurrentIndex(-1); typeCategory_->setCurrentIndex(category);
            AppendLine(QStringLiteral("Hot reload: DLL заменена. Сцена, выбор и камера сохранены; история отмены очищена."));
        }
    }
    if (command.type == EditorCommand::RefreshScene && command.name == QStringLiteral("confirm"))
    {
        auto continuation = std::move(afterCheck_);
        afterCheck_ = {};
        checkingChanges_ = false;
        if (!continuation) return;
        if (!host_->ReadSnapshot().dirty)
        {
            continuation();
            return;
        }
        const auto choice = QMessageBox::question(this, QStringLiteral("Несохранённая сцена"),
                                                  QStringLiteral("Сохранить изменения сцены?"),
                                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                                  QMessageBox::Save);
        if (choice == QMessageBox::Save) SaveScene(false, std::move(continuation));
        else if (choice == QMessageBox::Discard) continuation();
        return;
    }
    if (command.type == EditorCommand::RefreshScene && command.name == QStringLiteral("close"))
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
    if (command.type == EditorCommand::SaveScene || command.type == EditorCommand::LoadScene || command.type ==
        EditorCommand::NewScene) sceneBusy_ = false;
    if (status != EngineSceneOk)
    {
        afterSave_ = {};
        closing_ = false;
        const QString message = error.isEmpty()
                                    ? QStringLiteral(
                                        "Операция не выполнена (код %1). Проверьте имя объекта, остановите Play и повторите.")
                                    .arg(status)
                                    : error;
        AppendLine(message);
        QMessageBox::warning(this, QStringLiteral("Ошибка сцены"), message);
        return;
    }
    if (command.type == EditorCommand::ImportModel || command.type == EditorCommand::ImportTexture)
    {
        AppendLine(QStringLiteral("Импортировано: %1. Двойной щелчок в «Ресурсы и код»: добавить модель или назначить текстуру выбранному объекту.").arg(command.path));
        statusBar()->showMessage(QStringLiteral("Импортировано: %1").arg(command.path), 8000);
    }
    else if (command.type == EditorCommand::SaveScene)
    {
        scenePath_ = command.path;
        // Commit startupScene only after the scene itself was successfully written.
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
                // The project format requires a relative path (cross-drive paths cannot be relative).
                if (QDir::isRelativePath(relative))
                {
                    json.insert(QStringLiteral("startupScene"), relative);
                    QSaveFile output(file.fileName());
                    const auto bytes = QJsonDocument(json).toJson();
                    committed = output.open(QIODevice::WriteOnly) && output.write(bytes) == bytes.size() && output.
                        commit();
                }
            }
        }
        if (!committed)
        {
            afterSave_ = {};
            closing_ = false;
            QMessageBox::warning(this, QStringLiteral("Стартовая сцена не изменена"),
                                 QStringLiteral(
                                     "Сцена сохранена, но Project.json не обновлён. Проверьте права записи и сохраните сцену на том же диске, что и проект. Запуск отменён."));
            return;
        }
        statusBar()->showMessage(QStringLiteral("Сохранено: %1").arg(scenePath_), 5000);
        auto continuation = std::move(afterSave_);
        afterSave_ = {};
        if (continuation) continuation();
    }
    else if (command.type == EditorCommand::LoadScene) scenePath_ = command.path;
    else if (command.type == EditorCommand::NewScene) scenePath_.clear();
    RefreshSceneTools();
}
