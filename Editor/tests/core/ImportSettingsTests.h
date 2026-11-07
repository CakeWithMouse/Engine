#pragma once

#include "../../Core/FileService.h"
#include "../../Core/Logger.h"
#include "../../Core/ImportSettings.h"
#include "../../Core/ModelImportSidecar.h"

#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace
{

void CheckImportSettings(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (ImportSettings): %s\n", msg);
        std::exit(1);
    }
}

inline void RunImportSettingsTests(int& outChecks)
{
    using namespace EditorCore;

    int checks = 0;
    auto check = [&](bool cond, const char* msg) {
        CheckImportSettings(cond, msg);
        ++checks;
    };

    // 1. IsValidScale and ClampScale
    check(ImportSettingsFile::IsValidScale(1.0f), "Scale 1.0 is valid");
    check(ImportSettingsFile::IsValidScale(0.1f), "Scale 0.1 is valid");
    check(ImportSettingsFile::IsValidScale(0.01f), "Scale 0.01 is valid");
    check(ImportSettingsFile::IsValidScale(1e-4f), "Scale 1e-4 is valid");
    check(ImportSettingsFile::IsValidScale(1e4f), "Scale 1e4 is valid");
    check(!ImportSettingsFile::IsValidScale(0.0f), "Scale 0.0 is invalid");
    check(!ImportSettingsFile::IsValidScale(-1.0f), "Negative scale is invalid");
    check(!ImportSettingsFile::IsValidScale(1e-5f), "Scale < 1e-4 is invalid");
    check(!ImportSettingsFile::IsValidScale(1e5f), "Scale > 1e4 is invalid");
    check(!ImportSettingsFile::IsValidScale(std::numeric_limits<float>::quiet_NaN()), "NaN scale is invalid");
    check(!ImportSettingsFile::IsValidScale(std::numeric_limits<float>::infinity()), "Inf scale is invalid");

    check(std::abs(ImportSettingsFile::ClampScale(1.0f) - 1.0f) < 1e-6f, "Clamp 1.0 keeps 1.0");
    check(std::abs(ImportSettingsFile::ClampScale(1e-6f) - 1e-4f) < 1e-6f, "Clamp underflow to 1e-4");
    check(std::abs(ImportSettingsFile::ClampScale(1e6f) - 1e4f) < 1e-6f, "Clamp overflow to 1e4");

    // 2. Round-trip serialization
    ImportSettings s1{0.01f};
    const QByteArray bytes = ImportSettingsFile::Serialize(s1);
    check(!bytes.isEmpty(), "Serialize produced non-empty data");

    ImportSettings s1Loaded;
    QString err;
    const bool parseOk = ImportSettingsFile::Parse(bytes, s1Loaded, &err);
    check(parseOk, "Parse succeeded on valid serialized data");
    check(err.isEmpty(), "No error on valid parse");
    check(std::abs(s1Loaded.scale - 0.01f) < 1e-6f, "Loaded scale matches serialized 0.01");

    // 3. Broken JSON handling (explicit error logged, no silent fallback, default value set)
    ImportSettings sBroken;
    sBroken.scale = 42.0f;
    QString brokenErr;
    const bool brokenOk = ImportSettingsFile::Parse(QByteArray("not a valid json { broken"), sBroken, &brokenErr);
    check(!brokenOk, "Parse fails on broken JSON");
    check(!brokenErr.isEmpty(), "Error message populated for broken JSON");
    check(std::abs(sBroken.scale - 1.0f) < 1e-6f, "Scale reset to default 1.0 on broken JSON");

    // 4. Missing scale field in JSON
    ImportSettings sMissing;
    sMissing.scale = 42.0f;
    QString missingErr;
    const bool missingOk = ImportSettingsFile::Parse(QByteArray("{\"version\": 1}"), sMissing, &missingErr);
    check(!missingOk, "Parse fails on JSON missing scale");
    check(!missingErr.isEmpty(), "Error message populated for missing scale");
    check(std::abs(sMissing.scale - 1.0f) < 1e-6f, "Scale reset to default 1.0 on missing scale");

    // 5. Non-number scale field
    ImportSettings sStr;
    sStr.scale = 42.0f;
    QString strErr;
    const bool strOk = ImportSettingsFile::Parse(QByteArray("{\"version\": 1, \"scale\": \"big\"}"), sStr, &strErr);
    check(!strOk, "Parse fails on string scale");
    check(!strErr.isEmpty(), "Error message populated for string scale");
    check(std::abs(sStr.scale - 1.0f) < 1e-6f, "Scale reset to default 1.0 on string scale");

    // 6. Out of range scale values
    ImportSettings sOutOfRange;
    sOutOfRange.scale = 42.0f;
    QString rangeErr;
    const bool rangeOk = ImportSettingsFile::Parse(QByteArray("{\"version\": 1, \"scale\": 0.0}"), sOutOfRange, &rangeErr);
    check(!rangeOk, "Parse fails on scale 0.0");
    check(!rangeErr.isEmpty(), "Error message populated for scale 0.0");
    check(std::abs(sOutOfRange.scale - 1.0f) < 1e-6f, "Scale reset to default 1.0 on scale 0.0");

    const bool negOk = ImportSettingsFile::Parse(QByteArray("{\"version\": 1, \"scale\": -5.0}"), sOutOfRange, &rangeErr);
    check(!negOk, "Parse fails on negative scale");
    check(std::abs(sOutOfRange.scale - 1.0f) < 1e-6f, "Scale reset to default 1.0 on negative scale");

    const bool largeOk = ImportSettingsFile::Parse(QByteArray("{\"version\": 1, \"scale\": 100000.0}"), sOutOfRange, &rangeErr);
    check(!largeOk, "Parse fails on scale > 1e4");
    check(std::abs(sOutOfRange.scale - 1.0f) < 1e-6f, "Scale reset to default 1.0 on scale > 1e4");

    // 7. Sidecar path helper
    check(ImportSettingsFile::SidecarPathForAsset(QStringLiteral("Content/Models/Hero/Hero.glb")) ==
          QStringLiteral("Content/Models/Hero/Hero.glb.importsettings.json"), "SidecarPathForAsset appends .importsettings.json");
    check(ImportSettingsFile::SidecarPathForAsset(QString()).isEmpty(), "SidecarPathForAsset empty for empty path");

    // 8. FileService integration (Save & Load with sandbox)
    QTemporaryDir tempDir;
    check(tempDir.isValid(), "Temporary directory created for ImportSettings tests");

    Logger logger;
    FileService fs(&logger);
    fs.SetProjectRoot(tempDir.path());

    const QString sidecarRel = QStringLiteral("Models/Hero/Hero.glb.importsettings.json");
    const ImportSettings saveSet{0.1f};
    QString saveErr;
    const bool saveOk = ImportSettingsFile::Save(fs, sidecarRel, saveSet, &saveErr);
    check(saveOk, "ImportSettingsFile::Save succeeded via FileService");
    check(saveErr.isEmpty(), "No error on save");

    // FileExists checks
    check(fs.FileExists(sidecarRel), "FileExists returns true for existing file");
    check(!fs.FileExists(QStringLiteral("NonExistent.json")), "FileExists returns false for missing file");
    check(!fs.FileExists(QStringLiteral("../Outside.json")), "FileExists returns false outside sandbox");

    // Load existing valid file
    ImportSettings loadSet;
    QString loadErr;
    const auto loadOk = ImportSettingsFile::Load(fs, sidecarRel, loadSet, &loadErr);
    check(loadOk == ImportSettingsFile::LoadResult::Success, "ImportSettingsFile::Load succeeded via FileService");
    check(loadErr.isEmpty(), "No error on load");
    check(std::abs(loadSet.scale - 0.1f) < 1e-6f, "Loaded scale matches saved scale 0.1");

    // Load missing file (returns NotFound, scale = 1.0f, no error exception)
    ImportSettings missingSet;
    missingSet.scale = 42.0f;
    QString missingLoadErr;
    const auto missingRes = ImportSettingsFile::Load(fs, QStringLiteral("Models/Missing/none.glb.importsettings.json"), missingSet, &missingLoadErr);
    check(missingRes == ImportSettingsFile::LoadResult::NotFound, "Load missing file returns NotFound");
    check(std::abs(missingSet.scale - 1.0f) < 1e-6f, "Missing file resets scale to default 1.0f");
    check(missingLoadErr.contains(QStringLiteral("sidecar not found")), "Missing file error message contains info");

    // Load corrupt file (returns Error, scale = 1.0f)
    const QString corruptRel = QStringLiteral("Models/Corrupt/corrupt.importsettings.json");
    fs.WriteFileAtomic(corruptRel, QByteArray("not a valid json { broken"));
    ImportSettings corruptSet;
    corruptSet.scale = 42.0f;
    QString corruptLoadErr;
    const auto corruptRes = ImportSettingsFile::Load(fs, corruptRel, corruptSet, &corruptLoadErr);
    check(corruptRes == ImportSettingsFile::LoadResult::Error, "Load corrupt file returns Error");
    check(std::abs(corruptSet.scale - 1.0f) < 1e-6f, "Corrupt file resets scale to default 1.0f");
    check(corruptLoadErr.contains(QStringLiteral("JSON parse error")), "Corrupt file error message contains parse error");

    // Load out-of-range file (returns Error, scale = 1.0f)
    const QString outOfRangeRel = QStringLiteral("Models/Range/range.importsettings.json");
    fs.WriteFileAtomic(outOfRangeRel, QByteArray("{\"version\": 1, \"scale\": -10.0}"));
    ImportSettings rangeSet;
    rangeSet.scale = 42.0f;
    QString rangeLoadErr;
    const auto rangeRes = ImportSettingsFile::Load(fs, outOfRangeRel, rangeSet, &rangeLoadErr);
    check(rangeRes == ImportSettingsFile::LoadResult::Error, "Load out-of-range file returns Error");
    check(std::abs(rangeSet.scale - 1.0f) < 1e-6f, "Out-of-range file resets scale to default 1.0f");

    // Rejection on Save with invalid scale
    const ImportSettings badSaveSet{-1.0f};
    QString badSaveErr;
    const bool badSaveOk = ImportSettingsFile::Save(fs, sidecarRel, badSaveSet, &badSaveErr);
    check(!badSaveOk, "Save fails for invalid scale");
    check(!badSaveErr.isEmpty(), "Error populated for invalid scale save");

    // --- ModelImportSidecar Tests ---
    const QString projectRoot = tempDir.path();
    const QString contentRoot = QDir(projectRoot).filePath(QStringLiteral("Content"));
    fs.CreateDirectoryPath(QStringLiteral("Content/Models"));

    // 1. sidecar пишется после успешного импорта
    const QString heroVirtual = QStringLiteral("/Game/Models/Hero/Hero.glb");
    QString writeErr;
    const bool writeOk = ModelImportSidecar::WriteSidecar(fs, heroVirtual, 0.01f, contentRoot, &writeErr);
    check(writeOk, "WriteSidecar succeeds for valid virtual path");
    const QString heroSidecar = ModelImportSidecar::ResolveSidecarPath(heroVirtual, projectRoot, contentRoot);
    check(fs.FileExists(heroSidecar), "Hero sidecar exists after write");

    // 2. не пишется при неуспехе (симулируем неуспех импорта: sidecar отсутствует)
    const QString failedVirtual = QStringLiteral("/Game/Models/Failed/Failed.glb");
    const QString failedSidecar = ModelImportSidecar::ResolveSidecarPath(failedVirtual, projectRoot, contentRoot);
    check(!fs.FileExists(failedSidecar), "Failed import does not have sidecar file");

    // 3. пакет из 3 файлов даёт 3 sidecar с правильными путями
    const QString batch1 = QStringLiteral("/Game/Models/Batch/Tree1.glb");
    const QString batch2 = QStringLiteral("/Game/Models/Batch/Tree2.glb");
    const QString batch3 = QStringLiteral("/Game/Models/Batch/Tree3.glb");
    check(ModelImportSidecar::WriteSidecar(fs, batch1, 0.1f, contentRoot), "WriteSidecar batch 1 succeeds");
    check(ModelImportSidecar::WriteSidecar(fs, batch2, 0.1f, contentRoot), "WriteSidecar batch 2 succeeds");
    check(ModelImportSidecar::WriteSidecar(fs, batch3, 0.1f, contentRoot), "WriteSidecar batch 3 succeeds");
    const QString sidecar1 = ModelImportSidecar::ResolveSidecarPath(batch1, projectRoot, contentRoot);
    const QString sidecar2 = ModelImportSidecar::ResolveSidecarPath(batch2, projectRoot, contentRoot);
    const QString sidecar3 = ModelImportSidecar::ResolveSidecarPath(batch3, projectRoot, contentRoot);
    check(fs.FileExists(sidecar1), "Batch sidecar 1 exists");
    check(fs.FileExists(sidecar2), "Batch sidecar 2 exists");
    check(fs.FileExists(sidecar3), "Batch sidecar 3 exists");
    check(sidecar1 != sidecar2 && sidecar2 != sidecar3, "All 3 batch sidecars have distinct paths");

    // 4. спавн читает scale 0.01
    ImportSettingsFile::LoadResult spawnRes;
    QString spawnErr;
    const float spawnScale = ModelImportSidecar::ReadSpawnScale(fs, heroVirtual, contentRoot, &spawnRes, &spawnErr);
    check(spawnRes == ImportSettingsFile::LoadResult::Success, "Spawn scale read succeeds");
    check(std::abs(spawnScale - 0.01f) < 1e-6f, "Spawn scale is 0.01f");

    // 5. нет sidecar даёт 1 (NotFound, log Info)
    ImportSettingsFile::LoadResult noSidecarRes;
    QString noSidecarErr;
    const float noSidecarScale = ModelImportSidecar::ReadSpawnScale(fs, QStringLiteral("/Game/Models/NoSidecar/Chair.glb"), contentRoot, &noSidecarRes, &noSidecarErr);
    check(noSidecarRes == ImportSettingsFile::LoadResult::NotFound, "No sidecar returns NotFound");
    check(std::abs(noSidecarScale - 1.0f) < 1e-6f, "No sidecar defaults scale to 1.0f");

    // 6. битый sidecar даёт 1 и Error
    const QString brokenVirtual = QStringLiteral("/Game/Models/Broken/Broken.glb");
    const QString brokenSidecar = ModelImportSidecar::ResolveSidecarPath(brokenVirtual, projectRoot, contentRoot);
    fs.WriteFileAtomic(brokenSidecar, QByteArray("not a json {{{"));
    ImportSettingsFile::LoadResult brokenRes;
    QString brokenSidecarErr;
    const float brokenScale = ModelImportSidecar::ReadSpawnScale(fs, brokenVirtual, contentRoot, &brokenRes, &brokenSidecarErr);
    check(brokenRes == ImportSettingsFile::LoadResult::Error, "Broken sidecar returns Error");
    check(std::abs(brokenScale - 1.0f) < 1e-6f, "Broken sidecar defaults scale to 1.0f");
    check(!brokenSidecarErr.isEmpty(), "Broken sidecar produces error message");

    // 7. путь вне корня проекта отвергается
    const QString outsideVirtual = QStringLiteral("/Game/../../outside.glb");
    QString outsideErr;
    const bool outsideWrite = ModelImportSidecar::WriteSidecar(fs, outsideVirtual, 1.0f, contentRoot, &outsideErr);
    check(!outsideWrite, "Writing sidecar outside sandbox is rejected");
    check(!outsideErr.isEmpty(), "Error populated for outside sandbox write");

    ImportSettingsFile::LoadResult outsideReadRes;
    QString outsideReadErr;
    const float outsideScale = ModelImportSidecar::ReadSpawnScale(fs, QStringLiteral("C:/Outside/Other.glb"), contentRoot, &outsideReadRes, &outsideReadErr);
    check(outsideReadRes == ImportSettingsFile::LoadResult::Error, "Reading sidecar outside sandbox returns Error");
    check(std::abs(outsideScale - 1.0f) < 1e-6f, "Outside sandbox defaults scale to 1.0f");

    outChecks += checks;
}

} // namespace

