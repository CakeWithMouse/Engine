#pragma once

#include "ImportSettingsDialog.h"
#include "ImportSettings.h"
#include "UiTestHelper.h"
#include "CommandRegistry.h"
#include "IEditorHost.h"

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

    // 1. Creation and presence of widgets by objectName
    ImportSettingsDialog dlg;
    auto* spin = UiTestHelper::Require<QDoubleSpinBox>(&dlg, QStringLiteral("ImportScaleSpin"));
    auto* btnPreset1 = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("Preset1"));
    auto* btnPreset01 = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("Preset01"));
    auto* btnPreset001 = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("Preset001"));
    auto* btnOk = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("ImportOkBtn"));
    auto* btnCancel = UiTestHelper::Require<QPushButton>(&dlg, QStringLiteral("ImportCancelBtn"));

    Check(spin != nullptr, "ImportScaleSpin exists");
    Check(btnPreset1 != nullptr, "Preset1 button exists");
    Check(btnPreset01 != nullptr, "Preset01 button exists");
    Check(btnPreset001 != nullptr, "Preset001 button exists");
    Check(btnOk != nullptr, "ImportOkBtn exists");
    Check(btnCancel != nullptr, "ImportCancelBtn exists");
    ++checks;

    // 2. Default scale
    Check(std::abs(dlg.Scale() - 1.0f) < 1e-5f, "Initial default scale is 1.0");
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

    // 6. Cancel button rejects dialog
    UiTestHelper::ClickButton(&dlg, QStringLiteral("ImportCancelBtn"));
    Check(dlg.result() == QDialog::Rejected, "Cancel button sets Rejected result");
    ++checks;

    // 7. OK button accepts dialog and preserves value
    dlg.SetScale(0.1f);
    UiTestHelper::ClickButton(&dlg, QStringLiteral("ImportOkBtn"));
    Check(dlg.result() == QDialog::Accepted, "Ok button sets Accepted result");
    Check(std::abs(dlg.Scale() - 0.1f) < 1e-5f, "Accepted scale is 0.1");
    ++checks;

    // 8. Cancel via ResolveModelImportScale does not post any command
    {
        std::vector<EditorCommand> postedCommands;

        // RAII hook guarantees reset even upon early return or exception
        ScopedImportDialogHook hook([](ImportSettingsDialog& dialog) {
            UiTestHelper::ClickButton(&dialog, QStringLiteral("ImportCancelBtn"));
            return dialog.result();
        });

        // The exact handler logic executed by Assets.Import
        auto importHandler = [&postedCommands](const QVariantMap& params, bool hasSource) -> bool {
            const auto decision = ResolveModelImportScale(params, hasSource, nullptr);
            if (!decision.proceed)
            {
                return false;
            }
            EditorCommand c;
            c.type = EditorCommand::ImportModel;
            c.importScale = decision.scale;
            postedCommands.push_back(c);
            return true;
        };

        // When showDialog is true, dialog runs hook and Cancel returns proceed = false
        QVariantMap params;
        params[QStringLiteral("showDialog")] = true;

        const bool result = importHandler(params, true);
        Check(!result, "Handler returns false on Cancel");
        Check(postedCommands.empty(), "Cancel did not create or post any command");
        ++checks;
    }

    // 9. Ok with preset via ResolveModelImportScale posts command with chosen scale
    {
        std::vector<EditorCommand> postedCommands;

        // RAII hook guarantees reset even upon early return or exception
        ScopedImportDialogHook hook([](ImportSettingsDialog& dialog) {
            UiTestHelper::ClickButton(&dialog, QStringLiteral("Preset001"));
            UiTestHelper::ClickButton(&dialog, QStringLiteral("ImportOkBtn"));
            return dialog.result();
        });

        auto importHandler = [&postedCommands](const QVariantMap& params, bool hasSource) -> bool {
            const auto decision = ResolveModelImportScale(params, hasSource, nullptr);
            if (!decision.proceed)
            {
                return false;
            }
            EditorCommand c;
            c.type = EditorCommand::ImportModel;
            c.importScale = decision.scale;
            postedCommands.push_back(c);
            return true;
        };

        QVariantMap params;
        params[QStringLiteral("showDialog")] = true;

        const bool result = importHandler(params, true);
        Check(result, "Handler returns true on Ok");
        Check(postedCommands.size() == 1, "Ok created exactly one command");
        Check(std::abs(postedCommands[0].importScale - 0.01f) < 1e-5f, "Command received importScale 0.01");
        ++checks;

        // 10. Explicit scale in params bypasses dialog
        postedCommands.clear();
        QVariantMap explicitParams;
        explicitParams[QStringLiteral("scale")] = 0.5;
        const bool explicitResult = importHandler(explicitParams, true);
        Check(explicitResult, "Explicit scale returns true");
        Check(postedCommands.size() == 1, "Explicit scale created command");
        Check(std::abs(postedCommands[0].importScale - 0.5f) < 1e-5f, "Command received explicit scale 0.5");
        ++checks;
    }

    std::cout << "ImportSettingsDialogTests: " << checks << " checks passed." << std::endl;
}

} // namespace ImportSettingsDialogTests
