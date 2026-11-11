#pragma once

#include "ImportSettingsDialog.h"
#include "ModelImportEditFlow.h"
#include "ImportSettings.h"
#include "SettingsService.h"
#include "UiTestHelper.h"

#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace ImportSettingsV2EditTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string("ImportSettingsV2Edit test failed: ") + message);
    }
}

inline void Run()
{
    std::cout << "[Test] Running ImportSettingsV2EditTests..." << std::endl;
    int checks = 0;

    // 1. The dialog produces version 2 settings for new imports.
    {
        ImportSettingsDialog dlg;
        const auto settings = dlg.Settings();
        Check(settings.version == 2, "New import dialog settings are version 2");
        const auto root = QJsonDocument::fromJson(EditorCore::ImportSettingsFile::Serialize(settings)).object();
        Check(root.value(QStringLiteral("version")).toInt() == 2, "New import options JSON writes version 2");
        ++checks;
    }

    // 2. Editing a legacy v1 sidecar shows scale 1.0 with a notice, and migrates to v2 on save.
    {
        ImportSettingsDialog dlg;
        EditorCore::ImportSettings v1;
        v1.version = 1;
        v1.model.scale = 0.05f;

        const bool legacy = ModelImportEditFlow::PrepareForEdit(dlg, v1);
        Check(legacy, "PrepareForEdit reports a legacy v1 sidecar");
        Check(std::abs(dlg.Scale() - 1.0f) < 1e-5f, "v1 edit shows scale 1.0 (old scale lives on the scene object)");

        auto* spin = UiTestHelper::Require<QDoubleSpinBox>(&dlg, QStringLiteral("ImportScaleSpin"));
        Check(spin->toolTip().contains(QStringLiteral("сцене")) || spin->toolTip().contains(QStringLiteral("scene object")),
              "v1 edit scale tooltip explains the old scale is on the scene object");

        auto* notice = dlg.findChild<QLabel*>(QStringLiteral("LegacyScaleNotice"));
        Check(notice != nullptr, "v1 edit shows a legacy migration notice");
        Check(notice->text().contains(QStringLiteral("версию 2")) || notice->text().contains(QStringLiteral("version 2")),
              "notice warns that saving migrates to version 2");
        Check(notice->text().contains(QStringLiteral("Transform")) || notice->text().contains(QStringLiteral("геометрия")),
              "notice warns geometry scales while existing Transform stays");

        Check(dlg.Settings().version == 2, "Accepted v1 edit settings are version 2");
        ++checks;
    }

    // 3. Editing a v2 sidecar shows the stored geometry scale faithfully, with no legacy notice.
    {
        ImportSettingsDialog dlg;
        EditorCore::ImportSettings v2;
        v2.version = 2;
        v2.model.scale = 0.01f;

        const bool legacy = ModelImportEditFlow::PrepareForEdit(dlg, v2);
        Check(!legacy, "PrepareForEdit reports a v2 sidecar is not legacy");
        Check(std::abs(dlg.Scale() - 0.01f) < 1e-5f, "v2 edit shows the stored geometry scale 0.01");
        Check(dlg.findChild<QLabel*>(QStringLiteral("LegacyScaleNotice")) == nullptr, "v2 edit has no legacy notice");
        Check(dlg.Settings().version == 2, "v2 edit settings stay version 2");
        ++checks;
    }

    // 4. Step D: edit mode reads the sidecar, not defaults or SettingsService leftovers.
    {
        auto& svc = SettingsService::Instance();
        svc.SetValue(QStringLiteral("import/model/scale"), 0.01);
        svc.SetValue(QStringLiteral("import/model/flipUv"), true);
        svc.SetValue(QStringLiteral("import/model/normals"), QStringLiteral("smooth"));
        svc.SetValue(QStringLiteral("import/model/mergeMeshes"), true);

        ImportSettingsDialog dlg;
        EditorCore::ImportSettings v2;
        v2.version = 2;
        v2.model.scale = 0.05f;
        v2.model.useFileUnitScale = false;
        v2.model.normals = EditorCore::ModelNormalsMode::Flat;
        v2.model.flipUv = true;
        v2.model.mergeMeshes = false;

        const bool legacy = ModelImportEditFlow::PrepareForEdit(dlg, v2);
        Check(!legacy, "Sidecar v2 is not legacy");
        Check(std::abs(dlg.Scale() - 0.05f) < 1e-5f, "Edit dialog shows sidecar scale 0.05, not default 1.0 or stored 0.01");
        const auto shown = dlg.Settings();
        Check(!shown.model.useFileUnitScale, "Edit dialog shows sidecar useFileUnitScale false, not default true");
        Check(shown.model.normals == EditorCore::ModelNormalsMode::Flat, "Edit dialog shows sidecar normals Flat, not stored Smooth");
        Check(shown.model.flipUv, "Edit dialog shows sidecar flipUv true, not default false");
        Check(!shown.model.mergeMeshes, "Edit dialog shows sidecar mergeMeshes false, not stored true");

        svc.Remove(QStringLiteral("import/model/scale"));
        svc.Remove(QStringLiteral("import/model/flipUv"));
        svc.Remove(QStringLiteral("import/model/normals"));
        svc.Remove(QStringLiteral("import/model/mergeMeshes"));
        ++checks;
    }

    std::cout << "ImportSettingsV2EditTests: " << checks << " checks passed." << std::endl;
}

} // namespace ImportSettingsV2EditTests