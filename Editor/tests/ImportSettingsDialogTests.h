#pragma once

#include "ImportSettingsDialog.h"
#include "ImportSettings.h"
#include "CollapsibleCard.h"
#include "SettingsService.h"
#include "UiTestHelper.h"
#include "CommandRegistry.h"
#include "IEditorHost.h"
#include "EditorStyles.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <iostream>
#include <cmath>
#include <vector>

namespace ImportSettingsDialogTests
{

struct ScopedImportDialogHook
{
    explicit ScopedImportDialogHook(ImportSettingsDialog::ExecHook hook)
    {
        ImportSettingsDialog::SetExecHookForTesting(std::move(hook));
    }
    ~ScopedImportDialogHook()
    {
        ImportSettingsDialog::SetExecHookForTesting(nullptr);
    }
    ScopedImportDialogHook(const ScopedImportDialogHook&) = delete;
    ScopedImportDialogHook& operator=(const ScopedImportDialogHook&) = delete;
};

inline void Check(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string("ImportSettingsDialog test failed: ") + message);
    }
}

inline void Run()
{
    std::cout << "[Test] Running ImportSettingsDialogTests..." << std::endl;
    int checks = 0;

    // Reset settings service state for clean testing
    auto& settingsSvc = SettingsService::Instance();
    settingsSvc.Remove(QStringLiteral("import/model/scale"));
    settingsSvc.Remove(QStringLiteral("import/model/useFileUnitScale"));
    settingsSvc.Remove(QStringLiteral("import/model/importMaterials"));
    settingsSvc.Remove(QStringLiteral("import/model/importTextures"));
    settingsSvc.Remove(QStringLiteral("import/model/normals"));
    settingsSvc.Remove(QStringLiteral("import/model/smoothingAngle"));
    settingsSvc.Remove(QStringLiteral("import/model/flipUv"));
    settingsSvc.Remove(QStringLiteral("import/model/mergeMeshes"));

    // 1. Creation and presence of widgets by objectName
    ImportSettingsDialog dlg;
    auto* spin = UiTestHelper::Require<QDoubleSpinBox>(&dlg, QStringLiteral("ImportScaleSpin"));
    auto* btnPreset1 = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("Preset1"));
    auto* btnPreset01 = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("Preset01"));
    auto* btnPreset001 = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("Preset001"));
    auto* checkUnitScale = UiTestHelper::Require<QCheckBox>(&dlg, QStringLiteral("UseFileUnitScaleCheck"));
    auto* checkMaterials = UiTestHelper::Require<QCheckBox>(&dlg, QStringLiteral("ImportMaterialsCheck"));
    auto* checkTextures = UiTestHelper::Require<QCheckBox>(&dlg, QStringLiteral("ImportTexturesCheck"));
    auto* advCardFrame = UiTestHelper::Require<QFrame>(&dlg, QStringLiteral("AdvancedSettingsCard"));
    auto* advCard = static_cast<CollapsibleCard*>(advCardFrame);
    auto* comboNormals = UiTestHelper::Require<QComboBox>(&dlg, QStringLiteral("NormalsCombo"));
    auto* spinSmoothing = UiTestHelper::Require<QDoubleSpinBox>(&dlg, QStringLiteral("SmoothingAngleSpin"));
    auto* checkFlipUv = UiTestHelper::Require<QCheckBox>(&dlg, QStringLiteral("FlipUvCheck"));
    auto* checkMergeMeshes = UiTestHelper::Require<QCheckBox>(&dlg, QStringLiteral("MergeMeshesCheck"));
    auto* btnReset = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("ResetDefaultsBtn"));
    auto* btnOk = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("ImportOkBtn"));
    auto* btnCancel = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("ImportCancelBtn"));

    Check(spin != nullptr, "ImportScaleSpin exists");
    Check(btnPreset1 != nullptr, "Preset1 button exists");
    Check(btnPreset01 != nullptr, "Preset01 button exists");
    Check(btnPreset001 != nullptr, "Preset001 button exists");
    Check(checkUnitScale != nullptr, "UseFileUnitScaleCheck exists");
    Check(checkMaterials != nullptr, "ImportMaterialsCheck exists");
    Check(checkTextures != nullptr, "ImportTexturesCheck exists");
    Check(advCard != nullptr, "AdvancedSettingsCard exists");
    Check(comboNormals != nullptr, "NormalsCombo exists");
    Check(spinSmoothing != nullptr, "SmoothingAngleSpin exists");
    Check(checkFlipUv != nullptr, "FlipUvCheck exists");
    Check(checkMergeMeshes != nullptr, "MergeMeshesCheck exists");
    Check(btnReset != nullptr, "ResetDefaultsBtn exists");
    Check(btnOk != nullptr, "ImportOkBtn exists");
    Check(btnCancel != nullptr, "ImportCancelBtn exists");
    ++checks;

    // 2. Default values in dialog
    Check(std::abs(dlg.Scale() - 1.0f) < 1e-5f, "Default scale is 1.0");
    Check(checkUnitScale->isChecked(), "Default useFileUnitScale in dialog is true");
    Check(checkMaterials->isChecked(), "Default importMaterials is true");
    Check(checkTextures->isChecked(), "Default importTextures is true");
    Check(comboNormals->currentData().toInt() == static_cast<int>(EditorCore::ModelNormalsMode::File), "Default normals is file");
    Check(std::abs(spinSmoothing->value() - 60.0) < 1e-5, "Default smoothingAngle is 60.0");
    Check(!checkFlipUv->isChecked(), "Default flipUv is false");
    Check(checkMergeMeshes->isChecked(), "Default mergeMeshes is true");
    Check(!advCard->IsExpanded(), "Advanced settings card is collapsed by default");
    ++checks;

    // 3. Preset buttons set exact scale values
    UiTestHelper::ClickButton(&dlg, QStringLiteral("Preset01"));
    Check(std::abs(dlg.Scale() - 0.1f) < 1e-5f, "Preset 0.1 sets scale to 0.1");
    Check(std::abs(spin->value() - 0.1) < 1e-5, "SpinBox reflects 0.1");
    ++checks;

    UiTestHelper::ClickButton(&dlg, QStringLiteral("Preset001"));
    Check(std::abs(dlg.Scale() - 0.01f) < 1e-5f, "Preset 0.01 sets scale to 0.01");
    Check(std::abs(spin->value() - 0.01) < 1e-5, "SpinBox reflects 0.01");
    ++checks;

    UiTestHelper::ClickButton(&dlg, QStringLiteral("Preset1"));
    Check(std::abs(dlg.Scale() - 1.0f) < 1e-5f, "Preset 1 sets scale to 1.0");
    Check(std::abs(spin->value() - 1.0) < 1e-5, "SpinBox reflects 1.0");
    ++checks;

    // 4. Decimals precision: enough for 0.0254 (inches to meters)
    UiTestHelper::SetSpinBox(&dlg, QStringLiteral("ImportScaleSpin"), 0.0254);
    Check(std::abs(dlg.Scale() - 0.0254f) < 1e-5f, "SpinBox supports decimals for 0.0254");
    ++checks;

    // 5. Value out of range cannot be entered (clamped by SpinBox)
    UiTestHelper::SetSpinBox(&dlg, QStringLiteral("ImportScaleSpin"), 1e-6);
    Check(std::abs(dlg.Scale() - EditorCore::ImportSettingsFile::MinScale) < 1e-5f, "Below MinScale clamped to MinScale");
    ++checks;

    UiTestHelper::SetSpinBox(&dlg, QStringLiteral("ImportScaleSpin"), -10.0);
    Check(std::abs(dlg.Scale() - EditorCore::ImportSettingsFile::MinScale) < 1e-5f, "Negative scale clamped to MinScale");
    ++checks;

    UiTestHelper::SetSpinBox(&dlg, QStringLiteral("ImportScaleSpin"), 999999.0);
    Check(std::abs(dlg.Scale() - EditorCore::ImportSettingsFile::MaxScale) < 1e-3f, "Above MaxScale clamped to MaxScale");
    ++checks;

    // 6. Dependencies: ImportTextures disabled when ImportMaterials is unchecked
    Check(checkTextures->isEnabled(), "ImportTextures enabled initially when ImportMaterials is true");
    checkMaterials->setChecked(false);
    Check(!checkTextures->isEnabled(), "ImportTextures disabled when ImportMaterials is unchecked");
    checkMaterials->setChecked(true);
    Check(checkTextures->isEnabled(), "ImportTextures re-enabled when ImportMaterials is checked");
    ++checks;

    // 7. Dependencies: SmoothingAngle enabled only when Normals == Smooth
    Check(!spinSmoothing->isEnabled(), "SmoothingAngle disabled when normals is File");
    UiTestHelper::SelectComboBoxData(&dlg, QStringLiteral("NormalsCombo"), static_cast<int>(EditorCore::ModelNormalsMode::Flat));
    Check(!spinSmoothing->isEnabled(), "SmoothingAngle disabled when normals is Flat");
    UiTestHelper::SelectComboBoxData(&dlg, QStringLiteral("NormalsCombo"), static_cast<int>(EditorCore::ModelNormalsMode::Smooth));
    Check(spinSmoothing->isEnabled(), "SmoothingAngle enabled when normals is Smooth");
    UiTestHelper::SelectComboBoxData(&dlg, QStringLiteral("NormalsCombo"), static_cast<int>(EditorCore::ModelNormalsMode::File));
    Check(!spinSmoothing->isEnabled(), "SmoothingAngle disabled again when normals is File");
    ++checks;

    // 7b. EditorStyles: disabled stylesheet rules exist for spinbox and labels in dark & light
    for (const auto mode : {ThemeMode::Dark, ThemeMode::Light})
    {
        const QString qss = EditorStyles::BuildStyleSheet(mode, QColor(0, 122, 204), QStringLiteral("up"), QStringLiteral("down"), QStringLiteral("right"));
        Check(qss.contains(QStringLiteral("QDoubleSpinBox:disabled")), "StyleSheet contains QDoubleSpinBox:disabled");
        Check(qss.contains(QStringLiteral("QLabel:disabled")), "StyleSheet contains QLabel:disabled");
        Check(qss.contains(QStringLiteral("QLabel#CardFieldLabel:disabled")), "StyleSheet contains QLabel#CardFieldLabel:disabled");
        Check(qss.contains(QStringLiteral("QFrame#CollapsibleCard QDoubleSpinBox:disabled")), "StyleSheet contains CollapsibleCard QDoubleSpinBox:disabled");
        ++checks;
    }

    // 8. Reset to defaults restores all 8 parameters to schema defaults
    dlg.SetScale(0.05f);
    checkUnitScale->setChecked(true);
    checkMaterials->setChecked(false);
    UiTestHelper::SelectComboBoxData(&dlg, QStringLiteral("NormalsCombo"), static_cast<int>(EditorCore::ModelNormalsMode::Smooth));
    spinSmoothing->setValue(45.0);
    checkFlipUv->setChecked(true);
    checkMergeMeshes->setChecked(false);

    UiTestHelper::ClickButton(&dlg, QStringLiteral("ResetDefaultsBtn"));

    Check(std::abs(dlg.Scale() - 1.0f) < 1e-5f, "Reset restores scale 1.0");
    Check(checkUnitScale->isChecked(), "Reset restores useFileUnitScale true");
    Check(checkMaterials->isChecked(), "Reset restores importMaterials true");
    Check(checkTextures->isEnabled(), "Reset restores importTextures enabled");
    Check(checkTextures->isChecked(), "Reset restores importTextures true");
    Check(comboNormals->currentData().toInt() == static_cast<int>(EditorCore::ModelNormalsMode::File), "Reset restores normals file");
    Check(!spinSmoothing->isEnabled(), "Reset disables smoothingAngle for file normals");
    Check(std::abs(spinSmoothing->value() - 60.0) < 1e-5, "Reset restores smoothingAngle 60.0");
    Check(!checkFlipUv->isChecked(), "Reset restores flipUv false");
    Check(checkMergeMeshes->isChecked(), "Reset restores mergeMeshes true");
    ++checks;

    // 9. Cancel button rejects dialog
    UiTestHelper::ClickButton(&dlg, QStringLiteral("ImportCancelBtn"));
    Check(dlg.result() == QDialog::Rejected, "Cancel button sets Rejected result");
    ++checks;

    // 10. OK button accepts dialog, preserves all 8 values, produces valid JSON
    dlg.SetScale(0.05f);
    checkUnitScale->setChecked(true);
    checkMaterials->setChecked(true);
    checkTextures->setChecked(false);
    UiTestHelper::SelectComboBoxData(&dlg, QStringLiteral("NormalsCombo"), static_cast<int>(EditorCore::ModelNormalsMode::Smooth));
    spinSmoothing->setValue(45.0);
    checkFlipUv->setChecked(true);
    checkMergeMeshes->setChecked(false);

    UiTestHelper::ClickButton(&dlg, QStringLiteral("ImportOkBtn"));
    Check(dlg.result() == QDialog::Accepted, "Ok button sets Accepted result");

    const auto acceptedSettings = dlg.Settings();
    Check(std::abs(acceptedSettings.model.scale - 0.05f) < 1e-5f, "Accepted scale matches 0.05");
    Check(acceptedSettings.model.useFileUnitScale == true, "Accepted useFileUnitScale matches true");
    Check(acceptedSettings.model.importMaterials == true, "Accepted importMaterials matches true");
    Check(acceptedSettings.model.importTextures == false, "Accepted importTextures matches false");
    Check(acceptedSettings.model.normals == EditorCore::ModelNormalsMode::Smooth, "Accepted normals matches smooth");
    Check(std::abs(acceptedSettings.model.smoothingAngle - 45.0f) < 1e-5f, "Accepted smoothingAngle matches 45.0");
    Check(acceptedSettings.model.flipUv == true, "Accepted flipUv matches true");
    Check(acceptedSettings.model.mergeMeshes == false, "Accepted mergeMeshes matches false");

    const QByteArray jsonBytes = EditorCore::ImportSettingsFile::Serialize(acceptedSettings);
    const QString jsonStr = QString::fromUtf8(jsonBytes);
    Check(jsonStr.contains(QStringLiteral("\"scale\": 0.05")), "JSON contains scale 0.05");
    Check(jsonStr.contains(QStringLiteral("\"useFileUnitScale\": true")), "JSON contains useFileUnitScale true");
    Check(jsonStr.contains(QStringLiteral("\"normals\": \"smooth\"")), "JSON contains normals smooth");
    Check(jsonStr.contains(QStringLiteral("\"smoothingAngle\": 45")), "JSON contains smoothingAngle 45");
    Check(jsonStr.contains(QStringLiteral("\"importMaterials\": true")), "JSON contains importMaterials true");
    Check(jsonStr.contains(QStringLiteral("\"importTextures\": false")), "JSON contains importTextures false");
    Check(jsonStr.contains(QStringLiteral("\"flipUv\": true")), "JSON contains flipUv true");
    Check(jsonStr.contains(QStringLiteral("\"mergeMeshes\": false")), "JSON contains mergeMeshes false");
    ++checks;

    // 11. SettingsService persistence: reopening dialog restores last accepted values
    {
        ImportSettingsDialog dlg2;
        Check(std::abs(dlg2.Scale() - 0.05f) < 1e-5f, "Restored scale from SettingsService is 0.05");
        auto* u2 = UiTestHelper::Require<QCheckBox>(&dlg2, QStringLiteral("UseFileUnitScaleCheck"));
        Check(u2->isChecked(), "Restored useFileUnitScale from SettingsService is true");
        auto* t2 = UiTestHelper::Require<QCheckBox>(&dlg2, QStringLiteral("ImportTexturesCheck"));
        Check(!t2->isChecked(), "Restored importTextures from SettingsService is false");
        auto* n2 = UiTestHelper::Require<QComboBox>(&dlg2, QStringLiteral("NormalsCombo"));
        Check(n2->currentData().toInt() == static_cast<int>(EditorCore::ModelNormalsMode::Smooth), "Restored normals is Smooth");
        auto* m2 = UiTestHelper::Require<QCheckBox>(&dlg2, QStringLiteral("MergeMeshesCheck"));
        Check(!m2->isChecked(), "Restored mergeMeshes is false");
        ++checks;
    }

    // 12. Cancel via ResolveModelImportDecision does not post any command
    {
        std::vector<EditorCommand> postedCommands;

        ScopedImportDialogHook hook([](ImportSettingsDialog& dialog) {
            UiTestHelper::ClickButton(&dialog, QStringLiteral("ImportCancelBtn"));
            return dialog.result();
        });

        auto importHandler = [&postedCommands](const QVariantMap& params, bool hasSource) -> bool {
            const auto decision = ResolveModelImportDecision(params, hasSource, nullptr);
            if (!decision.proceed)
            {
                return false;
            }
            EditorCommand c;
            c.type = EditorCommand::ImportModel;
            c.importScale = decision.scale;
            c.importOptionsJson = QString::fromUtf8(EditorCore::ImportSettingsFile::Serialize(decision.settings));
            postedCommands.push_back(c);
            return true;
        };

        QVariantMap params;
        params[QStringLiteral("showDialog")] = true;

        const bool result = importHandler(params, true);
        Check(!result, "Handler returns false on Cancel");
        Check(postedCommands.empty(), "Cancel did not create or post any command");
        ++checks;
    }

    // 13. Ok via ResolveModelImportDecision posts command with options JSON
    {
        std::vector<EditorCommand> postedCommands;

        ScopedImportDialogHook hook([](ImportSettingsDialog& dialog) {
            UiTestHelper::ClickButton(&dialog, QStringLiteral("Preset001"));
            UiTestHelper::Require<QCheckBox>(&dialog, QStringLiteral("UseFileUnitScaleCheck"))->setChecked(true);
            UiTestHelper::Require<QCheckBox>(&dialog, QStringLiteral("MergeMeshesCheck"))->setChecked(false);
            UiTestHelper::ClickButton(&dialog, QStringLiteral("ImportOkBtn"));
            return dialog.result();
        });

        auto importHandler = [&postedCommands](const QVariantMap& params, bool hasSource) -> bool {
            const auto decision = ResolveModelImportDecision(params, hasSource, nullptr);
            if (!decision.proceed)
            {
                return false;
            }
            EditorCommand c;
            c.type = EditorCommand::ImportModel;
            c.importScale = decision.scale;
            c.importOptionsJson = QString::fromUtf8(EditorCore::ImportSettingsFile::Serialize(decision.settings));
            postedCommands.push_back(c);
            return true;
        };

        QVariantMap params;
        params[QStringLiteral("showDialog")] = true;

        const bool result = importHandler(params, true);
        Check(result, "Handler returns true on Ok");
        Check(postedCommands.size() == 1, "One command was posted");
        Check(std::abs(postedCommands[0].importScale - 0.01f) < 1e-5f, "Command has importScale 0.01");
        Check(postedCommands[0].importOptionsJson.contains(QStringLiteral("\"useFileUnitScale\": true")), "Command optionsJson has useFileUnitScale true");
        Check(postedCommands[0].importOptionsJson.contains(QStringLiteral("\"mergeMeshes\": false")), "Command optionsJson has mergeMeshes false");
        ++checks;
    }

    // 18. Dialog resize behavior on expand and collapse
    {
        ImportSettingsDialog testDlg;
        testDlg.show();
        QCoreApplication::processEvents();
        const int hBefore = testDlg.height();

        auto* card = UiTestHelper::Require<QFrame>(&testDlg, QStringLiteral("AdvancedSettingsCard"));
        auto* headerBtn = UiTestHelper::Require<QPushButton>(card, QStringLiteral("CardHeaderBtn"));

        headerBtn->click(); // expand
        QCoreApplication::processEvents();
        const int hExpanded = testDlg.height();

        headerBtn->click(); // collapse
        QCoreApplication::processEvents();
        const int hAfter = testDlg.height();

        std::cout << "[DialogResizeDiag] before: " << hBefore << ", expanded: " << hExpanded << ", after: " << hAfter << std::endl;
        Check(hExpanded > hBefore, "Dialog expands when advanced card is opened");
        Check(std::abs(hAfter - hBefore) <= 2, "Dialog height returns to initial after collapsing");
        ++checks;
    }

    std::cout << "ImportSettingsDialogTests: " << checks << " checks passed." << std::endl;
}

} // namespace ImportSettingsDialogTests
