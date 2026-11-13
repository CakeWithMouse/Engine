#pragma once

#include "UiTestHelper.h"
#include "ImportSettingsDialog.h"
#include "FileBrowser.h"
#include "ImportSettings.h"
#include "ModelImportSidecar.h"
#include "CommandRegistry.h"
#include "SettingsService.h"
#include "Logger.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace EditImportSettingsTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string("Check failed: ") + message);
    }
}

inline void Run()
{
    std::cout << "[Test] Running EditImportSettingsTests..." << std::endl;
    int checks = 0;

    // 1. Dialog default for useFileUnitScale is true
    {
        auto& svc = SettingsService::Instance();
        svc.Remove(QStringLiteral("import/model/useFileUnitScale"));

        ImportSettingsDialog dlg;
        auto* checkUnitScale = UiTestHelper::Require<QCheckBox>(&dlg, QStringLiteral("UseFileUnitScaleCheck"));
        Check(checkUnitScale->isChecked(), "ImportSettingsDialog default useFileUnitScale is true");
        ++checks;

        // Reset button sets useFileUnitScale back to true
        checkUnitScale->setChecked(false);
        Check(!checkUnitScale->isChecked(), "Manually unchecked useFileUnitScale is false");
        auto* btnReset = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("ResetDefaultsBtn"));
        btnReset->click();
        Check(checkUnitScale->isChecked(), "ResetDefaultsBtn resets useFileUnitScale to true in dialog");
        ++checks;

        Check(ImportSettingsDialog::DefaultSettings().model.useFileUnitScale, "ImportSettingsDialog::DefaultSettings useFileUnitScale is true");
        ++checks;

        // Prompt without initialSettings opens with useFileUnitScale == true without pressing reset
        bool promptDefaultUnitScale = false;
        ImportSettingsDialog::SetExecHookForTesting([&promptDefaultUnitScale](ImportSettingsDialog& dialog) {
            auto* cb = UiTestHelper::Require<QCheckBox>(&dialog, QStringLiteral("UseFileUnitScaleCheck"));
            promptDefaultUnitScale = cb->isChecked();
            UiTestHelper::ClickButton(&dialog, QStringLiteral("ImportCancelBtn"));
            return dialog.result();
        });
        ImportSettingsDialog::Prompt();
        ImportSettingsDialog::SetExecHookForTesting(nullptr);
        Check(promptDefaultUnitScale, "Prompt opens with useFileUnitScale true without reset");
        ++checks;
    }

    // 2. Schema and sidecar parser without field gives false
    {
        const EditorCore::ImportSettings schemaDefault;
        Check(!schemaDefault.model.useFileUnitScale, "Schema default model.useFileUnitScale is false");
        ++checks;

        const QByteArray noUnitScaleJson = R"({
            "version": 1,
            "kind": "model",
            "model": {
                "scale": 1.0,
                "mergeMeshes": true
            }
        })";

        EditorCore::ImportSettings parsed;
        QString err;
        const bool ok = EditorCore::ImportSettingsFile::Parse(noUnitScaleJson, parsed, &err);
        Check(ok, "Parse succeeds on JSON without useFileUnitScale");
        Check(!parsed.model.useFileUnitScale, "Missing useFileUnitScale parses as false");
        ++checks;
    }

    // 3. Edit mode in ImportSettingsDialog disables materials and textures
    {
        ImportSettingsDialog dlg;
        auto* checkMat = UiTestHelper::Require<QCheckBox>(&dlg, QStringLiteral("ImportMaterialsCheck"));
        auto* checkTex = UiTestHelper::Require<QCheckBox>(&dlg, QStringLiteral("ImportTexturesCheck"));

        Check(checkMat->isEnabled(), "ImportMaterialsCheck is enabled in import mode");
        Check(checkTex->isEnabled(), "ImportTexturesCheck is enabled in import mode");
        ++checks;

        dlg.SetEditMode(true);
        Check(!checkMat->isEnabled(), "ImportMaterialsCheck is disabled in edit mode");
        Check(!checkTex->isEnabled(), "ImportTexturesCheck is disabled in edit mode");
        Check(!checkMat->toolTip().isEmpty(), "Tooltip explains materials exist");
        Check(!checkTex->toolTip().isEmpty(), "Tooltip explains textures exist");
        ++checks;
    }

    // 4. EditImportSettings: reading current values, preserving unknown fields, writing sidecar
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "Temporary directory created");

        const QString modelPath = tempDir.filePath(QStringLiteral("Hero.glb"));
        const QString sidecarPath = tempDir.filePath(QStringLiteral("Hero.glb.importsettings.json"));

        // Create dummy model file
        {
            QFile modelFile(modelPath);
            Check(modelFile.open(QIODevice::WriteOnly), "Hero.glb created");
            modelFile.write("dummy-glb-geometry");
        }

        // Create sidecar with custom unknown field and non-default settings
        const QByteArray originalSidecarJson = R"({
            "version": 1,
            "kind": "model",
            "customPipelineField": "keepMeIntact",
            "model": {
                "scale": 0.05,
                "useFileUnitScale": false,
                "normals": "flat",
                "smoothingAngle": 45.0,
                "importMaterials": true,
                "importTextures": true,
                "flipUv": true,
                "mergeMeshes": false
            }
        })";
        {
            QFile sf(sidecarPath);
            Check(sf.open(QIODevice::WriteOnly), "Hero.glb.importsettings.json created");
            sf.write(originalSidecarJson);
        }

        // Load via ImportSettingsFile and verify current values loaded
        FileService fs(&Logger::Instance());
        fs.SetProjectRoot(tempDir.path());

        EditorCore::ImportSettings loaded;
        QString loadErr;
        const auto loadRes = EditorCore::ImportSettingsFile::Load(fs, QStringLiteral("Hero.glb.importsettings.json"), loaded, &loadErr);
        Check(loadRes == EditorCore::ImportSettingsFile::LoadResult::Success, "Load existing sidecar succeeds");
        Check(loadErr.isEmpty(), "Load existing sidecar has no error");
        Check(std::abs(loaded.model.scale - 0.05f) < 1e-5f, "Loaded scale is 0.05");
        Check(loaded.model.normals == EditorCore::ModelNormalsMode::Flat, "Loaded normals is Flat");
        Check(!loaded.model.mergeMeshes, "Loaded mergeMeshes is false");
        ++checks;

        // Verify ImportSettingsDialog populated with current values (not defaults)
        ImportSettingsDialog editDlg;
        editDlg.SetEditMode(true);
        editDlg.SetSettings(loaded);
        Check(std::abs(editDlg.Scale() - 0.05f) < 1e-5f, "Dialog scale matches loaded 0.05");
        Check(!editDlg.Settings().model.mergeMeshes, "Dialog mergeMeshes matches loaded false");
        Check(editDlg.Settings().model.normals == EditorCore::ModelNormalsMode::Flat, "Dialog normals matches Flat");
        ++checks;

        // Modify settings (e.g. scale to 0.02, mergeMeshes to true) and save
        loaded.model.scale = 0.02f;
        loaded.model.mergeMeshes = true;
        QString saveErr;
        Check(EditorCore::ImportSettingsFile::Save(fs, QStringLiteral("Hero.glb.importsettings.json"), loaded, &saveErr), "Save updated settings succeeds");

        // Verify sidecar on disk has new values and preserved unknown field
        QFile readBack(sidecarPath);
        Check(readBack.open(QIODevice::ReadOnly), "Re-read sidecar succeeds");
        const QByteArray savedBytes = readBack.readAll();
        const auto doc = QJsonDocument::fromJson(savedBytes);
        Check(doc.isObject(), "Saved sidecar is JSON object");
        const auto rootObj = doc.object();
        Check(rootObj.contains(QStringLiteral("customPipelineField")), "Unknown field customPipelineField preserved");
        Check(rootObj.value(QStringLiteral("customPipelineField")).toString() == QStringLiteral("keepMeIntact"), "Unknown field value intact");
        const auto modelObj = rootObj.value(QStringLiteral("model")).toObject();
        Check(std::abs(static_cast<float>(modelObj.value(QStringLiteral("scale")).toDouble()) - 0.02f) < 1e-5f, "Saved scale is 0.02");
        Check(modelObj.value(QStringLiteral("mergeMeshes")).toBool() == true, "Saved mergeMeshes is true");
        ++checks;
    }

    // 5. Missing sidecar: loads defaults and logs Info
    {
        QTemporaryDir tempDir;
        FileService fs(&Logger::Instance());
        fs.SetProjectRoot(tempDir.path());

        EditorCore::ImportSettings def;
        QString err;
        const auto res = EditorCore::ImportSettingsFile::Load(fs, QStringLiteral("NoSidecar.glb.importsettings.json"), def, &err);
        Check(res == EditorCore::ImportSettingsFile::LoadResult::NotFound, "Load missing sidecar returns NotFound");
        Check(err.contains(QStringLiteral("sidecar not found")), "Load missing sidecar reports informative message");
        Check(std::abs(def.model.scale - 1.0f) < 1e-5f, "Missing sidecar default scale is 1.0");
        Check(!def.model.useFileUnitScale, "Missing sidecar default useFileUnitScale is false per schema");
        ++checks;
    }

    // 6. Broken sidecar: logs Error and loads defaults
    {
        QTemporaryDir tempDir;
        FileService fs(&Logger::Instance());
        fs.SetProjectRoot(tempDir.path());

        const QString brokenSidecar = tempDir.filePath(QStringLiteral("Broken.glb.importsettings.json"));
        {
            QFile f(brokenSidecar);
            f.open(QIODevice::WriteOnly);
            f.write("{ invalid json syntax ... ");
        }
        EditorCore::ImportSettings def;
        QString err;
        const auto res = EditorCore::ImportSettingsFile::Load(fs, QStringLiteral("Broken.glb.importsettings.json"), def, &err);
        Check(res == EditorCore::ImportSettingsFile::LoadResult::Error, "Load broken sidecar reports Error");
        Check(!err.isEmpty(), "Load broken sidecar reports error message");
        Check(std::abs(def.model.scale - 1.0f) < 1e-5f, "Broken sidecar defaults scale to 1.0");
        ++checks;
    }

    // 7. Cancel behavior: sidecar is not modified
    {
        QTemporaryDir tempDir;
        const QString sidecar = tempDir.filePath(QStringLiteral("CancelTest.glb.importsettings.json"));
        const QByteArray original = R"({"version":1,"kind":"model","model":{"scale":0.5}})";
        {
            QFile f(sidecar);
            f.open(QIODevice::WriteOnly);
            f.write(original);
        }

        ImportSettingsDialog::SetExecHookForTesting([](ImportSettingsDialog&) {
            return QDialog::Rejected; // Simulate user clicking Cancel
        });

        // Simulate cancel flow: dialog rejected -> no save
        ImportSettingsDialog dlg;
        dlg.SetEditMode(true);
        dlg.SetScale(0.99f);
        const int res = dlg.exec();
        Check(res == QDialog::Rejected, "Dialog exec returned Rejected on Cancel");

        // Verify sidecar on disk is completely unchanged
        QFile f(sidecar);
        f.open(QIODevice::ReadOnly);
        Check(f.readAll() == original, "Sidecar content unmodified after Cancel");
        ImportSettingsDialog::SetExecHookForTesting(nullptr);
        ++checks;
    }

    // 8. Smoothing angle and label muted when Normals != Smooth
    {
        SettingsService::Instance().Remove(QStringLiteral("import/model/normals"));
        ImportSettingsDialog dlg;
        auto* comboNormals = UiTestHelper::Require<QComboBox>(&dlg, QStringLiteral("NormalsCombo"));
        auto* spinAngle = UiTestHelper::Require<QDoubleSpinBox>(&dlg, QStringLiteral("SmoothingAngleSpin"));
        auto* labelAngle = UiTestHelper::Require<QLabel>(&dlg, QStringLiteral("SmoothingAngleLabel"));

        // Normals is File -> spin and label must be disabled, tooltip present
        UiTestHelper::SelectComboBoxData(&dlg, QStringLiteral("NormalsCombo"), static_cast<int>(EditorCore::ModelNormalsMode::File));
        Check(!spinAngle->isEnabled(), "SmoothingAngle spin is disabled when normals != Smooth");
        Check(!labelAngle->isEnabled(), "SmoothingAngle label is disabled when normals != Smooth");
        Check(!spinAngle->toolTip().isEmpty(), "SmoothingAngle spin has tooltip when disabled");
        Check(spinAngle->toolTip().contains(QStringLiteral("нормал")) || spinAngle->toolTip().contains(QStringLiteral("Smooth")),
              "SmoothingAngle spin tooltip explains dependency");
        Check(labelAngle->toolTip() == spinAngle->toolTip(), "SmoothingAngle label tooltip matches spin tooltip");
        ++checks;

        // Switch to Flat -> still disabled
        const int flatIdx = comboNormals->findData(static_cast<int>(EditorCore::ModelNormalsMode::Flat));
        comboNormals->setCurrentIndex(flatIdx);
        Check(!spinAngle->isEnabled(), "SmoothingAngle spin is disabled when normals is Flat");
        Check(!labelAngle->isEnabled(), "SmoothingAngle label is disabled when normals is Flat");
        ++checks;

        // Switch to Smooth -> enabled, tooltip is smoothing description
        const int smoothIdx = comboNormals->findData(static_cast<int>(EditorCore::ModelNormalsMode::Smooth));
        comboNormals->setCurrentIndex(smoothIdx);
        Check(spinAngle->isEnabled(), "SmoothingAngle spin is enabled when normals is Smooth");
        Check(labelAngle->isEnabled(), "SmoothingAngle label is enabled when normals is Smooth");
        Check(!spinAngle->toolTip().contains(QStringLiteral("Доступно только")), "SmoothingAngle spin tooltip reset when enabled");
        ++checks;
    }

    // 9. ProjectFilesFilter hides companion service files (.import.json, .importsettings.json, .source.json)
    {
        Check(ProjectFilesFilter::IsHiddenServiceFile(QStringLiteral("Hero.fbx.importsettings.json")), "Hides .importsettings.json");
        Check(ProjectFilesFilter::IsHiddenServiceFile(QStringLiteral("Hero.fbx.source.json")), "Hides .source.json");
        Check(ProjectFilesFilter::IsHiddenServiceFile(QStringLiteral("Hero.fbx.import.json")), "Hides .import.json");
        Check(!ProjectFilesFilter::IsHiddenServiceFile(QStringLiteral("Hero.fbx")), "Shows model .fbx");
        Check(!ProjectFilesFilter::IsHiddenServiceFile(QStringLiteral("Hero.glb")), "Shows model .glb");
        Check(!ProjectFilesFilter::IsHiddenServiceFile(QStringLiteral("Hero.obj")), "Shows model .obj");
        Check(!ProjectFilesFilter::IsHiddenServiceFile(QStringLiteral("Main.scene.json")), "Shows scene .scene.json");
        ++checks;
    }

    std::cout << "EditImportSettingsTests: " << checks << " checks passed." << std::endl;
}

} // namespace EditImportSettingsTests
