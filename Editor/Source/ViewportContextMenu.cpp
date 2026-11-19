#include "MainWindow.h"
#include "QtHost.h"
#include "SceneObjectsPanel.h"
#include "CommandRegistry.h"
#include "ScenePanelHelpers.h"
#include "EditorLocale.h"
#include "IconProvider.h"

#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QLineEdit>
#include <QVariant>
#include <array>
#include <functional>

void EditorMainWindow::RegisterViewportMenuCommands()
{
    auto& reg = CommandRegistry::Instance();
    std::function<bool()> editing = [this] {
        return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::Edit;
    };
    std::function<bool()> selected = [this, editing] { return editing() && selectedId_ != 0; };

    CommandDef create;
    create.id = QStringLiteral("Scene.Create");
    create.title = Tr("Создать объект", "Create Object");
    create.description = Tr("Создать объект сцены заданного типа", "Create a scene object of the given type");
    create.handler = [this, editing](const CommandContext& ctx) {
        if (!editing()) return false;
        const QString type = ctx.parameters.value(QStringLiteral("type")).toString();
        if (type.isEmpty()) return false;
        EditorCommand c;
        c.type = EditorCommand::CreateObject;
        c.path = type;
        c.name = ctx.parameters.value(QStringLiteral("name")).toString();
        if (c.name.isEmpty()) c.name = EditorUI::LocalizedTypeLabel(type, type);
        c.parentId = ctx.parameters.value(QStringLiteral("parentId"), 0).toULongLong();
        c.transform.position[0] = ctx.parameters.value(QStringLiteral("x"), 0.0).toFloat();
        c.transform.position[1] = ctx.parameters.value(QStringLiteral("y"), 0.0).toFloat();
        c.transform.position[2] = ctx.parameters.value(QStringLiteral("z"), 0.0).toFloat();
        if (type == QStringLiteral("Light.Point")) c.castShadows = true;
        host_->PostCommand(c);
        return true;
    };
    create.canExecute = editing;
    reg.RegisterCommand(create);

    CommandDef rename;
    rename.id = QStringLiteral("Scene.Rename");
    rename.title = Tr("Переименовать объект", "Rename Object");
    rename.description = Tr("Переименовать выделенный объект сцены", "Rename the selected scene object");
    rename.handler = [this, selected](const CommandContext& ctx) {
        if (!selected()) return false;
        const uint64_t id = ctx.parameters.value(QStringLiteral("objectId"), selectedId_).toULongLong();
        if (!id) return false;
        QString name = ctx.parameters.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
        {
            QString current;
            for (const auto& o : host_->ReadSnapshot().objects)
                if (o.id == id) { current = QString::fromStdString(o.name); break; }
            bool ok = false;
            name = QInputDialog::getText(this, Tr("Переименовать объект", "Rename Object"),
                                         Tr("Имя:", "Name:"), QLineEdit::Normal, current, &ok);
            if (!ok) return false;
        }
        name = name.trimmed();
        if (name.isEmpty()) return false;
        EditorCommand c;
        c.type = EditorCommand::RenameObject;
        c.objectId = id;
        c.name = name;
        host_->PostCommand(c);
        return true;
    };
    rename.canExecute = selected;
    reg.RegisterCommand(rename);

    CommandDef reset;
    reset.id = QStringLiteral("Scene.ResetTransform");
    reset.title = Tr("Сбросить трансформацию", "Reset Transform");
    reset.description = Tr("Сбросить локальную трансформацию объекта к единичной", "Reset the object's local transform to identity");
    reset.handler = [this, selected](const CommandContext& ctx) {
        if (!selected()) return false;
        const uint64_t id = ctx.parameters.value(QStringLiteral("objectId"), selectedId_).toULongLong();
        if (!id) return false;
        EditorCommand c;
        c.type = EditorCommand::UpdateObjectTransform;
        c.objectId = id;
        c.transform = EngineSceneTransform{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
        host_->PostCommand(c);
        return true;
    };
    reset.canExecute = selected;
    reg.RegisterCommand(reset);

    CommandDef play;
    play.id = QStringLiteral("Editor.TogglePlay");
    play.title = Tr("Play / Stop", "Play / Stop");
    play.description = Tr("Запустить или остановить Play", "Start or stop Play");
    play.handler = [this](const CommandContext&) {
        if (!host_ || !host_->HasScene()) return false;
        EditorCommand c;
        c.type = EditorCommand::TogglePlay;
        host_->PostCommand(c);
        return true;
    };
    play.canExecute = [this] { return host_ && host_->HasScene(); };
    reg.RegisterCommand(play);
}

void EditorMainWindow::UnregisterViewportMenuCommands()
{
    auto& reg = CommandRegistry::Instance();
    reg.UnregisterCommand(QStringLiteral("Scene.Create"));
    reg.UnregisterCommand(QStringLiteral("Scene.Rename"));
    reg.UnregisterCommand(QStringLiteral("Scene.ResetTransform"));
    reg.UnregisterCommand(QStringLiteral("Editor.TogglePlay"));
}

void EditorMainWindow::BuildViewportContextMenu(const QPoint& globalPos)
{
    UpdatePlayButtons();
    const auto view = host_->ReadSnapshot();
    const bool edit = host_->HasScene() && host_->PlayState() == EditorQtHost::Edit && !sceneBusy_;
    const uint64_t sel = view.selected;

    QMenu menu(this);
    auto addCmd = [&menu, this](const QString& id, const QString& text) {
        auto* a = menu.addAction(text, this, [id] { CommandRegistry::Instance().Execute(id); });
        a->setEnabled(CommandRegistry::Instance().CanExecute(id));
        return a;
    };
    auto buildCreateMenu = [&](QMenu* m, uint64_t parentId) {
        const auto spawn = sceneObjectsPanel_ ? sceneObjectsPanel_->SpawnPosition() : std::array<double, 3>{0, 0, 0};
        QVector<std::pair<QString, QString>> typePairs;
        for (const auto& t : view.types)
            typePairs.append({QString::fromStdString(t.label), QString::fromStdString(t.name)});
        EditorUI::PopulateCategorizedCreateMenu(m, typePairs, [parentId, spawn](const QString& type, const QString& label) {
            QVariantMap p;
            p[QStringLiteral("parentId")] = QVariant::fromValue(parentId);
            p[QStringLiteral("x")] = spawn[0];
            p[QStringLiteral("y")] = spawn[1];
            p[QStringLiteral("z")] = spawn[2];
            p[QStringLiteral("type")] = type;
            p[QStringLiteral("name")] = label;
            if (type == QStringLiteral("Scene.Folder"))
                CommandRegistry::Instance().Execute(QStringLiteral("Scene.CreateFolder"), p);
            else if (type == QStringLiteral("Scene.Empty"))
                CommandRegistry::Instance().Execute(QStringLiteral("Scene.CreateEmpty"), p);
            else
                CommandRegistry::Instance().Execute(QStringLiteral("Scene.Create"), p);
        }, parentId ? QStringLiteral("CreateChildObject.") : QStringLiteral("CreateObject."));
    };

    if (edit && sel)
    {
        addCmd(QStringLiteral("Scene.Copy"), Tr("Копировать", "Copy"));
        if (host_->HasSceneClipboard())
            addCmd(QStringLiteral("Scene.Paste"), Tr("Вставить", "Paste"));
        addCmd(QStringLiteral("Scene.Duplicate"), Tr("Дублировать", "Duplicate"));
        addCmd(QStringLiteral("Scene.Rename"), Tr("Переименовать", "Rename"));
        addCmd(QStringLiteral("Scene.Delete"), Tr("Удалить", "Delete"));
        menu.addSeparator();
        auto* childMenu = menu.addMenu(Tr("Создать дочерний", "Create Child"));
        childMenu->setIcon(IconProvider::PlusIcon());
        buildCreateMenu(childMenu, sel);
        addCmd(QStringLiteral("Gizmo.FocusSelected"), Tr("Сфокусироваться (F)", "Focus (F)"));
        addCmd(QStringLiteral("Scene.ResetTransform"), Tr("Сбросить трансформацию", "Reset Transform"));
        const IEditorHost::SceneObject* obj = nullptr;
        for (const auto& o : view.objects)
            if (o.id == sel) { obj = &o; break; }
        const bool mesh = obj && (!obj->modelAsset.empty() || QString::fromStdString(obj->type).startsWith(QLatin1String("Primitive.")));
        if (mesh)
        {
            menu.addSeparator();
            auto* a = menu.addAction(Tr("Заменить материал…", "Replace Material…"), this, [sel] {
                CommandRegistry::Instance().Execute(QStringLiteral("Scene.AssignMaterial"),
                                                    {{QStringLiteral("objectId"), QVariant::fromValue(sel)}});
            });
            a->setEnabled(CommandRegistry::Instance().CanExecute(QStringLiteral("Scene.AssignMaterial")));
        }
    }
    else
    {
        if (edit)
        {
            auto* createMenu = menu.addMenu(Tr("Создать", "Create"));
            createMenu->setIcon(IconProvider::PlusIcon());
            buildCreateMenu(createMenu, 0);
            if (host_->HasSceneClipboard())
                addCmd(QStringLiteral("Scene.Paste"), Tr("Вставить", "Paste"));
            menu.addSeparator();
        }
        if (host_->HasScene())
        {
            const bool playing = host_->PlayState() != EditorQtHost::Edit;
            addCmd(QStringLiteral("Editor.TogglePlay"), playing ? Tr("Стоп", "Stop") : Tr("Play", "Play"));
            addCmd(QStringLiteral("Scene.Save"), Tr("Сохранить сцену", "Save Scene"));
        }
    }

    if (!menu.isEmpty())
        menu.exec(globalPos);
}