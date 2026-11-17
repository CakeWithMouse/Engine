#include "MainWindow.h"
#include "CommandRegistry.h"

#include <QVariantMap>

void EditorMainWindow::RegisterAssetCreateCommands()
{
    auto& registry = CommandRegistry::Instance();

    registry.RegisterCommand({
        QStringLiteral("Assets.CreateMaterial"),
        QStringLiteral("Create Material"),
        QStringLiteral("Create a new material in a project folder"),
        QString(),
        [this](const CommandContext& ctx) {
            const QString directory = ctx.parameters.value(QStringLiteral("directory")).toString();
            return !CreateMaterial(directory).isEmpty();
        },
        [this]() { return host_ && host_->HasMaterials(); }
    });

    registry.RegisterCommand({
        QStringLiteral("Assets.CreateScript"),
        QStringLiteral("Create Script"),
        QStringLiteral("Create a new visual script in a project folder"),
        QString(),
        [this](const CommandContext& ctx) {
            const QString directory = ctx.parameters.value(QStringLiteral("directory")).toString();
            return !CreateScript(directory).isEmpty();
        },
        [this]() { return host_ && host_->ReadSnapshot().scripts; }
    });

    registry.RegisterCommand({
        QStringLiteral("Assets.CreateAiMachine"),
        QStringLiteral("Create AI State Machine"),
        QStringLiteral("Create a new AI state machine in a project folder"),
        QString(),
        [this](const CommandContext& ctx) {
            const QString directory = ctx.parameters.value(QStringLiteral("directory")).toString();
            return !CreateAiMachine(directory).isEmpty();
        },
        nullptr
    });
}

void EditorMainWindow::UnregisterAssetCreateCommands()
{
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.CreateMaterial"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.CreateScript"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.CreateAiMachine"));
}