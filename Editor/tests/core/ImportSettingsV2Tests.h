#pragma once

#include "../../Core/ImportSettings.h"
#include "../../../Engine/tests/ImportSettingsTestCases.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace ImportSettingsV2Tests
{

inline QByteArray ToBytes(std::string_view sv)
{
    return QByteArray(sv.data(), static_cast<int>(sv.size()));
}

inline void Check(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (ImportSettingsV2): %s\n", msg);
        std::exit(1);
    }
}

// R1/T2 parity: the Editor Core parser handles the shared v2 cases exactly like the Engine
// parser. Reading v1 keeps version 1; the default stays v1 (baseline round-trip); a caller
// that sets version 2 (new imports / dialog) serializes version 2; unknown fields survive.
inline void Run(int& outChecks)
{
    using namespace EditorCore;
    using namespace ImportSettingsTestCases;

    int checks = 0;
    auto check = [&](bool cond, const char* msg) {
        Check(cond, msg);
        ++checks;
    };

    // v2 full schema
    {
        ImportSettings s;
        QString err;
        check(ImportSettingsFile::Parse(ToBytes(kValidV2), s, &err), "Parse valid v2");
        check(s.version == 2, "v2 version is 2");
        check(std::abs(s.model.scale - 0.01f) < 1e-6f, "v2 scale is 0.01");
        check(s.model.useFileUnitScale, "v2 useFileUnitScale is true");
        check(s.model.normals == ModelNormalsMode::Smooth, "v2 normals is smooth");

        const QByteArray roundTrip = ImportSettingsFile::Serialize(s);
        ImportSettings loaded;
        check(ImportSettingsFile::Parse(roundTrip, loaded, &err), "v2 round-trip parse");
        check(loaded.version == 2, "v2 round-trip keeps version 2");
        check(std::abs(loaded.model.scale - 0.01f) < 1e-6f, "v2 round-trip keeps scale");
    }

    // v2 with unknown fields preserved through serialize
    {
        ImportSettings s;
        QString err;
        check(ImportSettingsFile::Parse(ToBytes(kValidV2WithUnknownFields), s, &err), "Parse v2 with unknown fields");
        check(s.version == 2 && std::abs(s.model.scale - 2.0f) < 1e-6f, "v2 unknown-field version and scale");
        check(s.extraFields.contains(QStringLiteral("custom_root")), "v2 root unknown field preserved");
        check(s.model.extraFields.contains(QStringLiteral("custom_model_meta")), "v2 model unknown field preserved");

        const QByteArray roundTrip = ImportSettingsFile::Serialize(s);
        const QJsonObject root = QJsonDocument::fromJson(roundTrip).object();
        check(root.contains(QStringLiteral("custom_root")), "v2 serialize keeps root unknown field");
        check(root.value(QStringLiteral("model")).toObject().contains(QStringLiteral("custom_model_meta")),
              "v2 serialize keeps model unknown field");
        check(root.value(QStringLiteral("version")).toInt() == 2, "v2 serialize writes version 2");
    }

    // minimal v2 falls back to defaults
    {
        ImportSettings s;
        QString err;
        check(ImportSettingsFile::Parse(ToBytes(kValidV2Minimal), s, &err), "Parse minimal v2");
        check(s.version == 2, "minimal v2 version is 2");
        check(s.model.normals == ModelNormalsMode::File, "minimal v2 normals defaults to file");
        check(s.model.mergeMeshes, "minimal v2 mergeMeshes defaults to true");
        check(!s.model.useFileUnitScale, "minimal v2 useFileUnitScale defaults to false");
    }

    // v1 and legacy keep version 1
    {
        ImportSettings s;
        QString err;
        check(ImportSettingsFile::Parse(ToBytes(kValidV1), s, &err), "Parse valid v1");
        check(s.version == 1, "v1 stays version 1");
        check(ImportSettingsFile::Parse(ToBytes(kLegacyScale), s, &err), "Parse legacy scale");
        check(s.version == 1, "legacy migrates to version 1");
    }

    // Default stays v1 (baseline invariant); a new-import caller sets version 2 explicitly.
    {
        ImportSettings fresh;
        check(fresh.version == 1, "default ImportSettings stays version 1");
        const QByteArray defaultBytes = ImportSettingsFile::Serialize(fresh);
        check(QJsonDocument::fromJson(defaultBytes).object().value(QStringLiteral("version")).toInt() == 1,
              "default serialize writes version 1");

        fresh.version = 2;
        const QByteArray v2Bytes = ImportSettingsFile::Serialize(fresh);
        check(QJsonDocument::fromJson(v2Bytes).object().value(QStringLiteral("version")).toInt() == 2,
              "new-import settings with version 2 serialize version 2");
    }

    outChecks += checks;
}

} // namespace ImportSettingsV2Tests