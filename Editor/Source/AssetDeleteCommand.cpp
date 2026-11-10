#include "MainWindow.h"
#include "EditorLocale.h"
#include "QtHost.h"
#include "Logger.h"
#include "FileService.h"
#include "AssetOps.h"

#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

namespace {

int CountSceneUsages(IEditorHost* host, const QString& path, bool isDir, const QString& contentRoot)
{
    if (!host) return 0;
    const QString itemName = QFileInfo(path).fileName();
    const QString vfsPath = QStringLiteral("/Game/") + QDir::fromNativeSeparators(QDir(contentRoot).relativeFilePath(path));
    const QString folderVfs = isDir ? (vfsPath.endsWith(QLatin1Char('/')) ? vfsPath : (vfsPath + QLatin1Char('/'))) : QString();

    int usedCount = 0;
    for (const auto& obj : host->ReadSnapshot().objects)
    {
        const QString m = QString::fromUtf8(obj.modelAsset.c_str());
        const QString t = QString::fromUtf8(obj.textureAsset.c_str());
        const QString mat = QString::fromUtf8(obj.materialAsset.c_str());
        if (isDir)
        {
            if ((!m.isEmpty() && m.startsWith(folderVfs)) || (!t.isEmpty() && t.startsWith(folderVfs)) || (!mat.isEmpty() && mat.startsWith(folderVfs)))
                ++usedCount;
        }
        else if ((!m.isEmpty() && (m == vfsPath || m.endsWith(itemName))) ||
                 (!t.isEmpty() && (t == vfsPath || t.endsWith(itemName))) ||
                 (!mat.isEmpty() && (mat == vfsPath || mat.endsWith(itemName))))
        {
            ++usedCount;
        }
    }
    return usedCount;
}

bool PromptDeleteConfirmation(QWidget* parent, const QString& path, bool isDir, int usedCount)
{
    const QString itemName = QFileInfo(path).fileName();
    auto warn = [parent](const QString& title, const QString& text) {
        return QMessageBox::warning(parent, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
    };
    auto ask = [parent](const QString& title, const QString& text) {
        return QMessageBox::question(parent, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
    };

    if (isDir)
    {
        const int count = QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).size();
        if (usedCount > 0)
            return warn(Tr("Удаление папки", "Delete Folder"),
                        Tr("Папка '%1' содержит ассеты, используемые на сцене: %2 объектов. Удалить её и всё содержимое безвозвратно?",
                           "Folder '%1' contains assets used on the scene: %2 objects. Delete it and all its contents permanently?").arg(itemName).arg(usedCount));
        if (count > 0)
            return warn(Tr("Удаление папки", "Delete Folder"),
                        Tr("Папка '%1' не пуста и содержит файлы: %2 шт. Удалить её и всё содержимое безвозвратно?",
                           "Folder '%1' is not empty and contains %2 items. Delete it and all its contents permanently?").arg(itemName).arg(count));
        return ask(Tr("Удаление папки", "Delete Folder"),
                   Tr("Удалить пустую папку '%1'?", "Delete empty folder '%1'?").arg(itemName));
    }

    return (usedCount > 0
        ? warn(Tr("Удаление ассета", "Delete Asset"),
               Tr("Ассет используется на сцене: %1 объектов. Удалить файл и сопутствующие настройки?",
                  "Asset is currently used on the scene: %1 objects. Delete the file and companion settings?").arg(usedCount))
        : ask(Tr("Удаление ассета", "Delete Asset"),
              Tr("Удалить ассет '%1' и сопутствующие файлы?", "Delete asset '%1' and companion files?").arg(itemName)));
}

} // namespace

bool EditorMainWindow::DeleteAsset(const QString& path, bool skipPrompt)
{
    if (path.isEmpty() || !host_) return false;

    const QString projectDir = QFileInfo(ProjectPath()).dir().absolutePath();
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);

    FileService fs(&Logger::Instance(), this);
    fs.SetProjectRoot(projectDir);
    fs.SetContentRoot(contentRoot);

    if (fs.IsProtectedDirectory(path))
    {
        Logger::Instance().Error(QStringLiteral("DeleteAsset: cannot delete protected path '%1'").arg(path));
        return false;
    }

    const QString relPath = QDir(projectDir).relativeFilePath(path);
    EditorCore::AssetOps ops(&fs);
    if (!ops.Exists(relPath))
    {
        Logger::Instance().Error(QStringLiteral("DeleteAsset: asset does not exist: %1").arg(path));
        return false;
    }

    const bool isDir = QFileInfo(path).isDir();
    const int usedCount = CountSceneUsages(host_, path, isDir, contentRoot);
    if (!skipPrompt && !PromptDeleteConfirmation(this, path, isDir, usedCount))
    {
        return false;
    }

    QString error;
    if (!ops.Delete(relPath, &error, !isDir))
    {
        Logger::Instance().Error(QStringLiteral("DeleteAsset: failed to delete '%1': %2").arg(path, error));
        return false;
    }

    Logger::Instance().Info(QStringLiteral("DeleteAsset: successfully deleted %1 '%2'").arg(isDir ? QStringLiteral("directory") : QStringLiteral("file"), path));
    return true;
}
