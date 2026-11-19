#pragma once

#include "CommandRegistry.h"
#include "EditorLocale.h"
#include "EditorSettings.h"
#include "QtHost.h"

#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QObject>
#include <functional>

// Хоткеи сцены: одно действие пользователя = одна команда CommandRegistry.
// Шорткаты живут на QAction уровня окна, поэтому срабатывают и при фокусе
// во вьюпорте, и при фокусе в иерархии; сами обработчики уходят в host-команды.
class SceneEditCommands : public QObject
{
public:
    SceneEditCommands(EditorQtHost* host, QMenu* editMenu, QObject* parent,
                      std::function<void(int)> submit, std::function<void()> save)
        : QObject(parent), host_(host), submit_(std::move(submit)), save_(std::move(save))
    {
        auto& registry = CommandRegistry::Instance();
        auto command = [&registry, this](const QString& id, const QString& title, int type, std::function<bool()> can) {
            CommandDef def;
            def.id = id;
            def.title = title;
            def.description = title;
            def.handler = [this, type](const CommandContext&) { submit_(type); return true; };
            def.canExecute = std::move(can);
            registry.RegisterCommand(def);
        };
        auto editing = [this] { return host_->HasScene() && host_->PlayState() == IEditorHost::Edit; };
        auto selected = [this, editing] { return editing() && host_->HasSelection(); };
        command(QStringLiteral("Scene.Undo"), QStringLiteral("Undo Scene Edit"), EditorCommand::Undo, nullptr);
        command(QStringLiteral("Scene.Redo"), QStringLiteral("Redo Scene Edit"), EditorCommand::Redo, nullptr);
        command(QStringLiteral("Scene.Duplicate"), QStringLiteral("Duplicate Object"), EditorCommand::DuplicateObject, selected);
        command(QStringLiteral("Scene.Delete"), QStringLiteral("Remove Object"), EditorCommand::RemoveObject, selected);
        command(QStringLiteral("Scene.Copy"), QStringLiteral("Copy Object"), EditorCommand::CopyObject, selected);
        command(QStringLiteral("Scene.Cut"), QStringLiteral("Cut Object"), EditorCommand::CutObject, selected);
        command(QStringLiteral("Scene.Paste"), QStringLiteral("Paste Object"), EditorCommand::PasteObject,
                [this, editing] { return editing() && host_->HasSceneClipboard(); });
        CommandDef saveDef;
        saveDef.id = QStringLiteral("Scene.Save");
        saveDef.title = QStringLiteral("Save Scene");
        saveDef.description = saveDef.title;
        saveDef.handler = [this](const CommandContext&) { save_(); return true; };
        saveDef.canExecute = [this] { return host_->HasScene(); };
        registry.RegisterCommand(saveDef);

        auto menuAction = [this, editMenu](const QString& id, const QString& text, const QKeySequence& shortcut) {
            auto* action = editMenu->addAction(text, this, [id] { CommandRegistry::Instance().Execute(id); });
            action->setShortcut(shortcut);
            return action;
        };
        copyAction_ = menuAction(QStringLiteral("Scene.Copy"), Tr("Копировать объект", "Copy Object"), QKeySequence::Copy);
        cutAction_ = menuAction(QStringLiteral("Scene.Cut"), Tr("Вырезать объект", "Cut Object"), QKeySequence::Cut);
        pasteAction_ = menuAction(QStringLiteral("Scene.Paste"), Tr("Вставить объект", "Paste Object"), QKeySequence::Paste);

        auto gizmoAction = [this, parent](const QString& id) {
            auto* action = new QAction(parent);
            connect(action, &QAction::triggered, this, [id] { CommandRegistry::Instance().Execute(id); });
            return action;
        };
        gizmoTranslate_ = gizmoAction(QStringLiteral("Gizmo.SetTranslate"));
        gizmoRotate_ = gizmoAction(QStringLiteral("Gizmo.SetRotate"));
        gizmoScale_ = gizmoAction(QStringLiteral("Gizmo.SetScale"));
        gizmoFocus_ = gizmoAction(QStringLiteral("Gizmo.FocusSelected"));
        ApplyScheme();
        schemeSubId_ = EditorSettings::Instance().SubscribeControlSchemeChanged([this](ControlScheme) { ApplyScheme(); });
    }

    ~SceneEditCommands() override
    {
        EditorSettings::Instance().UnsubscribeControlSchemeChanged(schemeSubId_);
        auto& registry = CommandRegistry::Instance();
        for (const char* id : {"Scene.Undo", "Scene.Redo", "Scene.Duplicate", "Scene.Delete", "Scene.Copy", "Scene.Cut", "Scene.Paste", "Scene.Save"})
            registry.UnregisterCommand(QString::fromLatin1(id));
    }

    void UpdateActions(const IEditorHost::Snapshot& view, bool edit)
    {
        copyAction_->setEnabled(edit && view.selected);
        cutAction_->setEnabled(edit && view.selected);
        pasteAction_->setEnabled(edit && host_->HasSceneClipboard());
    }

private:
    void ApplyScheme()
    {
        const bool blender = EditorSettings::Instance().GetControlScheme() == ControlScheme::Blender;
        gizmoTranslate_->setShortcut(QKeySequence(blender ? Qt::Key_G : Qt::Key_W));
        gizmoRotate_->setShortcut(QKeySequence(blender ? Qt::Key_R : Qt::Key_E));
        gizmoScale_->setShortcut(QKeySequence(blender ? Qt::Key_S : Qt::Key_R));
        gizmoFocus_->setShortcut(QKeySequence(Qt::Key_F));
    }

    EditorQtHost* host_;
    std::function<void(int)> submit_;
    std::function<void()> save_;
    QAction* copyAction_ = nullptr;
    QAction* cutAction_ = nullptr;
    QAction* pasteAction_ = nullptr;
    QAction* gizmoTranslate_ = nullptr;
    QAction* gizmoRotate_ = nullptr;
    QAction* gizmoScale_ = nullptr;
    QAction* gizmoFocus_ = nullptr;
    int schemeSubId_ = 0;
};