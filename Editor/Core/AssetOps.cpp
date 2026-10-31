#include "AssetOps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EditorCore
{

namespace
{

bool CopyDirectoryRecursively(const QString& srcPath, const QString& dstPath)
{
    QDir srcDir(srcPath);
    if (!srcDir.exists())
        return false;

    QDir dstDir(dstPath);
    if (!dstDir.exists() && !dstDir.mkpath(QStringLiteral(".")))
        return false;

    const QFileInfoList entries = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries)
    {
        const QString srcItem = entry.absoluteFilePath();
        const QString dstItem = dstPath + QStringLiteral("/") + entry.fileName();

        if (entry.isDir())
        {
            if (!CopyDirectoryRecursively(srcItem, dstItem))
                return false;
        }
        else
        {
            if (QFile::exists(dstItem))
                QFile::remove(dstItem);
            if (!QFile::copy(srcItem, dstItem))
                return false;
        }
    }
    return true;
}

} // namespace

AssetOps::AssetOps(FileService* fileService)
    : fileService_(fileService)
{
}

bool AssetOps::CreateFile(const QString& relPath, const QByteArray& content, QString* outError)
{
    if (!fileService_)
    {
        if (outError) *outError = QStringLiteral("FileService is null");
        return false;
    }

    const QString absPath = fileService_->ResolveSandboxPath(relPath);
    if (absPath.isEmpty())
    {
        if (outError) *outError = QStringLiteral("Path is outside project sandbox: %1").arg(relPath);
        return false;
    }

    return fileService_->WriteFileAtomic(absPath, content, outError);
}

bool AssetOps::CreateFolder(const QString& relPath, QString* outError)
{
    if (!fileService_)
    {
        if (outError) *outError = QStringLiteral("FileService is null");
        return false;
    }

    const QString absPath = fileService_->ResolveSandboxPath(relPath);
    if (absPath.isEmpty())
    {
        if (outError) *outError = QStringLiteral("Path is outside project sandbox: %1").arg(relPath);
        return false;
    }

    return fileService_->CreateDirectoryPath(absPath, outError);
}

bool AssetOps::Rename(const QString& oldRelPath, const QString& newRelPath, QString* outError)
{
    if (!fileService_)
    {
        if (outError) *outError = QStringLiteral("FileService is null");
        return false;
    }

    const QString oldAbs = fileService_->ResolveSandboxPath(oldRelPath);
    const QString newAbs = fileService_->ResolveSandboxPath(newRelPath);
    if (oldAbs.isEmpty() || newAbs.isEmpty())
    {
        if (outError) *outError = QStringLiteral("Path is outside project sandbox");
        return false;
    }

    if (!QFileInfo::exists(oldAbs))
    {
        if (outError) *outError = QStringLiteral("Source asset does not exist: %1").arg(oldRelPath);
        return false;
    }

    if (QFileInfo::exists(newAbs))
    {
        if (outError) *outError = QStringLiteral("Target asset already exists: %1").arg(newRelPath);
        return false;
    }

    const QFileInfo newInfo(newAbs);
    QDir parentDir = newInfo.dir();
    if (!parentDir.exists() && !parentDir.mkpath(QStringLiteral(".")))
    {
        if (outError) *outError = QStringLiteral("Failed to create destination directory: %1").arg(parentDir.absolutePath());
        return false;
    }

    if (!QFile::rename(oldAbs, newAbs))
    {
        QDir dir;
        if (!dir.rename(oldAbs, newAbs))
        {
            if (outError) *outError = QStringLiteral("Failed to rename '%1' to '%2'").arg(oldRelPath, newRelPath);
            return false;
        }
    }

    return true;
}

bool AssetOps::Duplicate(const QString& srcRelPath, const QString& dstRelPath, QString* outError, QString* outCreatedPath)
{
    if (!fileService_)
    {
        if (outError) *outError = QStringLiteral("FileService is null");
        return false;
    }

    const QString srcAbs = fileService_->ResolveSandboxPath(srcRelPath);
    if (srcAbs.isEmpty())
    {
        if (outError) *outError = QStringLiteral("Source path is outside project sandbox: %1").arg(srcRelPath);
        return false;
    }

    const QFileInfo srcInfo(srcAbs);
    if (!srcInfo.exists())
    {
        if (outError) *outError = QStringLiteral("Source asset does not exist: %1").arg(srcRelPath);
        return false;
    }

    QString targetRel = dstRelPath;
    if (targetRel.isEmpty())
    {
        targetRel = GenerateDuplicateName(srcRelPath);
        int counter = 1;
        while (Exists(targetRel))
        {
            const QFileInfo fi(srcRelPath);
            const QString base = fi.completeBaseName();
            const QString ext = fi.suffix().isEmpty() ? QString() : (QStringLiteral(".") + fi.suffix());
            const QString dir = fi.dir().path();
            const QString name = QStringLiteral("%1_Copy_%2%3").arg(base).arg(counter++).arg(ext);
            targetRel = (dir == QStringLiteral(".") || dir.isEmpty()) ? name : (dir + QStringLiteral("/") + name);
        }
    }

    const QString dstAbs = fileService_->ResolveSandboxPath(targetRel);
    if (dstAbs.isEmpty())
    {
        if (outError) *outError = QStringLiteral("Target path is outside project sandbox: %1").arg(targetRel);
        return false;
    }

    if (QFileInfo::exists(dstAbs))
    {
        if (outError) *outError = QStringLiteral("Target asset already exists: %1").arg(targetRel);
        return false;
    }

    if (srcInfo.isDir())
    {
        if (!CopyDirectoryRecursively(srcAbs, dstAbs))
        {
            if (outError) *outError = QStringLiteral("Failed to duplicate directory '%1'").arg(srcRelPath);
            return false;
        }
    }
    else
    {
        QByteArray data;
        if (!fileService_->ReadFile(srcAbs, data, outError))
            return false;

        if (!CreateFile(targetRel, data, outError))
            return false;
    }

    if (outCreatedPath)
        *outCreatedPath = targetRel;

    return true;
}

bool AssetOps::Delete(const QString& relPath, QString* outError)
{
    if (!fileService_)
    {
        if (outError) *outError = QStringLiteral("FileService is null");
        return false;
    }

    const QString absPath = fileService_->ResolveSandboxPath(relPath);
    if (absPath.isEmpty())
    {
        if (outError) *outError = QStringLiteral("Path is outside project sandbox: %1").arg(relPath);
        return false;
    }

    const QFileInfo info(absPath);
    if (!info.exists())
    {
        if (outError) *outError = QStringLiteral("Path does not exist: %1").arg(relPath);
        return false;
    }

    if (info.isDir())
    {
        QDir dir(absPath);
        if (!dir.removeRecursively())
        {
            if (outError) *outError = QStringLiteral("Failed to remove directory: %1").arg(relPath);
            return false;
        }
        return true;
    }

    return fileService_->DeleteFile(absPath, outError);
}

bool AssetOps::Exists(const QString& relPath) const
{
    if (!fileService_)
        return false;

    const QString absPath = fileService_->ResolveSandboxPath(relPath);
    return !absPath.isEmpty() && QFileInfo::exists(absPath);
}

bool AssetOps::IsFolder(const QString& relPath) const
{
    if (!fileService_)
        return false;

    const QString absPath = fileService_->ResolveSandboxPath(relPath);
    return !absPath.isEmpty() && QFileInfo(absPath).isDir();
}

QStringList AssetOps::List(const QString& relDir) const
{
    if (!fileService_)
        return {};

    const QString absDir = fileService_->ResolveSandboxPath(relDir.isEmpty() ? QStringLiteral(".") : relDir);
    if (absDir.isEmpty() || !QDir(absDir).exists())
        return {};

    QDir dir(absDir);
    return dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
}

QString AssetOps::GenerateDuplicateName(const QString& relPath)
{
    const QFileInfo fi(relPath);
    const QString base = fi.completeBaseName();
    const QString ext = fi.suffix().isEmpty() ? QString() : (QStringLiteral(".") + fi.suffix());
    const QString dir = fi.dir().path();
    const QString copyName = QStringLiteral("%1_Copy%2").arg(base, ext);

    if (dir == QStringLiteral(".") || dir.isEmpty())
        return copyName;

    return dir + QStringLiteral("/") + copyName;
}

} // namespace EditorCore
