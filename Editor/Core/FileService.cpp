#include "FileService.h"
#include "Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

FileService::FileService(Logger* logger, QObject* parent)
    : QObject(parent)
    , logger_(logger ? logger : &Logger::Instance())
{
}

void FileService::SetProjectRoot(const QString& projectRoot)
{
    projectRoot_ = QDir::cleanPath(QFileInfo(projectRoot).absoluteFilePath());
    LogInfo(QStringLiteral("FileService: sandbox root set to '%1'").arg(projectRoot_));
}

bool FileService::IsInSandbox(const QString& path) const
{
    if (projectRoot_.isEmpty())
    {
        return false;
    }

    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    return clean == projectRoot_ || clean.startsWith(projectRoot_ + QStringLiteral("/"), Qt::CaseInsensitive);
}

QString FileService::ResolveSandboxPath(const QString& relativeOrAbsolutePath) const
{
    if (projectRoot_.isEmpty())
    {
        return QString();
    }

    QFileInfo info(relativeOrAbsolutePath);
    QString resolved;
    if (info.isAbsolute())
    {
        resolved = QDir::cleanPath(info.absoluteFilePath());
    }
    else
    {
        resolved = QDir::cleanPath(QDir(projectRoot_).filePath(relativeOrAbsolutePath));
    }

    if (!IsInSandbox(resolved))
    {
        LogError(QStringLiteral("FileService: path '%1' is outside sandbox '%2'").arg(relativeOrAbsolutePath, projectRoot_));
        return QString();
    }

    return resolved;
}

bool FileService::ReadFile(const QString& path, QByteArray& outData, QString* outError) const
{
    const QString resolved = ResolveSandboxPath(path);
    if (resolved.isEmpty())
    {
        const QString err = QStringLiteral("Access denied: path is outside project sandbox");
        if (outError) *outError = err;
        LogError(err + QStringLiteral(" (") + path + QStringLiteral(")"));
        return false;
    }

    QFile file(resolved);
    if (!file.open(QIODevice::ReadOnly))
    {
        const QString err = file.errorString();
        if (outError) *outError = err;
        LogError(QStringLiteral("FileService: read failed '%1': %2").arg(resolved, err));
        return false;
    }

    outData = file.readAll();
    return true;
}

bool FileService::WriteFileAtomic(const QString& path, const QByteArray& data, QString* outError)
{
    const QString resolved = ResolveSandboxPath(path);
    if (resolved.isEmpty())
    {
        const QString err = QStringLiteral("Access denied: path is outside project sandbox");
        if (outError) *outError = err;
        LogError(err + QStringLiteral(" (") + path + QStringLiteral(")"));
        return false;
    }

    QFileInfo info(resolved);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
    {
        const QString err = QStringLiteral("Cannot create parent directory: ") + dir.absolutePath();
        if (outError) *outError = err;
        LogError(err);
        return false;
    }

    QSaveFile file(resolved);
    if (!file.open(QIODevice::WriteOnly))
    {
        const QString err = file.errorString();
        if (outError) *outError = err;
        LogError(QStringLiteral("FileService: cannot open '%1' for write: %2").arg(resolved, err));
        return false;
    }

    if (file.write(data) != data.size())
    {
        const QString err = QStringLiteral("Incomplete write to ") + resolved;
        if (outError) *outError = err;
        file.cancelWriting();
        LogError(err);
        return false;
    }

    if (!file.commit())
    {
        const QString err = QStringLiteral("Atomic commit failed for ") + resolved;
        if (outError) *outError = err;
        LogError(err);
        return false;
    }

    LogInfo(QStringLiteral("FileService: wrote %1 bytes to '%2'").arg(data.size()).arg(resolved));
    return true;
}

bool FileService::DeleteFile(const QString& path, QString* outError)
{
    const QString resolved = ResolveSandboxPath(path);
    if (resolved.isEmpty())
    {
        const QString err = QStringLiteral("Access denied: path is outside project sandbox");
        if (outError) *outError = err;
        LogError(err);
        return false;
    }

    if (!QFile::exists(resolved))
    {
        return true; // Already deleted
    }

    if (!QFile::remove(resolved))
    {
        const QString err = QStringLiteral("Failed to remove file ") + resolved;
        if (outError) *outError = err;
        LogError(err);
        return false;
    }

    LogInfo(QStringLiteral("FileService: deleted '%1'").arg(resolved));
    return true;
}

bool FileService::DirectoryExists(const QString& path) const
{
    const QString resolved = ResolveSandboxPath(path);
    if (resolved.isEmpty())
    {
        return false;
    }
    return QDir(resolved).exists();
}

bool FileService::CreateDirectoryPath(const QString& path, QString* outError)
{
    const QString resolved = ResolveSandboxPath(path);
    if (resolved.isEmpty())
    {
        const QString err = QStringLiteral("Access denied: path is outside project sandbox");
        if (outError) *outError = err;
        return false;
    }

    QDir dir(resolved);
    if (dir.exists())
    {
        return true;
    }

    if (!dir.mkpath(QStringLiteral(".")))
    {
        const QString err = QStringLiteral("Failed to mkpath: ") + resolved;
        if (outError) *outError = err;
        LogError(err);
        return false;
    }

    LogInfo(QStringLiteral("FileService: created directory '%1'").arg(resolved));
    return true;
}

QStringList FileService::ListFiles(const QString& subDir, const QStringList& nameFilters) const
{
    const QString targetDir = subDir.isEmpty() ? projectRoot_ : ResolveSandboxPath(subDir);
    if (targetDir.isEmpty() || !QDir(targetDir).exists())
    {
        return {};
    }

    QDir dir(targetDir);
    return dir.entryList(nameFilters.isEmpty() ? QStringList(QStringLiteral("*")) : nameFilters,
                          QDir::Files | QDir::NoDotAndDotDot);
}

void FileService::LogInfo(const QString& msg) const
{
    if (logger_) logger_->Info(msg);
}

void FileService::LogError(const QString& msg) const
{
    if (logger_) logger_->Error(msg);
}

