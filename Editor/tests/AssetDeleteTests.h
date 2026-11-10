#pragma once

#include "CommandRegistry.h"
#include "FileService.h"
#include "AssetOps.h"
#include "Logger.h"
#include "AssetDeletePrompt.h"
#include "GeometryCard.h"
#include "MaterialCard.h"

#include <QTemporaryDir>
#include <QFile>
#include <QApplication>
#include <QKeyEvent>
#include <QTimer>
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

        // Bypass tests: case-insensitivity on Windows, trailing slashes, dots and dot-dots
        Check(!fs.DeleteDirectory(QStringLiteral("content"), &err), "content (lowercase) is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("CONTENT"), &err), "CONTENT (uppercase) is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("source"), &err), "source (lowercase) is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("SOURCE"), &err), "SOURCE (uppercase) is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("Content/"), &err), "Content/ (trailing slash) is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("Content\\"), &err), "Content\\ (trailing backslash) is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("Content/."), &err), "Content/. is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("Content/../Content"), &err), "Content/../Content is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("Source/."), &err), "Source/. is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("Source/../Source"), &err), "Source/../Source is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("."), &err), ". (project root) is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("./"), &err), "./ (project root) is protected");
        Check(!fs.DeleteDirectory(QStringLiteral("Content/.."), &err), "Content/.. (project root) is protected");

        // Instance test without manual SetContentRoot
        {
            FileService fsAuto(&logger);
            fsAuto.SetProjectRoot(projectDir);
            Check(fsAuto.GetContentRoot() == contentDir, "Default content root initialized to <projectDir>/Content");
            Check(!fsAuto.DeleteDirectory(QStringLiteral("Content"), &err), "Cannot delete Content on auto-initialized instance");
            Check(!fsAuto.DeleteDirectory(QStringLiteral("content"), &err), "Cannot delete lowercase content on auto-initialized instance");
            Check(!fsAuto.DeleteDirectory(QStringLiteral("Source"), &err), "Cannot delete Source on auto-initialized instance");
            Check(QDir(contentDir).exists(), "Content directory preserved");
        }

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

        // Subfolder with different case inside Content (CONTENT and SOURCE)
        const QString subUpperContent = QDir(contentDir).filePath(QStringLiteral("CONTENT"));
        QDir().mkpath(subUpperContent);
        Check(!fs.IsProtectedDirectory(subUpperContent), "Subfolder Content/CONTENT is NOT protected");
        Check(fs.DeleteDirectory(QStringLiteral("Content/CONTENT"), &err), "Deleting Content/CONTENT succeeds");
        Check(!QDir(subUpperContent).exists(), "Content/CONTENT removed");

        const QString subUpperSource = QDir(contentDir).filePath(QStringLiteral("SOURCE"));
        QDir().mkpath(subUpperSource);
        Check(!fs.IsProtectedDirectory(subUpperSource), "Subfolder Content/SOURCE is NOT protected");
        Check(fs.DeleteDirectory(QStringLiteral("Content/SOURCE"), &err), "Deleting Content/SOURCE succeeds");
        Check(!QDir(subUpperSource).exists(), "Content/SOURCE removed");
        ++checks;
    }

    // 7. Delete confirmation dialog: Enter defaults to Yes, Escape to No
    {
        // 7.1 Verify default and escape buttons configuration
        auto* box = EditorUI::CreateDeleteConfirmationBox(nullptr, QMessageBox::Question, QStringLiteral("Title"), QStringLiteral("Prompt"));
        Check(box->defaultButton() != nullptr, "Default button is configured");
        Check(box->defaultButton() == box->button(QMessageBox::Yes), "Default button is Yes");
        Check(box->escapeButton() == box->button(QMessageBox::No), "Escape button is No");

        // 7.2 Return key triggers default button (Yes)
        QTimer::singleShot(20, [box] {
            QKeyEvent ev(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(box, &ev);
        });
        const int resReturn = box->exec();
        Check(resReturn == QMessageBox::Yes, "Key_Return triggers Yes");
        delete box;
        ++checks;

        // 7.3 Enter keypad key also triggers default button (Yes)
        auto* boxEnter = EditorUI::CreateDeleteConfirmationBox(nullptr, QMessageBox::Warning, QStringLiteral("Title"), QStringLiteral("Prompt"));
        QTimer::singleShot(20, [boxEnter] {
            QKeyEvent ev(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
            QApplication::sendEvent(boxEnter, &ev);
        });
        const int resEnter = boxEnter->exec();
        Check(resEnter == QMessageBox::Yes, "Key_Enter triggers Yes");
        delete boxEnter;
        ++checks;

        // 7.4 Escape key triggers escape button (No)
        auto* boxEsc = EditorUI::CreateDeleteConfirmationBox(nullptr, QMessageBox::Warning, QStringLiteral("Title"), QStringLiteral("Prompt"));
        QTimer::singleShot(20, [boxEsc] {
            QKeyEvent ev(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(boxEsc, &ev);
        });
        const int resEsc = boxEsc->exec();
        Check(resEsc == QMessageBox::No, "Key_Escape triggers No");
        delete boxEsc;
        ++checks;
    }

    // 8. Missing asset indicator in Inspector (GeometryCard & MaterialCard with cache)
    {
        QTemporaryDir tempContent;
        const QString contentRoot = tempContent.path();
        const QString validModelRel = QStringLiteral("Models/Valid.fbx");
        const QString validModelPath = QDir(contentRoot).filePath(validModelRel);
        QDir().mkpath(QFileInfo(validModelPath).absolutePath());
        {
            QFile f(validModelPath);
            f.open(QIODevice::WriteOnly);
            f.write("FBX");
        }

        const QString validMatRel = QStringLiteral("Materials/Valid.mat");
        const QString validMatPath = QDir(contentRoot).filePath(validMatRel);
        QDir().mkpath(QFileInfo(validMatPath).absolutePath());
        {
            QFile f(validMatPath);
            f.open(QIODevice::WriteOnly);
            f.write("MAT");
        }

        // 8.1 GeometryCard: missing model shows color and tooltip
        {
            GeometryCard geomCard;
            geomCard.SetData(QStringLiteral("/Game/Models/Missing.fbx"), true, contentRoot, true);
            auto* edit = geomCard.findChild<QLineEdit*>(QStringLiteral("ModelAsset"));
            Check(edit != nullptr, "ModelAsset edit found");
            Check(edit->styleSheet().contains(QStringLiteral("#E06C75")), "Missing model colored in red (#E06C75)");
            Check(!edit->toolTip().isEmpty(), "Missing model has tooltip");

            // Valid model clears color and tooltip
            geomCard.SetData(QStringLiteral("/Game/") + validModelRel, true, contentRoot, true);
            Check(edit->styleSheet().isEmpty(), "Valid model styleSheet is clear");
            Check(edit->toolTip().isEmpty(), "Valid model tooltip is empty");

            // Cache check: without forceCheck and without path change, no re-check
            geomCard.SetData(QStringLiteral("/Game/") + validModelRel, true, contentRoot, false);
            Check(edit->styleSheet().isEmpty(), "Cached valid model remains clear");
            ++checks;
        }

        // 8.2 MaterialCard: missing material shows color, tooltip, and disables Edit button
        {
            MaterialCard matCard;
            matCard.SetData(QStringLiteral("/Game/Materials/Missing.mat"), false, contentRoot, true);
            auto* edit = matCard.findChild<QLineEdit*>(QStringLiteral("MaterialAsset"));
            auto* btn = matCard.findChild<QPushButton*>(QStringLiteral("EditMaterial"));
            Check(edit != nullptr && btn != nullptr, "MaterialCard widgets found");
            Check(edit->styleSheet().contains(QStringLiteral("#E06C75")), "Missing material colored in red (#E06C75)");
            Check(!edit->toolTip().isEmpty(), "Missing material has tooltip");
            Check(!btn->isEnabled(), "Edit button is DISABLED for missing material");
            Check(!btn->toolTip().isEmpty(), "Edit button explains missing material file");

            // Valid material enables button and clears color
            matCard.SetData(QStringLiteral("/Game/") + validMatRel, false, contentRoot, true);
            Check(edit->styleSheet().isEmpty(), "Valid material styleSheet is clear");
            Check(edit->toolTip().isEmpty(), "Valid material tooltip is empty");
            Check(btn->isEnabled(), "Edit button is ENABLED for valid material");
            ++checks;
        }
    }

    std::cout << "AssetDeleteTests: " << checks << " checks passed." << std::endl;
}

} // namespace AssetDeleteTests

