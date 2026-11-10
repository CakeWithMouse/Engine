#pragma once

#include "CommandRegistry.h"
#include "FileService.h"
#include "AssetOps.h"
#include "Logger.h"

#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#include <stdexcept>

namespace AssetDeleteTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string("AssetDelete check failed: ") + message);
    }
}

inline void Run()
{
    std::cout << "[Test] Running AssetDeleteTests..." << std::endl;
    int checks = 0;

    // 1. Assets.Delete command registration
    {
        // When registered, CommandRegistry can find Assets.Delete
        // Note: registered in EditorMainWindow::RegisterAssetCommands, here we check command handler logic via AssetOps
        const auto& suffixes = EditorCore::AssetOps::CompanionSuffixes();
        Check(suffixes.contains(QStringLiteral(".importsettings.json")), "Companion list contains .importsettings.json");
        Check(suffixes.contains(QStringLiteral(".source.json")), "Companion list contains .source.json");
        Check(suffixes.contains(QStringLiteral(".import.json")), "Companion list contains .import.json");
        ++checks;
    }

    // 2. Sandbox enforcement and nonexistent file deletion
    {
        QTemporaryDir tempDir;
        Logger logger;
        FileService fs(&logger);
        fs.SetProjectRoot(tempDir.path());
        EditorCore::AssetOps ops(&fs);

        QString error;
        Check(!ops.Delete(QStringLiteral("../outside.glb"), &error, true), "Delete outside sandbox rejected");
        Check(!error.isEmpty(), "Error populated for outside sandbox");
        ++checks;

        Check(!ops.Delete(QStringLiteral("NonExistent.glb"), &error, true), "Delete nonexistent file rejected");
        Check(!error.isEmpty(), "Error populated for nonexistent file");
        ++checks;
    }

    // 3. Asset deletion with companion cleanup
    {
        QTemporaryDir tempDir;
        Logger logger;
        FileService fs(&logger);
        fs.SetProjectRoot(tempDir.path());
        EditorCore::AssetOps ops(&fs);

        const QString modelRel = QStringLiteral("Content/Models/Hero.fbx");
        const QString otherRel = QStringLiteral("Content/Models/Other.fbx");

        QString err;
        ops.CreateFile(modelRel, "FBX_DATA", &err);
        ops.CreateFile(modelRel + QStringLiteral(".importsettings.json"), "{\"scale\":0.01}", &err);
        ops.CreateFile(modelRel + QStringLiteral(".source.json"), "{\"source\":\"C:/src/hero.fbx\"}", &err);
        ops.CreateFile(modelRel + QStringLiteral(".import.json"), "{\"imported\":true}", &err);

        // Another model that must not be touched
        ops.CreateFile(otherRel, "OTHER_FBX", &err);
        ops.CreateFile(otherRel + QStringLiteral(".importsettings.json"), "{\"scale\":1.0}", &err);

        Check(ops.Exists(modelRel), "Model exists before deletion");
        Check(ops.Exists(modelRel + QStringLiteral(".importsettings.json")), "Model sidecar exists before deletion");
        Check(ops.Exists(modelRel + QStringLiteral(".source.json")), "Model source sidecar exists before deletion");
        Check(ops.Exists(modelRel + QStringLiteral(".import.json")), "Model import sidecar exists before deletion");
        ++checks;

        // Delete with companions
        const bool deleted = ops.Delete(modelRel, &err, true);
        Check(deleted, "Delete with companions succeeded");
        Check(!ops.Exists(modelRel), "Model deleted from disk");
        Check(!ops.Exists(modelRel + QStringLiteral(".importsettings.json")), "Companion .importsettings.json deleted from disk");
        Check(!ops.Exists(modelRel + QStringLiteral(".source.json")), "Companion .source.json deleted from disk");
        Check(!ops.Exists(modelRel + QStringLiteral(".import.json")), "Companion .import.json deleted from disk");
        ++checks;

        // Other model and its sidecar must remain untouched
        Check(ops.Exists(otherRel), "Unrelated model still exists");
        Check(ops.Exists(otherRel + QStringLiteral(".importsettings.json")), "Unrelated model sidecar still exists");
        ++checks;
    }

    // 4. Delete directory recursively
    {
        QTemporaryDir tempDir;
        Logger logger;
        FileService fs(&logger);
        fs.SetProjectRoot(tempDir.path());
        EditorCore::AssetOps ops(&fs);

        QString err;
        ops.CreateFile(QStringLiteral("Folder/Sub/item.txt"), "DATA", &err);
        Check(ops.Exists(QStringLiteral("Folder/Sub/item.txt")), "Nested file exists");
        Check(ops.Delete(QStringLiteral("Folder"), &err), "Delete directory recursively succeeded");
        Check(!ops.Exists(QStringLiteral("Folder")), "Directory deleted");
        ++checks;
    }

    // 5. FileService::DeleteDirectory sandbox and direct behavior
    {
        QTemporaryDir tempDir;
        Logger logger;
        FileService fs(&logger);
        fs.SetProjectRoot(tempDir.path());

        QString err;
        Check(!fs.DeleteDirectory(QStringLiteral("../outside"), &err), "DeleteDirectory outside sandbox rejected");
        Check(fs.DeleteDirectory(QStringLiteral("nonexistent_dir"), &err), "DeleteDirectory nonexistent returns true");

        const QString subDirPath = QDir(tempDir.path()).filePath(QStringLiteral("TestDir/Nested"));
        QDir().mkpath(subDirPath);
        QFile file(QDir(subDirPath).filePath(QStringLiteral("data.bin")));
        file.open(QIODevice::WriteOnly);
        file.write("hello");
        file.close();

        Check(QDir(subDirPath).exists(), "Subdirectory created");
        Check(fs.DeleteDirectory(QStringLiteral("TestDir"), &err), "DeleteDirectory succeeded");
        Check(!QDir(tempDir.path() + QStringLiteral("/TestDir")).exists(), "Directory removed from filesystem");
        ++checks;
    }

    // 6. Protection check for Content, Source, and Project root (in Core)
    {
        QTemporaryDir tempDir;
        Logger logger;
        FileService fs(&logger);
        const QString projectDir = tempDir.path();
        const QString contentDir = QDir(projectDir).filePath(QStringLiteral("Content"));
        const QString sourceDir = QDir(projectDir).filePath(QStringLiteral("Source"));
        QDir().mkpath(contentDir);
        QDir().mkpath(sourceDir);

        fs.SetProjectRoot(projectDir);
        fs.SetContentRoot(contentDir);

        Check(fs.IsProtectedDirectory(projectDir), "Project root is protected");
        Check(fs.IsProtectedDirectory(contentDir), "Content root is protected");
        Check(fs.IsProtectedDirectory(sourceDir), "Source root is protected");

        QString err;
        Check(!fs.DeleteDirectory(QStringLiteral("Content"), &err), "Cannot delete root Content");
        Check(QDir(contentDir).exists(), "Root Content directory still exists");
        Check(!fs.DeleteDirectory(QStringLiteral("Source"), &err), "Cannot delete root Source");
        Check(QDir(sourceDir).exists(), "Root Source directory still exists");
        Check(!fs.DeleteDirectory(QStringLiteral(""), &err), "Cannot delete project root");

        // Subfolder named "Source" inside Content MUST NOT be protected and can be deleted
        const QString subSource = QDir(contentDir).filePath(QStringLiteral("Source"));
        QDir().mkpath(subSource);
        Check(QDir(subSource).exists(), "Subfolder Content/Source created");
        Check(!fs.IsProtectedDirectory(subSource), "Content/Source is NOT protected");
        Check(fs.DeleteDirectory(QStringLiteral("Content/Source"), &err), "Deleting Content/Source succeeds");
        Check(!QDir(subSource).exists(), "Content/Source removed");

        // Subfolder named "Content" inside Content MUST NOT be protected and can be deleted
        const QString subContent = QDir(contentDir).filePath(QStringLiteral("Content"));
        QDir().mkpath(subContent);
        Check(QDir(subContent).exists(), "Subfolder Content/Content created");
        Check(!fs.IsProtectedDirectory(subContent), "Content/Content is NOT protected");
        Check(fs.DeleteDirectory(QStringLiteral("Content/Content"), &err), "Deleting Content/Content succeeds");
        Check(!QDir(subContent).exists(), "Content/Content removed");
        ++checks;
    }

    std::cout << "AssetDeleteTests: " << checks << " checks passed." << std::endl;
}

} // namespace AssetDeleteTests

