#pragma once

#include "../../Core/ImportSettings.h"
#include "../../Core/ModelImportSidecar.h"
#include "../../Core/FileService.h"
#include "../../Core/Logger.h"

#include <QDir>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ImportSettingsSpawnV2Tests
{

inline void Check(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (ImportSettingsSpawnV2): %s\n", msg);
        std::exit(1);
    }
}

// R1/T3 spawn semantics: a version >= 2 sidecar bakes scale into geometry, so the spawned
// Transform stays at 1. Version 1 / legacy / missing keep the old Transform-only behavior.
inline void Run(int& outChecks)
{
    using namespace EditorCore;

    int checks = 0;
    auto check = [&](bool cond, const char* msg) {
        Check(cond, msg);
        ++checks;
    };

    QTemporaryDir tempDir;
    check(tempDir.isValid(), "Temporary directory created for spawn v2 tests");
    Logger logger;
    FileService fs(&logger);
    fs.SetProjectRoot(tempDir.path());
    const QString projectRoot = tempDir.path();
    const QString contentRoot = QDir(projectRoot).filePath(QStringLiteral("Content"));
    fs.CreateDirectoryPath(QStringLiteral("Content/Models"));

    auto spawnTransform = [&](const QString& virtualPath, float out[3]) {
        const float s = ModelImportSidecar::ReadSpawnScale(fs, virtualPath, contentRoot);
        out[0] = s;
        out[1] = s;
        out[2] = s;
    };

    // version 2: scale lives in geometry, spawn Transform is 1 regardless of stored scale.
    {
        const QString vpath = QStringLiteral("/Game/Models/V2/Hero.glb");
        ImportSettings v2;
        v2.version = 2;
        v2.model.scale = 0.01f;
        check(ModelImportSidecar::WriteSidecar(fs, vpath, v2, contentRoot), "Write v2 sidecar scale 0.01");

        ImportSettingsFile::LoadResult res;
        QString err;
        const float spawn = ModelImportSidecar::ReadSpawnScale(fs, vpath, contentRoot, &res, &err);
        check(res == ImportSettingsFile::LoadResult::Success, "v2 spawn read succeeds");
        check(std::abs(spawn - 1.0f) < 1e-6f, "v2 spawn scale is 1 (geometry holds the scale)");

        float t[3];
        spawnTransform(vpath, t);
        check(std::abs(t[0] - 1.0f) < 1e-6f && std::abs(t[1] - 1.0f) < 1e-6f && std::abs(t[2] - 1.0f) < 1e-6f,
              "v2 spawn Transform.scale is {1,1,1}");

        // The stored geometry scale is still readable for the Engine loader.
        const ImportSettings loaded = ModelImportSidecar::ReadSettings(fs, vpath, contentRoot);
        check(loaded.version == 2 && std::abs(loaded.model.scale - 0.01f) < 1e-6f,
              "v2 sidecar keeps geometry scale 0.01 for the Engine");
    }

    // version 2 scale 2 and scale 1 also spawn at Transform 1.
    {
        const QString vpath = QStringLiteral("/Game/Models/V2Big/Tree.glb");
        ImportSettings v2;
        v2.version = 2;
        v2.model.scale = 2.0f;
        check(ModelImportSidecar::WriteSidecar(fs, vpath, v2, contentRoot), "Write v2 sidecar scale 2");
        check(std::abs(ModelImportSidecar::ReadSpawnScale(fs, vpath, contentRoot) - 1.0f) < 1e-6f,
              "v2 scale 2 still spawns at Transform 1");
    }

    // version 1: unchanged legacy behavior, spawn Transform carries the scale.
    {
        const QString vpath = QStringLiteral("/Game/Models/V1/Chair.glb");
        check(ModelImportSidecar::WriteSidecar(fs, vpath, 0.01f, contentRoot), "Write v1 sidecar scale 0.01");
        ImportSettingsFile::LoadResult res;
        QString err;
        const float spawn = ModelImportSidecar::ReadSpawnScale(fs, vpath, contentRoot, &res, &err);
        check(res == ImportSettingsFile::LoadResult::Success, "v1 spawn read succeeds");
        check(std::abs(spawn - 0.01f) < 1e-6f, "v1 spawn scale is the stored 0.01");
        float t[3];
        spawnTransform(vpath, t);
        check(std::abs(t[0] - 0.01f) < 1e-6f && std::abs(t[1] - 0.01f) < 1e-6f && std::abs(t[2] - 0.01f) < 1e-6f,
              "v1 spawn Transform.scale is {0.01,0.01,0.01}");
    }

    // legacy {"scale":...} migrates to v1 and keeps Transform-only semantics.
    {
        const QString vpath = QStringLiteral("/Game/Models/Legacy/Table.glb");
        const QString sidecar = ModelImportSidecar::ResolveSidecarPath(vpath, projectRoot, contentRoot);
        fs.WriteFileAtomic(sidecar, QByteArray("{\"scale\":0.0254}"));
        check(std::abs(ModelImportSidecar::ReadSpawnScale(fs, vpath, contentRoot) - 0.0254f) < 1e-6f,
              "legacy scale spawns at Transform 0.0254");
    }

    // no sidecar defaults to Transform 1.
    {
        const QString vpath = QStringLiteral("/Game/Models/None/Lamp.glb");
        ImportSettingsFile::LoadResult res;
        QString err;
        const float spawn = ModelImportSidecar::ReadSpawnScale(fs, vpath, contentRoot, &res, &err);
        check(res == ImportSettingsFile::LoadResult::NotFound, "missing sidecar returns NotFound");
        check(std::abs(spawn - 1.0f) < 1e-6f, "missing sidecar spawns at Transform 1");
    }

    outChecks += checks;
}

} // namespace ImportSettingsSpawnV2Tests