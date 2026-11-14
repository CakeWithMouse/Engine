#pragma once

#include "../../Core/AssetOps.h"
#include "../../Core/FileService.h"
#include "../../Core/FolderPaths.h"
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

inline void RunFolderPathValidationTests()
{
    using namespace EditorCore;
    QTemporaryDir tempDir;
    CheckAsset(tempDir.isValid(), "Temporary directory created for folder validation");
    FileService fs;
    fs.SetProjectRoot(tempDir.path());
    AssetOps ops(&fs);
    QString error;

    const QStringList invalidPaths = {
        QString(), QStringLiteral("../Rejected"), QStringLiteral("Inside/../Rejected"),
        QStringLiteral("Inside/.."), QStringLiteral("Inside\\..\\Rejected"),
        QStringLiteral("..\\Rejected"), QStringLiteral("/Rejected"),
        tempDir.path() + QStringLiteral("/Rejected"), QStringLiteral("C:/Rejected"),
        QStringLiteral("C:Rejected"), QStringLiteral("Inside/name:stream"),
        QStringLiteral("//server/share/Rejected"), QStringLiteral("\\\\server\\share\\Rejected"),
        QStringLiteral("\\Rejected")
    };
    for (const QString& path : invalidPaths)
    {
        error.clear();
        CheckAsset(!ValidateFolderRelativePath(path, &error), "Invalid relative folder path rejected");
        CheckAsset(!error.isEmpty(), "Relative folder validation reports an error");
        error.clear();
        CheckAsset(!ops.CreateFolder(path, &error), "CreateFolder rejects invalid path");
        CheckAsset(!error.isEmpty(), "Rejected CreateFolder reports an error");
    }
    CheckAsset(!ops.Exists(QStringLiteral("Rejected")), "Rejected paths do not create a folder");
    CheckAsset(ValidateFolderRelativePath(QStringLiteral(".")), "Project root is a valid relative parent");
    CheckAsset(ValidateFolderRelativePath(QStringLiteral("Inside\\Child")), "Backslash relative path is valid");
    CheckAsset(ops.CreateFolder(QStringLiteral("Inside\\Child")), "Valid backslash path creates nested folder");
    CheckAsset(ops.IsFolder(QStringLiteral("Inside/Child")), "Backslash path is normalized before creation");

    const auto entries = Logger::Instance().GetRecentEntries(100);
    bool logged = false;
    for (const LogEntry& entry : entries)
        logged = logged || (entry.level == LogLevel::Error && entry.message.startsWith(QStringLiteral("FolderPaths:")));
    CheckAsset(logged, "Rejected folder paths are logged");
}

inline void RunFolderNameValidationTests()
{
    using namespace EditorCore;
    const QStringList validNames = {
        QStringLiteral("New Folder"), QStringLiteral("Folder_2"), QStringLiteral("Folder.v2"),
        QStringLiteral(".assets"), QStringLiteral("COM10"), QStringLiteral("LPT10"), QStringLiteral("Console")
    };
    for (const QString& name : validNames)
    {
        QString error;
        CheckAsset(ValidateFolderName(name, &error), "Safe folder leaf name accepted");
        CheckAsset(error.isEmpty(), "Safe leaf name has no error");
    }
    const QStringList invalidNames = {
        QString(), QStringLiteral("."), QStringLiteral(".."), QStringLiteral("a/b"),
        QStringLiteral("a\\b"), QStringLiteral("a:b"), QStringLiteral("<name"),
        QStringLiteral("name>"), QStringLiteral("\"name"), QStringLiteral("name|"),
        QStringLiteral("name?"), QStringLiteral("name*"), QStringLiteral("name."),
        QStringLiteral("name "), QStringLiteral("CON"), QStringLiteral("con.txt"),
        QStringLiteral("PRN"), QStringLiteral("AUX"), QStringLiteral("NUL.bin"),
        QStringLiteral("COM1"), QStringLiteral("com9.more"), QStringLiteral("lpt1"),
        QStringLiteral("LPT9.ext"), QStringLiteral("COM\u00b9"), QStringLiteral("LPT\u00b2"),
        QStringLiteral("CONIN$"), QStringLiteral("CONOUT$"), QStringLiteral("bad\nname"),
        QString(QChar(0))
    };
    for (const QString& name : invalidNames)
    {
        QString error;
        CheckAsset(!ValidateFolderName(name, &error), "Unsafe folder leaf name rejected");
        CheckAsset(!error.isEmpty(), "Unsafe leaf name has an error");
    }
}

inline void RunExclusiveFolderTests()
{
    using namespace EditorCore;
    QTemporaryDir tempDir;
    CheckAsset(tempDir.isValid(), "Temporary directory created for exclusive folders");
    Logger logger;
    FileService fs(&logger);
    fs.SetProjectRoot(tempDir.path());
    AssetOps ops(&fs);
    QString error;

    CheckAsset(!ops.CreateFolder(QStringLiteral("Folders/Owned"), &error, true), "Exclusive creation requires existing parent");
    CheckAsset(!error.isEmpty(), "Missing exclusive parent reports error");
    CheckAsset(!ops.Exists(QStringLiteral("Folders")), "Exclusive creation does not make missing parent");
    CheckAsset(ops.CreateFolder(QStringLiteral("Folders")), "Create exclusive fixture parent");
    error.clear();
    CheckAsset(ops.CreateFolder(QStringLiteral("Folders/Owned"), &error, true), "First exclusive folder creation succeeds");
    CheckAsset(error.isEmpty(), "First exclusive folder creation has no error");
    CheckAsset(ops.IsFolder(QStringLiteral("Folders/Owned")), "Exclusively created folder exists");
    CheckAsset(ops.CreateFile(QStringLiteral("Folders/Owned/keep.txt"), "KEEP"), "Create owned folder sentinel");
    CheckAsset(!ops.CreateFolder(QStringLiteral("Folders/Owned"), &error, true), "Second exclusive folder creation fails");
    CheckAsset(!error.isEmpty(), "Exclusive folder collision reports error");
    CheckAsset(ops.Exists(QStringLiteral("Folders/Owned/keep.txt")), "Folder collision preserves existing contents");

    CheckAsset(ops.CreateFile(QStringLiteral("Folders/File"), "FILE"), "Create file collision fixture");
    error.clear();
    CheckAsset(!ops.CreateFolder(QStringLiteral("Folders/File"), &error, true), "Exclusive creation rejects existing file");
    CheckAsset(!error.isEmpty(), "Exclusive file collision reports error");
    QByteArray content;
    CheckAsset(fs.ReadFile(QStringLiteral("Folders/File"), content), "Collision file still readable");
    CheckAsset(content == QByteArray("FILE"), "Exclusive collision does not alter file");
    CheckAsset(!ops.CreateFolder(QStringLiteral("."), &error, true), "Exclusive project-root collision rejected");

    error.clear();
    CheckAsset(fs.CreateDirectoryPath(QStringLiteral("Direct/Nested"), &error), "Default FileService creation remains nested");
    CheckAsset(fs.CreateDirectoryPath(QStringLiteral("Direct/Nested"), &error), "Default FileService creation remains idempotent");
    const QString direct = QDir(tempDir.path()).filePath(QStringLiteral("Direct/Owned"));
    CheckAsset(fs.CreateDirectoryPath(direct, &error, true), "FileService exclusive absolute creation succeeds");
    CheckAsset(!fs.CreateDirectoryPath(direct, &error, true), "FileService exclusive collision fails");
    CheckAsset(!error.isEmpty(), "FileService exclusive collision reports error");
    const auto entries = logger.GetRecentEntries(100);
    bool logged = false;
    for (const LogEntry& entry : entries)
        logged = logged || (entry.level == LogLevel::Error && entry.message.contains(QStringLiteral("already exists")));
    CheckAsset(logged, "Exclusive collisions are logged through FileService logger");
}

inline void RunFolderResolutionTests()
{
    using namespace EditorCore;
    QTemporaryDir tempDir;
    CheckAsset(tempDir.isValid(), "Temporary directory created for canonical folder resolution");
    FileService fs;
    fs.SetProjectRoot(tempDir.path());
    AssetOps ops(&fs);
    QString error;

    CheckAsset(ops.CreateFolder(QStringLiteral("Inside")), "Create canonical resolution fixture");
    const QString inside = ResolveFolderPath(fs, QStringLiteral("Inside"), &error);
    CheckAsset(inside == fs.ResolveSandboxPath(QStringLiteral("Inside")), "Existing inside folder resolves to sandbox absolute path");
    CheckAsset(!QFileInfo(inside).canonicalFilePath().isEmpty(), "Existing inside folder has canonical path");
    CheckAsset(error.isEmpty(), "Valid canonical resolution has no error");
    CheckAsset(ResolveFolderPath(fs, QStringLiteral(".")) == fs.GetProjectRoot(), "Dot resolves to project root");
    const QString missing = QStringLiteral("Inside/Missing/Child");
    CheckAsset(ResolveFolderPath(fs, missing, &error) == fs.ResolveSandboxPath(missing), "Nonexistent child resolves through existing ancestor");
    CheckAsset(ResolveFolderPath(fs, QStringLiteral("Inside\\Missing\\Child")) == fs.ResolveSandboxPath(missing), "Canonical resolution normalizes backslashes");
    CheckAsset(!ops.Exists(missing), "Resolving nonexistent child does not create directories");
    CheckAsset(ops.CreateFolder(missing, &error), "Resolved nonexistent nested child can be created by default");
    CheckAsset(ops.IsFolder(missing), "Resolved nested child really exists");

    CheckAsset(ops.CreateFile(QStringLiteral("ParentFile"), "FILE"), "Create nondirectory ancestor fixture");
    error.clear();
    CheckAsset(ResolveFolderPath(fs, QStringLiteral("ParentFile/Child"), &error).isEmpty(), "File ancestor cannot resolve as folder parent");
    CheckAsset(!error.isEmpty(), "Nondirectory ancestor reports error");
    CheckAsset(!ops.CreateFolder(QStringLiteral("ParentFile/Child")), "CreateFolder rejects file ancestor");

    FileService missingRoot;
    missingRoot.SetProjectRoot(QDir(tempDir.path()).filePath(QStringLiteral("UnresolvedRoot")));
    error.clear();
    CheckAsset(ResolveFolderPath(missingRoot, QStringLiteral("Child"), &error).isEmpty(), "Nonexistent project root is rejected");
    CheckAsset(!error.isEmpty(), "Unresolved project root reports error");
    AssetOps missingOps(&missingRoot);
    CheckAsset(!missingOps.CreateFolder(QStringLiteral("Child")), "CreateFolder rejects unresolved root");
    CheckAsset(!ops.Exists(QStringLiteral("UnresolvedRoot")), "Rejected unresolved root is not created");
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
        CheckAsset(ops.CreateFile(QStringLiteral("Textures/Albedo/keep.txt"), "KEEP"), "Create nested folder sentinel");
        CheckAsset(ops.CreateFolder(QStringLiteral("Textures/Albedo"), &error), "Default folder creation is idempotent");
        CheckAsset(error.isEmpty(), "Repeated default folder creation has no error");
        CheckAsset(ops.Exists(QStringLiteral("Textures/Albedo/keep.txt")), "Default folder creation preserves contents");
        QByteArray preserved;
        CheckAsset(fs.ReadFile(QStringLiteral("Textures/Albedo/keep.txt"), preserved), "Preserved nested sentinel is readable");
        CheckAsset(preserved == QByteArray("KEEP"), "Default folder creation preserves file data");

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

    // 8. Companion sidecar handling on Rename, Duplicate, Delete
    {
        QString error;
        const QString modelPath = QStringLiteral("Models/Hero.glb");
        CheckAsset(ops.CreateFile(modelPath, "HERO_DATA", &error), "Create model file");
        CheckAsset(ops.CreateFile(modelPath + QStringLiteral(".importsettings.json"), "{\"scale\":1.0}", &error), "Create companion importsettings");
        CheckAsset(ops.CreateFile(modelPath + QStringLiteral(".source.json"), "{\"src\":\"Hero.fbx\"}", &error), "Create companion source");
        CheckAsset(ops.CreateFile(modelPath + QStringLiteral(".import.json"), "{\"hash\":123}", &error), "Create companion import");

        // Companion suffixes check
        const auto& suffixes = AssetOps::CompanionSuffixes();
        CheckAsset(suffixes.size() == 3, "AssetOps provides 3 companion suffixes");

        // Duplicate with companions
        QString dupCreated;
        const QString dupPath = QStringLiteral("Models/Hero_Copy.glb");
        CheckAsset(ops.Duplicate(modelPath, dupPath, &error, &dupCreated, true), "Duplicate model with companions");
        CheckAsset(ops.Exists(dupPath), "Duplicated model exists");
        CheckAsset(ops.Exists(dupPath + QStringLiteral(".importsettings.json")), "Duplicated companion importsettings exists");
        CheckAsset(ops.Exists(dupPath + QStringLiteral(".source.json")), "Duplicated companion source exists");
        CheckAsset(ops.Exists(dupPath + QStringLiteral(".import.json")), "Duplicated companion import exists");

        // Rename with companions
        const QString renPath = QStringLiteral("Models/Hero_Renamed.glb");
        CheckAsset(ops.Rename(dupPath, renPath, &error, true), "Rename model with companions");
        CheckAsset(!ops.Exists(dupPath), "Old duplicated model removed");
        CheckAsset(!ops.Exists(dupPath + QStringLiteral(".importsettings.json")), "Old duplicated companion importsettings removed");
        CheckAsset(ops.Exists(renPath), "Renamed model exists");
        CheckAsset(ops.Exists(renPath + QStringLiteral(".importsettings.json")), "Renamed companion importsettings exists");
        CheckAsset(ops.Exists(renPath + QStringLiteral(".source.json")), "Renamed companion source exists");
        CheckAsset(ops.Exists(renPath + QStringLiteral(".import.json")), "Renamed companion import exists");

        // Delete with companions
        CheckAsset(ops.Delete(renPath, &error, true), "Delete model with companions");
        CheckAsset(!ops.Exists(renPath), "Renamed model deleted");
        CheckAsset(!ops.Exists(renPath + QStringLiteral(".importsettings.json")), "Companion importsettings deleted");
        CheckAsset(!ops.Exists(renPath + QStringLiteral(".source.json")), "Companion source deleted");
        CheckAsset(!ops.Exists(renPath + QStringLiteral(".import.json")), "Companion import deleted");

        // Original model and its companions still intact
        CheckAsset(ops.Exists(modelPath), "Original model still exists");
        CheckAsset(ops.Exists(modelPath + QStringLiteral(".importsettings.json")), "Original companion still exists");
    }

    RunFolderPathValidationTests();
    RunFolderNameValidationTests();
    RunExclusiveFolderTests();
    RunFolderResolutionTests();
}

} // namespace
