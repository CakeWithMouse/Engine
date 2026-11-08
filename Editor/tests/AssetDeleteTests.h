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

    std::cout << "AssetDeleteTests: " << checks << " checks passed." << std::endl;
}

} // namespace AssetDeleteTests

