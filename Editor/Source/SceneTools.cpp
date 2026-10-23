#include "MainWindow.h"
#include "QtHost.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
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
#include <QMessageBox>
#include <QPushButton>
#include <QQuaternion>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <map>

void EditorMainWindow::BuildSceneTools()
{
    auto* menu = menuBar()->addMenu(QStringLiteral("Сцена"));
    const bool available = host_->HasScene();
    menu->setEnabled(available);
    menu->addAction(QStringLiteral("Новая сцена"), this, [this] {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        MaybeSave([this] { sceneBusy_ = true; EditorCommand c; c.type = EditorCommand::NewScene; host_->PostCommand(c); });
    }, QKeySequence::New);
    menu->addAction(QStringLiteral("Открыть сцену…"), this, [this] { OpenScene(); }, QKeySequence::Open);
    saveAction_ = menu->addAction(QStringLiteral("Сохранить и сделать стартовой"), this, [this] { SaveScene(); }, QKeySequence::Save);
    menu->addAction(QStringLiteral("Сохранить как…"), this, [this] { SaveScene(true); }, QKeySequence::SaveAs);
    menu->addSeparator();
    undoAction_ = menu->addAction(QStringLiteral("Отменить"), this, [this] { SubmitObjectCommand(EditorCommand::Undo); }, QKeySequence::Undo);
    redoAction_ = menu->addAction(QStringLiteral("Повторить"), this, [this] { SubmitObjectCommand(EditorCommand::Redo); }, QKeySequence::Redo);
    duplicateAction_ = menu->addAction(QStringLiteral("Дублировать объект"), this, [this] { SubmitObjectCommand(EditorCommand::DuplicateObject); }, QKeySequence(QStringLiteral("Ctrl+D")));
    deleteAction_ = menu->addAction(QStringLiteral("Удалить объект"), this, [this] { SubmitObjectCommand(EditorCommand::RemoveObject); });
    // Delete is local to the hierarchy; typing in name/number fields must not delete objects.
    auto* removeKey = new QAction(this); removeKey->setShortcut(QKeySequence::Delete); removeKey->setShortcutContext(Qt::WidgetShortcut);
    connect(removeKey, &QAction::triggered, this, [this] { SubmitObjectCommand(EditorCommand::RemoveObject); });

    if (!available)
    {
        AppendLine(QStringLiteral("Preview проекта. Создайте свой проект в лаунчере для редактирования сцены."));
        return;
    }
    QFile manifest(QString::fromUtf8(host_->host().projectFile));
    if (manifest.open(QIODevice::ReadOnly))
    {
        const auto json = QJsonDocument::fromJson(manifest.readAll()).object();
        const auto relative = json.value(QStringLiteral("startupScene")).toString();
        if (!relative.isEmpty()) scenePath_ = QFileInfo(manifest).dir().absoluteFilePath(relative);
    }

    auto* dock = new QDockWidget(QStringLiteral("Сцена / примитивы"), this);
    scenePanel_ = new QWidget(dock); auto* layout = new QVBoxLayout(scenePanel_);
    primitive_ = new QComboBox(scenePanel_); primitive_->setObjectName(QStringLiteral("PrimitiveType"));
    for (const auto& type : host_->ReadSnapshot().types)
        primitive_->addItem(QString::fromStdString(type.label), QString::fromStdString(type.name));
    layout->addWidget(primitive_);
    layout->addWidget(new QLabel(QStringLiteral("Точка добавления — X / Y / Z"), scenePanel_));
    auto* spawnRow = new QHBoxLayout;
    for (int i = 0; i < 3; ++i)
    {
        spawn_[i] = new QDoubleSpinBox(scenePanel_); spawn_[i]->setRange(-10000, 10000); spawn_[i]->setDecimals(2);
        spawn_[i]->setSingleStep(0.5); spawnRow->addWidget(spawn_[i]);
    }
    spawn_[0]->setValue(2);
    layout->addLayout(spawnRow);
    auto* add = new QPushButton(QStringLiteral("+ Добавить примитив"), scenePanel_); layout->addWidget(add); add->setObjectName(QStringLiteral("AddPrimitive"));
    connect(add, &QPushButton::clicked, this, [this] {
        if (sceneBusy_ || host_->PlayState() != EditorQtHost::Edit || primitive_->currentIndex() < 0) return;
        EditorCommand c; c.type = EditorCommand::CreateObject;
        c.path = primitive_->currentData().toString(); c.name = primitive_->currentText();
        for (int i = 0; i < 3; ++i) c.transform.position[i] = static_cast<float>(spawn_[i]->value());
        host_->PostCommand(c);
    });
    hierarchy_ = new QTreeWidget(scenePanel_); hierarchy_->setHeaderLabels({QStringLiteral("Объект"), QStringLiteral("Тип")});
    hierarchy_->setSelectionMode(QAbstractItemView::SingleSelection); hierarchy_->addAction(removeKey); layout->addWidget(hierarchy_, 1);
    connect(hierarchy_, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (refreshing_) return;
        const auto items = hierarchy_->selectedItems();
        EditorCommand c; c.type = EditorCommand::SelectObject;
        c.objectId = items.isEmpty() ? 0 : items.front()->data(0, Qt::UserRole).toULongLong(); host_->PostCommand(c);
    });
    connect(hierarchy_, &QTreeWidget::itemDoubleClicked, this, [this] {
        EditorCommand c; c.type = EditorCommand::FocusObject; host_->PostCommand(c);
    });
    auto* buttons = new QHBoxLayout;
    auto* duplicate = new QPushButton(QStringLiteral("Дубликат"), scenePanel_);
    auto* remove = new QPushButton(QStringLiteral("Удалить"), scenePanel_);
    buttons->addWidget(duplicate); buttons->addWidget(remove); layout->addLayout(buttons);
    connect(duplicate, &QPushButton::clicked, this, [this] { SubmitObjectCommand(EditorCommand::DuplicateObject); });
    connect(remove, &QPushButton::clicked, this, [this] { SubmitObjectCommand(EditorCommand::RemoveObject); });
    auto* help = new QLabel(QStringLiteral("ПКМ + мышь: обзор; WASD, Q/E: камера.\nЛКМ/Tab: выбор; F: фокус камеры.\nСтрелки, PgUp/PgDn: объект.\nCtrl+S: сохранить; Ctrl+Z: отменить.\nPlay проверяет копию сцены; Stop возвращает правки."), scenePanel_);
    help->setWordWrap(true); layout->addWidget(help);
    dock->setWidget(scenePanel_); addDockWidget(Qt::LeftDockWidgetArea, dock);

    auto* properties = new QDockWidget(QStringLiteral("Свойства объекта"), this);
    inspector_ = new QWidget(properties); auto* form = new QFormLayout(inspector_);
    objectName_ = new QLineEdit(inspector_); objectName_->setMaxLength(120); form->addRow(QStringLiteral("Имя"), objectName_);
    connect(objectName_, &QLineEdit::editingFinished, this, [this] {
        if (refreshing_ || !selectedId_ || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        const auto name = objectName_->text().trimmed();
        for (const auto& o : host_->ReadSnapshot().objects) if (o.id == selectedId_ && name == QString::fromStdString(o.name)) return;
        EditorCommand c; c.type = EditorCommand::RenameObject; c.objectId = selectedId_; c.name = name; host_->PostCommand(c);
    });
    const QString labels[] = {QStringLiteral("Позиция X"),QStringLiteral("Позиция Y"),QStringLiteral("Позиция Z"),
        QStringLiteral("Поворот X, °"),QStringLiteral("Поворот Y, °"),QStringLiteral("Поворот Z, °"),
        QStringLiteral("Масштаб X"),QStringLiteral("Масштаб Y"),QStringLiteral("Масштаб Z")};
    for (int i = 0; i < 9; ++i)
    {
        auto* spin = transform_[i] = new QDoubleSpinBox(inspector_);
        spin->setObjectName(QStringLiteral("Transform%1").arg(i));
        spin->setRange(i >= 6 ? 0.001 : -10000, 10000); spin->setDecimals(3); spin->setSingleStep(i >= 3 && i < 6 ? 15 : 0.25);
        spin->setKeyboardTracking(false); form->addRow(labels[i], spin);
        connect(spin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::ApplyTransform);
    }
    properties->setWidget(inspector_); addDockWidget(Qt::RightDockWidgetArea, properties);
    auto* view = menuBar()->addMenu(QStringLiteral("Панели сцены")); view->addAction(dock->toggleViewAction()); view->addAction(properties->toggleViewAction());
    resizeDocks({dock, properties}, {270, 250}, Qt::Horizontal);
    RefreshSceneTools();
}

void EditorMainWindow::SubmitObjectCommand(int type)
{
    if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    EditorCommand c; c.type = static_cast<EditorCommand::Type>(type); c.objectId = selectedId_;
    if (!c.objectId && (c.type == EditorCommand::DuplicateObject || c.type == EditorCommand::RemoveObject)) return;
    host_->PostCommand(c);
}

void EditorMainWindow::ApplyTransform()
{
    if (refreshing_ || !selectedId_ || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    EditorCommand c; c.type = EditorCommand::UpdateObjectTransform; c.objectId = selectedId_;
    for (int i = 0; i < 3; ++i) { c.transform.position[i] = static_cast<float>(transform_[i]->value()); c.transform.scale[i] = static_cast<float>(transform_[i + 6]->value()); }
    const auto q = QQuaternion::fromEulerAngles(static_cast<float>(transform_[3]->value()), static_cast<float>(transform_[4]->value()), static_cast<float>(transform_[5]->value()));
    c.transform.rotation[0] = q.x(); c.transform.rotation[1] = q.y(); c.transform.rotation[2] = q.z(); c.transform.rotation[3] = q.scalar();
    // Focus changes alone must not add an undo step or mark the document dirty.
    for (const auto& o : host_->ReadSnapshot().objects) if (o.id == selectedId_)
    {
        bool changed = false;
        for (int i = 0; i < 3; ++i) changed |= std::abs(c.transform.position[i] - o.transform.position[i]) > 0.0006f || std::abs(c.transform.scale[i] - o.transform.scale[i]) > 0.0006f;
        const auto& r = o.transform.rotation;
        changed |= std::abs(QQuaternion::dotProduct(q, QQuaternion(r[3], r[0], r[1], r[2]).normalized())) < 0.999999f;
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
    undoAction_->setEnabled(edit && view.canUndo); redoAction_->setEnabled(edit && view.canRedo);
    duplicateAction_->setEnabled(edit && view.selected); deleteAction_->setEnabled(edit && view.selected); saveAction_->setEnabled(!sceneBusy_);
    setWindowTitle(QStringLiteral("%1%2 — %3 — Engine Editor").arg(view.dirty ? QStringLiteral("* ") : QString(),
        scenePath_.isEmpty() ? QStringLiteral("Новая сцена") : QFileInfo(scenePath_).fileName(), QString::fromUtf8(host_->host().projectName)));
    QString key;
    for (const auto& o : view.objects) key += QStringLiteral("%1/%2/%3:%4;").arg(o.id).arg(o.parent).arg(o.name.size()).arg(QString::fromStdString(o.name));
    if (key != hierarchyKey_)
    {
        hierarchy_->clear(); std::map<uint64_t, QTreeWidgetItem*> items;
        for (const auto& o : view.objects)
        {
            auto* item = new QTreeWidgetItem({QString::fromStdString(o.name), QString::fromStdString(o.type)});
            item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(o.id)); items[o.id] = item;
        }
        for (const auto& o : view.objects)
        { if (o.parent && items.count(o.parent)) items[o.parent]->addChild(items[o.id]); else hierarchy_->addTopLevelItem(items[o.id]); }
        hierarchy_->expandAll(); hierarchyKey_ = key;
    }
    for (QTreeWidgetItemIterator it(hierarchy_); *it; ++it) (*it)->setSelected((*it)->data(0, Qt::UserRole).toULongLong() == view.selected);
    const bool selectionChanged = selectedId_ != view.selected; selectedId_ = view.selected;
    inspector_->setEnabled(edit && selectedId_);
    const auto* focus = QApplication::focusWidget();
    if (selectionChanged || !focus || !inspector_->isAncestorOf(focus))
    {
        for (const auto& o : view.objects) if (o.id == selectedId_)
        {
            objectName_->setText(QString::fromStdString(o.name));
            const auto& t = o.transform; const auto euler = QQuaternion(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]).toEulerAngles();
            for (int i = 0; i < 3; ++i) { transform_[i]->setValue(t.position[i]); transform_[i+3]->setValue(euler[i]); transform_[i+6]->setValue(t.scale[i]); }
        }
    }
    refreshing_ = false;
    EditorCommand c; c.type = EditorCommand::RefreshScene; host_->PostCommand(c);
}

void EditorMainWindow::MaybeSave(std::function<void()> after)
{
    if (checkingChanges_) return;
    // Commit the active text/number field, then wait behind every queued mutation.
    if (auto* focus = QApplication::focusWidget(); focus && inspector_ && inspector_->isAncestorOf(focus)) focus->clearFocus();
    checkingChanges_ = true; afterCheck_ = std::move(after);
    EditorCommand c; c.type = EditorCommand::RefreshScene; c.name = QStringLiteral("confirm"); host_->PostCommand(c);
}
void EditorMainWindow::SaveScene(bool saveAs, std::function<void()> after)
{
    if (!host_->HasScene() || sceneBusy_) return;
    if (auto* focus = QApplication::focusWidget(); focus && inspector_ && inspector_->isAncestorOf(focus)) focus->clearFocus();
    QString path = scenePath_;
    if (saveAs || path.isEmpty())
    {
        path = QFileDialog::getSaveFileName(this, QStringLiteral("Сохранить стартовую сцену"),
            path.isEmpty() ? QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(QStringLiteral("Scenes/New.scene.json")) : path,
            QStringLiteral("Scene (*.scene.json)"));
        if (path.isEmpty()) return;
        if (!path.endsWith(QStringLiteral(".scene.json"), Qt::CaseInsensitive))
        {
            path += QStringLiteral(".scene.json");
            if (QFileInfo::exists(path) && QMessageBox::question(this, QStringLiteral("Заменить файл?"), path,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        }
    }
    sceneBusy_ = true; afterSave_ = std::move(after);
    EditorCommand c; c.type = EditorCommand::SaveScene; c.path = QFileInfo(path).absoluteFilePath(); host_->PostCommand(c);
}
void EditorMainWindow::OpenScene(const QString& supplied)
{
    if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    MaybeSave([this, supplied] {
        const auto path = supplied.isEmpty() ? QFileDialog::getOpenFileName(this, QStringLiteral("Открыть сцену"), QString::fromUtf8(host_->host().contentRoot), QStringLiteral("Scene (*.scene.json)")) : supplied;
        if (path.isEmpty()) return;
        sceneBusy_ = true; EditorCommand c; c.type = EditorCommand::LoadScene; c.path = path; host_->PostCommand(c);
    });
}

void EditorMainWindow::CommandFinished(const EditorCommand& command, uint32_t status, const QString& error)
{
    if (command.type == EditorCommand::RefreshScene && command.name == QStringLiteral("confirm"))
    {
        auto continuation = std::move(afterCheck_); afterCheck_ = {}; checkingChanges_ = false;
        if (!continuation) return;
        if (!host_->ReadSnapshot().dirty) { continuation(); return; }
        const auto choice = QMessageBox::question(this, QStringLiteral("Несохранённая сцена"), QStringLiteral("Сохранить изменения сцены?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (choice == QMessageBox::Save) SaveScene(false, std::move(continuation));
        else if (choice == QMessageBox::Discard) continuation();
        return;
    }
    if (command.type == EditorCommand::RefreshScene && command.name == QStringLiteral("close"))
    {
        closing_ = false;
        MaybeSave([this] { closing_ = true; EditorCommand c; c.type = EditorCommand::RequestExit; host_->PostCommand(c); });
        return;
    }
    if (command.type == EditorCommand::SaveScene || command.type == EditorCommand::LoadScene || command.type == EditorCommand::NewScene) sceneBusy_ = false;
    if (status != EngineSceneOk)
    {
        afterSave_ = {}; closing_ = false;
        const QString message = error.isEmpty() ? QStringLiteral("Операция не выполнена (код %1). Проверьте имя объекта, остановите Play и повторите.").arg(status) : error;
        AppendLine(message); QMessageBox::warning(this, QStringLiteral("Ошибка сцены"), message); return;
    }
    if (command.type == EditorCommand::SaveScene)
    {
        scenePath_ = command.path;
        // Commit startupScene only after the scene itself was successfully written.
        QFile file(QString::fromUtf8(host_->host().projectFile));
        bool committed = false;
        if (file.open(QIODevice::ReadOnly))
        {
            QJsonParseError parse{}; auto document = QJsonDocument::fromJson(file.readAll(), &parse); file.close();
            if (parse.error == QJsonParseError::NoError && document.isObject())
            {
                auto json = document.object();
                const auto relative = QFileInfo(file).dir().relativeFilePath(scenePath_);
                // The project format requires a relative path (cross-drive paths cannot be relative).
                if (QDir::isRelativePath(relative))
                {
                    json.insert(QStringLiteral("startupScene"), relative);
                    QSaveFile output(file.fileName()); const auto bytes = QJsonDocument(json).toJson();
                    committed = output.open(QIODevice::WriteOnly) && output.write(bytes) == bytes.size() && output.commit();
                }
            }
        }
        if (!committed)
        {
            afterSave_ = {}; closing_ = false;
            QMessageBox::warning(this, QStringLiteral("Стартовая сцена не изменена"), QStringLiteral("Сцена сохранена, но Project.json не обновлён. Проверьте права записи и сохраните сцену на том же диске, что и проект. Запуск отменён.")); return;
        }
        statusBar()->showMessage(QStringLiteral("Сохранено: %1").arg(scenePath_), 5000);
        auto continuation = std::move(afterSave_); afterSave_ = {}; if (continuation) continuation();
    }
    else if (command.type == EditorCommand::LoadScene) scenePath_ = command.path;
    else if (command.type == EditorCommand::NewScene) scenePath_.clear();
    RefreshSceneTools();
}
