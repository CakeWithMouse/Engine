#pragma once

#include "FileService.h"
#include "Logger.h"

#include <QDir>
#include <QFileInfo>

namespace EditorCore
{

namespace FolderPathDetail
{
inline bool Reject(const QString& message, QString* outError)
{
    if (outError) *outError = message;
    Logger::Instance().Error(QStringLiteral("FolderPaths: ") + message);
    return false;
}

inline bool Contains(const QString& root, const QString& path)
{
#ifdef Q_OS_WIN
    constexpr auto sensitivity = Qt::CaseInsensitive;
#else
    constexpr auto sensitivity = Qt::CaseSensitive;
#endif
    const QString prefix = root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
    return path.compare(root, sensitivity) == 0 || path.startsWith(prefix, sensitivity);
}
} // namespace FolderPathDetail

inline bool ValidateFolderRelativePath(const QString& relPath, QString* outError = nullptr)
{
    QString normalized = relPath;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalized.isEmpty() || normalized.startsWith(QLatin1Char('/')) ||
        QDir::isAbsolutePath(normalized) || normalized.contains(QLatin1Char(':')) ||
        normalized.contains(QChar(0)))
        return FolderPathDetail::Reject(QStringLiteral("Folder path must be project-relative: %1").arg(relPath), outError);

    for (const QString& component : normalized.split(QLatin1Char('/')))
    {
        if (component == QStringLiteral(".."))
            return FolderPathDetail::Reject(QStringLiteral("Parent traversal is not allowed: %1").arg(relPath), outError);
    }
    return true;
}

inline bool ValidateFolderName(const QString& name, QString* outError = nullptr)
{
    const auto reject = [&]() {
        return FolderPathDetail::Reject(QStringLiteral("Invalid folder name: %1").arg(name), outError);
    };
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..") ||
        name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
        return reject();

    const QString invalid = QStringLiteral("<>:\"/\\|?*");
    for (const QChar ch : name)
    {
        if (ch.unicode() < 32 || invalid.contains(ch))
            return reject();
    }

    const QString base = name.section(QLatin1Char('.'), 0, 0).trimmed().toUpper();
    if (base == QStringLiteral("CON") || base == QStringLiteral("PRN") ||
        base == QStringLiteral("AUX") || base == QStringLiteral("NUL") ||
        base == QStringLiteral("CONIN$") || base == QStringLiteral("CONOUT$"))
        return reject();
    if (base.size() == 4 && (base.startsWith(QStringLiteral("COM")) || base.startsWith(QStringLiteral("LPT"))))
    {
        const QChar digit = base.at(3);
        if ((digit >= QLatin1Char('1') && digit <= QLatin1Char('9')) ||
            digit == QChar(0x00b9) || digit == QChar(0x00b2) || digit == QChar(0x00b3))
            return reject();
    }
    return true;
}

inline QString ResolveFolderPath(FileService& fileService, const QString& relPath, QString* outError = nullptr)
{
    if (!ValidateFolderRelativePath(relPath, outError))
        return {};

    const auto reject = [&](const QString& reason) {
        FolderPathDetail::Reject(QStringLiteral("%1: %2").arg(reason, relPath), outError);
        return QString();
    };
    const QFileInfo rootInfo(fileService.GetProjectRoot());
    const QString root = QDir::fromNativeSeparators(rootInfo.canonicalFilePath());
    if (root.isEmpty() || !rootInfo.isDir())
        return reject(QStringLiteral("Cannot resolve project root"));

    QString normalized = relPath;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QString resolved = fileService.ResolveSandboxPath(normalized);
    if (resolved.isEmpty())
        return reject(QStringLiteral("Path is outside project sandbox"));

    QString ancestor = resolved;
    for (;;)
    {
        const QFileInfo info(ancestor);
        const QString canonical = QDir::fromNativeSeparators(info.canonicalFilePath());
        bool linked = info.isSymLink();
#ifdef Q_OS_WIN
        linked = linked || info.isJunction();
#endif
        if (linked && canonical.isEmpty())
            return reject(QStringLiteral("Cannot resolve linked folder ancestor"));
        if (info.exists())
        {
            if (canonical.isEmpty())
                return reject(QStringLiteral("Cannot resolve folder ancestor"));
            if (!FolderPathDetail::Contains(root, canonical))
                return reject(QStringLiteral("Canonical folder path is outside project sandbox"));
            if (ancestor != resolved && !info.isDir())
                return reject(QStringLiteral("Folder parent is not a directory"));
            return resolved;
        }
        const QString parent = info.dir().absolutePath();
        if (parent == ancestor || ancestor == fileService.GetProjectRoot())
            return reject(QStringLiteral("Cannot resolve folder ancestor"));
        ancestor = parent;
    }
}

} // namespace EditorCore
