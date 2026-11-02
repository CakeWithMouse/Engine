#pragma once

#include "../../Core/AssetOps.h"
#include "../../Core/FileService.h"
#include "../../Core/Logger.h"

#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

namespace
{

void CheckAsset(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (AssetOps): %s\n", msg);
        std::exit(1);
    }
}

inline void RunAssetOpsTests()
{
    using namespace EditorCore;

    // 1. GenerateDuplicateName logic
    {
        const QString n1 = AssetOps::GenerateDuplicateName(QStringLiteral("scene.json"));
        CheckAsset(n1 == QStringLiteral("scene_Copy.json"), "Duplicate name for scene.json");

        const QString n2 = AssetOps::GenerateDuplicateName(QStringLiteral("Materials/rock.mat"));
        CheckAsset(n2 == QStringLiteral("Materials/rock_Copy.mat"), "Duplicate name for Materials/rock.mat");

        const QString n3 = AssetOps::GenerateDuplicateName(QStringLiteral("Sub/folder"));
        CheckAsset(n3 == QStringLiteral("Sub/folder_Copy"), "Duplicate name for folder without extension");
    }

    QTemporaryDir tempDir;
    CheckAsset(tempDir.isValid(), "Temporary directory created for AssetOps tests");

    Logger logger;
    FileService fs(&logger);
    fs.SetProjectRoot(tempDir.path());
    AssetOps ops(&fs);

    // 2. CreateFile & Exists & IsFolder
    {
        QString error;
        const QByteArray content = "MAT_CONTENT";
        CheckAsset(ops.CreateFile(QStringLiteral("Materials/test.mat"), content, &error), "CreateFile creates nested file");
        CheckAsset(error.isEmpty(), "No error on valid CreateFile");
        CheckAsset(ops.Exists(QStringLiteral("Materials/test.mat")), "Created file exists");
        CheckAsset(!ops.IsFolder(QStringLiteral("Materials/test.mat")), "Created file is not folder");

        // Sandbox escape rejected
        CheckAsset(!ops.CreateFile(QStringLiteral("../outside.txt"), content, &error), "CreateFile outside sandbox rejected");
        CheckAsset(!error.isEmpty(), "Error populated for outside sandbox CreateFile");
    }

    // 3. CreateFolder
    {
        QString error;
        CheckAsset(ops.CreateFolder(QStringLiteral("Textures/Albedo"), &error), "CreateFolder creates nested directory");
        CheckAsset(error.isEmpty(), "No error on valid CreateFolder");
        CheckAsset(ops.Exists(QStringLiteral("Textures/Albedo")), "Created folder exists");
        CheckAsset(ops.IsFolder(QStringLiteral("Textures/Albedo")), "Created folder is folder");

        // Sandbox escape rejected
        CheckAsset(!ops.CreateFolder(QStringLiteral("../evil_dir"), &error), "CreateFolder outside sandbox rejected");
        CheckAsset(!error.isEmpty(), "Error populated for outside sandbox CreateFolder");
    }

    // 4. List
    {
        QString error;
        CheckAsset(ops.CreateFile(QStringLiteral("Materials/metal.mat"), "METAL", &error), "Create second file");
        const QStringList list = ops.List(QStringLiteral("Materials"));
        CheckAsset(list.contains(QStringLiteral("test.mat")), "List contains test.mat");
        CheckAsset(list.contains(QStringLiteral("metal.mat")), "List contains metal.mat");
    }

    // 5. Rename
    {
        QString error;
        CheckAsset(ops.Rename(QStringLiteral("Materials/test.mat"), QStringLiteral("Materials/test_renamed.mat"), &error), "Rename file success");
        CheckAsset(!ops.Exists(QStringLiteral("Materials/test.mat")), "Old path no longer exists after rename");
        CheckAsset(ops.Exists(QStringLiteral("Materials/test_renamed.mat")), "New path exists after rename");

        // Rename to existing destination fails
        CheckAsset(!ops.Rename(QStringLiteral("Materials/test_renamed.mat"), QStringLiteral("Materials/metal.mat"), &error), "Rename to existing fails");
        CheckAsset(!error.isEmpty(), "Error populated on collision rename");

        // Rename non-existent fails
        CheckAsset(!ops.Rename(QStringLiteral("Materials/nonexistent.mat"), QStringLiteral("Materials/nowhere.mat"), &error), "Rename nonexistent fails");

        // Rename outside sandbox fails
        CheckAsset(!ops.Rename(QStringLiteral("Materials/metal.mat"), QStringLiteral("../outside.mat"), &error), "Rename outside sandbox fails");
    }

    // 6. Duplicate
    {
        QString error;
        QString createdPath;

        // Auto-duplicate name
        CheckAsset(ops.Duplicate(QStringLiteral("Materials/test_renamed.mat"), QString(), &error, &createdPath), "Duplicate with auto name success");
        CheckAsset(createdPath == QStringLiteral("Materials/test_renamed_Copy.mat"), "Auto duplicate path matches expected");
        CheckAsset(ops.Exists(QStringLiteral("Materials/test_renamed_Copy.mat")), "Auto duplicated file exists");

        // Duplicate again with collision resolving
        QString createdPath2;
        CheckAsset(ops.Duplicate(QStringLiteral("Materials/test_renamed.mat"), QString(), &error, &createdPath2), "Duplicate second time resolves collision");
        CheckAsset(createdPath2 == QStringLiteral("Materials/test_renamed_Copy_1.mat"), "Second auto duplicate got _1 suffix");
        CheckAsset(ops.Exists(QStringLiteral("Materials/test_renamed_Copy_1.mat")), "Second auto duplicate exists");

        // Explicit destination duplicate
        QString customCreated;
        CheckAsset(ops.Duplicate(QStringLiteral("Materials/test_renamed.mat"), QStringLiteral("Materials/custom_dup.mat"), &error, &customCreated), "Explicit duplicate success");
        CheckAsset(customCreated == QStringLiteral("Materials/custom_dup.mat"), "Explicit duplicate path correct");
        CheckAsset(ops.Exists(QStringLiteral("Materials/custom_dup.mat")), "Explicit duplicate exists");

        // Duplicate directory
        QString dirCreated;
        CheckAsset(ops.Duplicate(QStringLiteral("Textures"), QStringLiteral("Textures_Backup"), &error, &dirCreated), "Duplicate directory success");
        CheckAsset(ops.IsFolder(QStringLiteral("Textures_Backup/Albedo")), "Duplicated nested folder exists");
    }

    // 7. Delete
    {
        QString error;
        CheckAsset(ops.Delete(QStringLiteral("Materials/custom_dup.mat"), &error), "Delete file success");
        CheckAsset(!ops.Exists(QStringLiteral("Materials/custom_dup.mat")), "Deleted file no longer exists");

        // Delete nonexistent fails
        CheckAsset(!ops.Delete(QStringLiteral("Materials/custom_dup.mat"), &error), "Delete nonexistent file fails");
        CheckAsset(!error.isEmpty(), "Error populated on nonexistent delete");

        // Delete directory recursively
        CheckAsset(ops.Delete(QStringLiteral("Textures_Backup"), &error), "Delete directory recursively success");
        CheckAsset(!ops.Exists(QStringLiteral("Textures_Backup")), "Deleted directory no longer exists");

        // Delete outside sandbox fails
        CheckAsset(!ops.Delete(QStringLiteral("../outside.txt"), &error), "Delete outside sandbox fails");
    }
}

} // namespace
