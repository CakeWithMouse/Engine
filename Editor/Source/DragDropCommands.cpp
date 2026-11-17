#include "MainWindow.h"
#include "QtHost.h"
#include "CommandRegistry.h"
#include "AssetOps.h"
#include "FileService.h"
#include "Logger.h"

#include <QDir>
#include <QFileInfo>

void EditorMainWindow::RegisterDragDropCommands()
{
    const QString projectRoot = QFileInfo(ProjectPath()).dir().absolutePath();
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);

    const auto canExecute = [this]() {
        return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit;
    };

    const auto moveOrCopy = [projectRoot, contentRoot](const CommandContext& ctx, bool copy) {
        FileService files(&Logger::Instance());
        files.SetProjectRoot(projectRoot);
        files.SetContentRoot(contentRoot);
        EditorCore::AssetOps operations(&files);

        const QStringList paths = ctx.parameters.value(QStringLiteral("paths")).toStringList();
        const QString destination = ctx.parameters.value(QStringLiteral("destination")).toString();
        if (paths.isEmpty() || destination.isEmpty())
            return false;

        const QDir projectDir(projectRoot);
        bool allOk = true;
        for (const QString& rawPath : paths)
        {
            const QString absolute = rawPath.startsWith(QStringLiteral("/Game/"))
                ? QDir::cleanPath(contentRoot + QStringLiteral("/") + rawPath.mid(6))
                : QDir::cleanPath(rawPath);
            const QFileInfo sourceInfo(absolute);
            if (!sourceInfo.exists())
            {
                Logger::Instance().Error(QStringLiteral("AssetMoveCopy: source does not exist: %1").arg(rawPath));
                allOk = false;
                continue;
            }
            const QString sourceAbs = sourceInfo.absoluteFilePath();
            const QString destFolder = QDir::cleanPath(destination);
            if (sourceInfo.isDir() && (destFolder == sourceAbs || destFolder.startsWith(sourceAbs + QStringLiteral("/"))))
            {
                Logger::Instance().Error(QStringLiteral("AssetMoveCopy: cannot move folder into itself: %1").arg(sourceAbs));
                allOk = false;
                continue;
            }
            const QString targetAbs = QDir(destFolder).filePath(sourceInfo.fileName());
            if (!copy && QDir::cleanPath(targetAbs) == sourceAbs)
                continue;

            const QString sourceRel = projectDir.relativeFilePath(sourceAbs);
            const QString baseTargetRel = projectDir.relativeFilePath(QDir::cleanPath(targetAbs));
            QString targetRel = baseTargetRel;
            int counter = 0;
            while (operations.Exists(targetRel))
                targetRel = EditorCore::AssetOps::ConflictName(baseTargetRel, ++counter);

            QString error;
            const bool ok = copy
                ? operations.Duplicate(sourceRel, targetRel, &error, nullptr, true)
                : operations.Rename(sourceRel, targetRel, &error, true);
            if (!ok)
            {
                Logger::Instance().Error(QStringLiteral("AssetMoveCopy: %1").arg(error));
                allOk = false;
            }
            else
            {
                Logger::Instance().Info(QStringLiteral("AssetMoveCopy: %1 '%2' -> '%3'")
                    .arg(copy ? QStringLiteral("copied") : QStringLiteral("moved"), sourceRel, targetRel));
            }
        }
        return allOk;
    };

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Assets.Move"),
        QStringLiteral("Move Assets"),
        QString(),
        QStringLiteral("Move assets and their companion files to a project folder"),
        [moveOrCopy](const CommandContext& ctx) { return moveOrCopy(ctx, false); },
        canExecute
    });

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Assets.Copy"),
        QStringLiteral("Copy Assets"),
        QString(),
        QStringLiteral("Copy assets and their companion files to a project folder"),
        [moveOrCopy](const CommandContext& ctx) { return moveOrCopy(ctx, true); },
        canExecute
    });
}

void EditorMainWindow::UnregisterDragDropCommands()
{
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.Move"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.Copy"));
}