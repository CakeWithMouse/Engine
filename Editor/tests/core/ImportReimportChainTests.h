#pragma once

#include "../../Core/ModelImportSidecar.h"
#include "../../Core/ModelImportSidecarTransaction.h"
#include "../../Core/ImportSettings.h"
#include "../../Core/FileService.h"
#include "../../Core/Logger.h"

#include <QDir>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ImportReimportChainTests
{

inline void Check(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (ImportReimportChain): %s\n", msg);
        std::exit(1);
    }
}

inline bool Near1(float v) { return std::fabs(v - 1.0f) < 1e-6f; }

// Step 17.7 / T7 regression chain on the Editor side, without a GPU:
//   new import writes a version 2 sidecar (scale lives in geometry)
//   -> spawn reads Transform.scale = 1 (R1)
//   -> "Import Settings..." edits the geometry scale under a transaction (R4)
//   -> a successful reimport keeps Transform at 1 and stores the new geometry scale
//   -> a failed reimport rolls the sidecar back, Transform is still 1, geometry scale reverts.
// The Engine side of "reimport preserves Transform" is covered by ModelReimportScaleTests;
// here the invariant is that the Editor never moves scale onto Transform for a v2 asset and
// that a broken reimport cannot leave a half-written sidecar behind.
inline void Run(int& outChecks)
{
    using namespace EditorCore;

    int checks = 0;
    auto check = [&](bool cond, const char* msg) {
        Check(cond, msg);
        ++checks;
    };

    QTemporaryDir tempDir;
    check(tempDir.isValid(), "Temporary directory created for reimport chain tests");
    Logger logger;
    FileService fs(&logger);
    fs.SetProjectRoot(tempDir.path());
    const QString projectRoot = tempDir.path();
    const QString contentRoot = QDir(projectRoot).filePath(QStringLiteral("Content"));
    fs.CreateDirectoryPath(QStringLiteral("Content/Models"));

    const QString vpath = QStringLiteral("/Game/Models/Hero.glb");

    // Step 1: a new import writes a version 2 sidecar with geometry scale 0.01.
    {
        ImportSettings v2;
        v2.version = 2;
        v2.model.scale = 0.01f;
        check(ModelImportSidecar::WriteSidecar(fs, vpath, v2, contentRoot), "New import writes a v2 sidecar");
    }

    // Step 2: spawn reads Transform.scale = 1 while geometry keeps 0.01 (R1).
    {
        ImportSettingsFile::LoadResult res;
        QString err;
        const float spawn = ModelImportSidecar::ReadSpawnScale(fs, vpath, contentRoot, &res, &err);
        check(res == ImportSettingsFile::LoadResult::Success, "Spawn read succeeds");
        check(Near1(spawn), "v2 spawn Transform.scale is 1 (R1)");
        const ImportSettings geom = ModelImportSidecar::ReadSettings(fs, vpath, contentRoot);
        check(geom.version == 2 && std::fabs(geom.model.scale - 0.01f) < 1e-6f,
              "Geometry scale stays 0.01 for the Engine loader");
    }

    // Step 3: "Import Settings..." changes the geometry scale to 2.0 under a transaction,
    //         the reimport succeeds -> Transform is still 1, geometry is now 2.0.
    {
        SidecarTransaction tx;
        tx.Begin(fs, vpath, contentRoot);
        check(tx.active(), "Transaction active before writing new settings");

        ImportSettings edited;
        edited.version = 2;
        edited.model.scale = 2.0f;
        check(ModelImportSidecar::WriteSidecar(fs, vpath, edited, contentRoot), "Edit writes v2 sidecar scale 2.0");

        QString err;
        check(tx.FinishUpdate(fs, vpath, contentRoot, true, &err) == SidecarUpdateResult::Committed,
              "Successful reimport commits");

        ImportSettingsFile::LoadResult res;
        const float spawn = ModelImportSidecar::ReadSpawnScale(fs, vpath, contentRoot, &res, nullptr);
        check(res == ImportSettingsFile::LoadResult::Success && Near1(spawn),
              "Transform.scale is still 1 after a successful reimport (not reset)");
        const ImportSettings geom = ModelImportSidecar::ReadSettings(fs, vpath, contentRoot);
        check(geom.version == 2 && std::fabs(geom.model.scale - 2.0f) < 1e-6f,
              "Geometry scale updated to 2.0 after commit");
    }

    // Step 4: a second edit fails (e.g. the material slot count changed) -> rollback,
    //         Transform is still 1, geometry reverts to the committed 2.0.
    {
        SidecarTransaction tx;
        tx.Begin(fs, vpath, contentRoot);
        check(tx.active(), "Transaction active before the failing edit");

        ImportSettings edited;
        edited.version = 2;
        edited.model.scale = 5.0f;
        check(ModelImportSidecar::WriteSidecar(fs, vpath, edited, contentRoot), "Edit writes v2 sidecar scale 5.0");

        check(tx.FinishUpdate(fs, vpath, contentRoot, false) == SidecarUpdateResult::RolledBack,
              "Failed reimport rolls back (R4)");

        ImportSettingsFile::LoadResult res;
        const float spawn = ModelImportSidecar::ReadSpawnScale(fs, vpath, contentRoot, &res, nullptr);
        check(res == ImportSettingsFile::LoadResult::Success && Near1(spawn),
              "Transform.scale is still 1 after a failed reimport");
        const ImportSettings geom = ModelImportSidecar::ReadSettings(fs, vpath, contentRoot);
        check(geom.version == 2 && std::fabs(geom.model.scale - 2.0f) < 1e-6f,
              "Geometry scale restored to the committed 2.0 after rollback");
    }

    // Step 5: a legacy v1 asset reimported and converted to v2 flips spawn from carrying
    //         the scale on Transform to baking it into geometry (Transform becomes 1).
    {
        const QString v1path = QStringLiteral("/Game/Models/Old.glb");
        check(ModelImportSidecar::WriteSidecar(fs, v1path, 0.01f, contentRoot), "Write a v1 sidecar scale 0.01");
        check(std::fabs(ModelImportSidecar::ReadSpawnScale(fs, v1path, contentRoot) - 0.01f) < 1e-6f,
              "v1 spawn Transform.scale carries 0.01 before conversion");

        SidecarTransaction tx;
        tx.Begin(fs, v1path, contentRoot);
        ImportSettings converted;
        converted.version = 2;
        converted.model.scale = 0.01f;
        check(ModelImportSidecar::WriteSidecar(fs, v1path, converted, contentRoot), "Convert the v1 sidecar to v2");
        check(tx.FinishUpdate(fs, v1path, contentRoot, true) == SidecarUpdateResult::Committed,
              "Conversion commits");
        check(Near1(ModelImportSidecar::ReadSpawnScale(fs, v1path, contentRoot)),
              "After conversion to v2 the spawn Transform.scale is 1 (R1)");
    }

    outChecks += checks;
}

} // namespace ImportReimportChainTests
