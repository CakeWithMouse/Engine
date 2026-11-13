#include "ImportMigration.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace ImportMigration {

bool IsCodeProject(const QString& projectDir)
{
    const QDir dir(projectDir);
    return QFileInfo::exists(dir.filePath(QStringLiteral("Project.code.json")))
        || QFileInfo::exists(dir.filePath(QStringLiteral("Source")))
        || QFileInfo::exists(dir.filePath(QStringLiteral("Game.vcxproj")));
}

namespace {

QByteArray FileHash(const QString& path, bool* ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        *ok = false;
        return {};
    }
    *ok = true;
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

}

QString MigrateNoCodeModule(const QString& projectDir, const QString& runtimeDir, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error) *error = message;
        return QString();
    };

    const QDir project(projectDir);
    const QString templateDll = QDir(runtimeDir).filePath(QStringLiteral("Templates/SceneProject/Game.dll"));
    if (!QFileInfo::exists(templateDll))
        return fail(QStringLiteral("runtime template Game.dll is missing"));

    QFile manifest(project.filePath(QStringLiteral("Project.json")));
    if (!manifest.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("cannot read Project.json"));
    const auto json = QJsonDocument::fromJson(manifest.readAll()).object();
    manifest.close();
    const QString previousModule = json.value(QStringLiteral("module")).toString();
    if (previousModule.isEmpty())
        return fail(QStringLiteral("Project.json has no module"));

    const QString currentDll = project.filePath(previousModule);
    if (QFileInfo::exists(currentDll))
    {
        bool currentOk = false, templateOk = false;
        const auto currentHash = FileHash(currentDll, &currentOk);
        const auto templateHash = FileHash(templateDll, &templateOk);
        if (currentOk && templateOk && currentHash == templateHash)
            return fail(QStringLiteral("module already matches the runtime template"));
    }

    const QString stamp = QString::number(QCoreApplication::applicationPid())
        + QStringLiteral("-") + QString::number(QDateTime::currentMSecsSinceEpoch());
    if (QFileInfo::exists(currentDll))
    {
        const QString backupDir = project.filePath(QStringLiteral(".painengine/GameDllBackups/") + stamp);
        if (!QDir().mkpath(backupDir)
            || !QFile::copy(currentDll, QDir(backupDir).filePath(QStringLiteral("Game.dll"))))
            return fail(QStringLiteral("cannot back up the current Game.dll"));
    }

    const QString newRelative = QStringLiteral("Binaries/Migrated/") + stamp + QStringLiteral("/Game.dll");
    const QString newDll = project.filePath(newRelative);
    if (!QDir().mkpath(QFileInfo(newDll).absolutePath()) || !QFile::copy(templateDll, newDll))
        return fail(QStringLiteral("cannot copy the template Game.dll"));

    QSaveFile out(project.filePath(QStringLiteral("Project.json")));
    if (!out.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("cannot write Project.json"));
    auto updated = json;
    updated[QStringLiteral("module")] = newRelative;
    if (out.write(QJsonDocument(updated).toJson()) < 0 || !out.commit())
        return fail(QStringLiteral("cannot write Project.json"));
    return newRelative;
}

}