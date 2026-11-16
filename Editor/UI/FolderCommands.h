#pragma once

#include "AssetOps.h"
#include "CommandRegistry.h"
#include "EditorLocale.h"
#include "FileBrowser.h"
#include "FolderPaths.h"
#include "Logger.h"
#include <QFileInfo>

namespace FolderCommands
{
inline void Register(FileBrowserDock* browser, const QString& projectRoot, const QString& contentRoot,
                     const std::function<bool()>& canExecute)
{
    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Assets.CreateFolder"), QStringLiteral("Create Folder"),
        QStringLiteral("Create a project folder and edit its name"), QString(),
        [browser, projectRoot](const CommandContext& context) {
            FileService files(&Logger::Instance());
            files.SetProjectRoot(projectRoot);
            QString error;
            const QString parent = context.parameters.value(QStringLiteral("parent")).toString();
            const QString absoluteParent = EditorCore::ResolveFolderPath(files, parent, &error);
            if (absoluteParent.isEmpty()) return false;
            if (!QFileInfo(absoluteParent).isDir()) {
                Logger::Instance().Error(QStringLiteral("CreateFolder: parent is not a directory: %1").arg(parent));
                return false;
            }
            const QString base = Tr("Новая папка", "New Folder");
            QString name = base;
            for (int suffix = 2; ; ++suffix) {
                const QFileInfo entry(QDir(absoluteParent).filePath(name));
                if (!entry.exists() && !entry.isSymLink()
#ifdef Q_OS_WIN
                    && !entry.isJunction()
#endif
                ) break;
                name = QStringLiteral("%1 (%2)").arg(base).arg(suffix);
            }
            const QString relative = QDir(projectRoot).relativeFilePath(QDir(absoluteParent).filePath(name));
            EditorCore::AssetOps operations(&files);
            if (!operations.CreateFolder(relative, &error, true)) return false;
            browser->BeginInlineRename(QDir(projectRoot).filePath(relative));
            return true;
        }, canExecute
    });
    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Assets.RenameFolder"), QStringLiteral("Rename Folder"),
        QStringLiteral("Rename a project folder without leaving its parent"), QString(),
        [projectRoot, contentRoot](const CommandContext& context) {
            FileService files(&Logger::Instance());
            files.SetProjectRoot(projectRoot);
            files.SetContentRoot(contentRoot);
            QString error;
            const QString relative = context.parameters.value(QStringLiteral("path")).toString();
            const QString name = context.parameters.value(QStringLiteral("name")).toString();
            if (!EditorCore::ValidateFolderName(name, &error)) return false;
            const QString source = EditorCore::ResolveFolderPath(files, relative, &error);
            if (source.isEmpty()) return false;
            const QFileInfo sourceInfo(source);
            bool linkedSource = sourceInfo.isSymLink();
#ifdef Q_OS_WIN
            linkedSource = linkedSource || sourceInfo.isJunction();
#endif
            if (!sourceInfo.isDir() || linkedSource || files.IsProtectedDirectory(source)) {
                Logger::Instance().Error(QStringLiteral("RenameFolder: source is not an editable folder: %1").arg(relative));
                return false;
            }
            if (QFileInfo(source).fileName() == name) return true;
            const QString destination = QDir(projectRoot).relativeFilePath(QFileInfo(source).dir().filePath(name));
            if (EditorCore::ResolveFolderPath(files, destination, &error).isEmpty()) return false;
            EditorCore::AssetOps operations(&files);
            if (!operations.Rename(relative, destination, &error)) {
                Logger::Instance().Error(QStringLiteral("RenameFolder: %1").arg(error));
                return false;
            }
            Logger::Instance().Info(QStringLiteral("RenameFolder: renamed '%1' to '%2'").arg(relative, destination));
            return true;
        }, canExecute
    });
}
}
