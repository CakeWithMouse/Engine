#pragma once
#include "ImportMigration.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

namespace ImportMigrationTests {

inline void WriteFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
        throw std::runtime_error("ImportMigrationTests: fixture write failed");
}

inline QByteArray ReadFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("ImportMigrationTests: fixture read failed");
    return file.readAll();
}

inline QString ModuleOf(const QString& projectDir)
{
    return QJsonDocument::fromJson(ReadFile(QDir(projectDir).filePath("Project.json")))
        .object().value("module").toString();
}

inline int Run()
{
    int checks = 0;
    auto check = [&](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); ++checks; };

    {
        QTemporaryDir temp;
        const QString project = temp.path();
        check(!ImportMigration::IsCodeProject(project), "empty project is no-code");
        WriteFile(QDir(project).filePath("Project.code.json"), "{}");
        check(ImportMigration::IsCodeProject(project), "Project.code.json marks a C++ project");
        QFile::remove(QDir(project).filePath("Project.code.json"));
        QDir().mkpath(QDir(project).filePath("Source"));
        check(ImportMigration::IsCodeProject(project), "Source dir marks a C++ project");
        QDir(QDir(project).filePath("Source")).removeRecursively();
        WriteFile(QDir(project).filePath("Game.vcxproj"), "<Project/>");
        check(ImportMigration::IsCodeProject(project), "Game.vcxproj marks a C++ project");
    }

    {
        QTemporaryDir temp;
        const QString project = temp.path() + "/Project";
        const QString runtime = temp.path() + "/Runtime";
        WriteFile(QDir(project).filePath("Project.json"),
                  QJsonDocument(QJsonObject{{"name", "P"}, {"module", "Game.dll"}, {"content", "Content"}}).toJson());
        WriteFile(QDir(project).filePath("Game.dll"), "old-module");
        WriteFile(QDir(runtime).filePath("Templates/SceneProject/Game.dll"), "new-module");

        QString error = "unset";
        const QString newModule = ImportMigration::MigrateNoCodeModule(project, runtime, &error);
        check(!newModule.isEmpty(), "no-code module migrates");
        check(newModule.startsWith("Binaries/Migrated/"), "migration uses a fresh Binaries path");
        check(ReadFile(QDir(project).filePath(newModule)) == "new-module", "template copied to the new module path");
        check(ReadFile(QDir(project).filePath("Game.dll")) == "old-module", "loaded module file is not overwritten");
        check(ModuleOf(project) == newModule, "Project.json repointed to the new module");
        bool backedUp = false;
        QDir backups(QDir(project).filePath(".painengine/GameDllBackups"));
        for (const auto& stamp : backups.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            if (ReadFile(backups.filePath(stamp + "/Game.dll")) == "old-module") backedUp = true;
        check(backedUp, "previous module backed up");

        // Same bytes as the template: no migration loop.
        const QString again = ImportMigration::MigrateNoCodeModule(project, runtime, &error);
        check(again.isEmpty(), "second migration against the same template is a no-op");
        // The migrated module now matches the template hash, so a repeat is refused too.
        check(ModuleOf(project) == newModule, "manifest unchanged after refused migration");
    }

    {
        QTemporaryDir temp;
        const QString project = temp.path() + "/Project";
        const QString runtime = temp.path() + "/Runtime";
        WriteFile(QDir(project).filePath("Project.json"),
                  QJsonDocument(QJsonObject{{"name", "P"}, {"module", "Game.dll"}, {"content", "Content"}}).toJson());
        QString error;
        check(ImportMigration::MigrateNoCodeModule(project, runtime, &error).isEmpty(), "missing template refuses migration");
        check(!error.isEmpty(), "missing template reports an error");
        WriteFile(QDir(runtime).filePath("Templates/SceneProject/Game.dll"), "new-module");
        const QString newModule = ImportMigration::MigrateNoCodeModule(project, runtime, &error);
        check(!newModule.isEmpty(), "missing current module still migrates");
        check(ModuleOf(project) == newModule, "manifest repointed without a backup");
    }

    std::cout << "Import migration checks: " << checks << std::endl;
    return checks;
}

}