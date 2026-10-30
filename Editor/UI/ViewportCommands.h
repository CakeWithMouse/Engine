#pragma once

#include "CommandRegistry.h"
#include "ViewportWidget.h"
#include "EditorSettings.h"
#include "QtHost.h"

inline void RegisterViewportCommands(ViewportWidget* viewport, EditorQtHost* host)
{
    auto reg = [](const QString& id, const QString& title, CommandHandler handler) {
        CommandDef def;
        def.id = id;
        def.title = title;
        def.handler = std::move(handler);
        CommandRegistry::Instance().RegisterCommand(def);
    };

    reg(QStringLiteral("Gizmo.SetTranslate"), QStringLiteral("Translate Mode"), [viewport](const CommandContext&) {
        if (viewport) viewport->setGizmoMode(EditorGizmoTranslate);
        return true;
    });

    reg(QStringLiteral("Gizmo.SetRotate"), QStringLiteral("Rotate Mode"), [viewport](const CommandContext&) {
        if (viewport) viewport->setGizmoMode(EditorGizmoRotate);
        return true;
    });

    reg(QStringLiteral("Gizmo.SetScale"), QStringLiteral("Scale Mode"), [viewport](const CommandContext&) {
        if (viewport) viewport->setGizmoMode(EditorGizmoScale);
        return true;
    });

    reg(QStringLiteral("Gizmo.ToggleCoordinateSpace"), QStringLiteral("Toggle World/Local Space"), [viewport](const CommandContext&) {
        if (viewport) viewport->ToggleCoordinateSpace();
        return true;
    });

    reg(QStringLiteral("Gizmo.ToggleSnap"), QStringLiteral("Toggle Snapping"), [viewport](const CommandContext&) {
        if (viewport) viewport->ToggleSnap();
        return true;
    });

    reg(QStringLiteral("Gizmo.FocusSelected"), QStringLiteral("Focus Selected Object"), [host](const CommandContext&) {
        if (host && host->HasScene())
        {
            EditorCommand c;
            c.type = EditorCommand::FocusObject;
            host->PostCommand(c);
        }
        return true;
    });

    reg(QStringLiteral("Editor.ToggleControlScheme"), QStringLiteral("Toggle Control Scheme"), [](const CommandContext&) {
        const auto scheme = EditorSettings::Instance().GetControlScheme();
        EditorSettings::Instance().SetControlScheme(scheme == ControlScheme::Blender ? ControlScheme::UnrealUnity : ControlScheme::Blender);
        return true;
    });
}

inline void UnregisterViewportCommands()
{
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Gizmo.SetTranslate"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Gizmo.SetRotate"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Gizmo.SetScale"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Gizmo.ToggleCoordinateSpace"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Gizmo.ToggleSnap"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Gizmo.FocusSelected"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Editor.ToggleControlScheme"));
}

