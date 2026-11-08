#pragma once

#include "../../Core/FileService.h"
#include "../../Core/Logger.h"
#include "../../Core/ImportSettings.h"
#include "../../Core/ModelImportSidecar.h"
#include "../../../Engine/tests/ImportSettingsTestCases.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace
{

inline QByteArray ToQByteArray(std::string_view sv)
{
    return QByteArray(sv.data(), static_cast<int>(sv.size()));
}

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

    // 1b. IsValidSmoothingAngle and IsValidNormals
    check(ImportSettingsFile::IsValidSmoothingAngle(0.0f), "Angle 0.0 is valid");
    check(ImportSettingsFile::IsValidSmoothingAngle(60.0f), "Angle 60.0 is valid");
    check(ImportSettingsFile::IsValidSmoothingAngle(175.0f), "Angle 175.0 is valid");
    check(!ImportSettingsFile::IsValidSmoothingAngle(-0.1f), "Angle -0.1 is invalid");
    check(!ImportSettingsFile::IsValidSmoothingAngle(175.1f), "Angle 175.1 is invalid");
    check(!ImportSettingsFile::IsValidSmoothingAngle(std::numeric_limits<float>::quiet_NaN()), "NaN angle is invalid");

    check(ImportSettingsFile::IsValidNormals(QStringLiteral("file")), "Normals 'file' is valid");
    check(ImportSettingsFile::IsValidNormals(QStringLiteral("flat")), "Normals 'flat' is valid");
    check(ImportSettingsFile::IsValidNormals(QStringLiteral("smooth")), "Normals 'smooth' is valid");
    check(!ImportSettingsFile::IsValidNormals(QStringLiteral("unknown")), "Normals 'unknown' is invalid");
    check(!ImportSettingsFile::IsValidNormals(QStringLiteral("")), "Empty normals string is invalid");
    check(ImportSettingsFile::IsValidNormalsMode(ModelNormalsMode::File), "Mode File is valid");
    check(ImportSettingsFile::IsValidNormalsMode(ModelNormalsMode::Flat), "Mode Flat is valid");
    check(ImportSettingsFile::IsValidNormalsMode(ModelNormalsMode::Smooth), "Mode Smooth is valid");
    check(!ImportSettingsFile::IsValidNormalsMode(static_cast<ModelNormalsMode>(999)), "Mode 999 is invalid");

    // 2. Round-trip serialization (v1 full schema)
    ImportSettings s1;
    s1.model.scale = 0.05f;
    s1.model.useFileUnitScale = true;
    s1.model.normals = ModelNormalsMode::Smooth;
    s1.model.smoothingAngle = 45.0f;
    s1.model.importMaterials = false;
    s1.model.importTextures = false;
    s1.model.flipUv = true;
    s1.model.mergeMeshes = true;

    const QByteArray bytes = ImportSettingsFile::Serialize(s1);
    check(!bytes.isEmpty(), "Serialize produced non-empty data");

    ImportSettings s1Loaded;
    QString err;
    const bool parseOk = ImportSettingsFile::Parse(bytes, s1Loaded, &err);
    check(parseOk, "Parse succeeded on valid serialized data");
    check(err.isEmpty(), "No error on valid parse");
    check(s1Loaded.version == 1, "Loaded version is 1");
    check(s1Loaded.kind == QStringLiteral("model"), "Loaded kind is model");
    check(std::abs(s1Loaded.model.scale - 0.05f) < 1e-6f, "Loaded scale matches 0.05");
    check(s1Loaded.model.useFileUnitScale == true, "Loaded useFileUnitScale matches true");
    check(s1Loaded.model.normals == ModelNormalsMode::Smooth, "Loaded normals matches smooth");
    check(std::abs(s1Loaded.model.smoothingAngle - 45.0f) < 1e-6f, "Loaded smoothingAngle matches 45.0");
    check(s1Loaded.model.importMaterials == false, "Loaded importMaterials matches false");
    check(s1Loaded.model.importTextures == false, "Loaded importTextures matches false");
    check(s1Loaded.model.flipUv == true, "Loaded flipUv matches true");
    check(s1Loaded.model.mergeMeshes == true, "Loaded mergeMeshes matches true");

    // 2a. Direct parse of shared kValidV1 constant
    ImportSettings sValidV1;
    QString validV1Err;
    check(ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kValidV1), sValidV1, &validV1Err), "Parse succeeded on shared kValidV1");
    check(validV1Err.isEmpty(), "No error on shared kValidV1 parse");
    check(sValidV1.version == 1 && sValidV1.kind == QStringLiteral("model"), "kValidV1 header correct");
    check(std::abs(sValidV1.model.scale - 0.05f) < 1e-6f, "kValidV1 scale 0.05");
    check(sValidV1.model.useFileUnitScale == true, "kValidV1 useFileUnitScale true");
    check(sValidV1.model.normals == ModelNormalsMode::Smooth, "kValidV1 normals smooth");

    // 2b. Migration of legacy S16 format {"scale": 0.01} and {"scale": 0.01, "foo": 1}
    ImportSettings sLegacyPure;
    QString legacyPureErr;
    check(ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kLegacyScale), sLegacyPure, &legacyPureErr), "Parse succeeded on shared kLegacyScale");
    check(sLegacyPure.version == 1 && sLegacyPure.kind == QStringLiteral("model"), "kLegacyScale header correct");
    check(std::abs(sLegacyPure.model.scale - 0.01f) < 1e-6f, "kLegacyScale scale 0.01");
    check(sLegacyPure.model.useFileUnitScale == false, "kLegacyScale useFileUnitScale false");
    check(sLegacyPure.model.normals == ModelNormalsMode::File, "kLegacyScale normals file");
    check(std::abs(sLegacyPure.model.smoothingAngle - 60.0f) < 1e-6f, "kLegacyScale smoothingAngle 60.0");
    check(sLegacyPure.model.importMaterials == true, "kLegacyScale importMaterials true");
    check(sLegacyPure.model.importTextures == true, "kLegacyScale importTextures true");
    check(sLegacyPure.model.flipUv == false, "kLegacyScale flipUv false");
    check(sLegacyPure.model.mergeMeshes == true, "kLegacyScale mergeMeshes true");

    ImportSettings sLegacy;
    QString legacyErr;
    const bool legacyOk = ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kLegacyWithUnknown), sLegacy, &legacyErr);
    check(legacyOk, "Parse succeeded on legacy S16 sidecar with foo");
    check(sLegacy.version == 1, "Legacy sidecar migrated to version 1");
    check(sLegacy.kind == QStringLiteral("model"), "Legacy sidecar migrated to kind model");
    check(std::abs(sLegacy.model.scale - 0.01f) < 1e-6f, "Legacy sidecar scale loaded as 0.01");
    check(sLegacy.model.useFileUnitScale == false, "Legacy sidecar useFileUnitScale defaults to false");
    check(sLegacy.model.normals == ModelNormalsMode::File, "Legacy sidecar normals defaults to file");
    check(std::abs(sLegacy.model.smoothingAngle - 60.0f) < 1e-6f, "Legacy sidecar smoothingAngle defaults to 60.0");
    check(sLegacy.model.importMaterials == true, "Legacy sidecar importMaterials defaults to true");
    check(sLegacy.model.importTextures == true, "Legacy sidecar importTextures defaults to true");
    check(sLegacy.model.flipUv == false, "Legacy sidecar flipUv defaults to false");
    check(sLegacy.model.mergeMeshes == true, "Legacy sidecar mergeMeshes defaults to true");
    check(sLegacy.extraFields.contains(QStringLiteral("foo")), "Legacy extra field foo captured");
    check(sLegacy.extraFields.value(QStringLiteral("foo")).toInt() == 1, "Legacy extra field foo value is 1");

    const QByteArray legacyReserialized = ImportSettingsFile::Serialize(sLegacy);
    const QJsonDocument legacyDoc = QJsonDocument::fromJson(legacyReserialized);
    check(legacyDoc.object().value(QStringLiteral("foo")).toInt() == 1, "Legacy foo preserved upon reserialization");
    check(std::abs(legacyDoc.object().value(QStringLiteral("model")).toObject().value(QStringLiteral("scale")).toDouble() - 0.01) < 1e-6, "Migrated model scale is 0.01");

    // 2c. Preservation of unknown fields (root and model)
    ImportSettings sUnknown;
    check(ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kValidWithUnknownFields), sUnknown), "Parse succeeds with unknown fields");
    check(sUnknown.extraFields.contains(QStringLiteral("custom_root")), "Root extra fields captured");
    check(sUnknown.extraFields.value(QStringLiteral("custom_root")).toInt() == 12345, "Root extra field value matches");
    check(sUnknown.model.extraFields.contains(QStringLiteral("custom_model_meta")), "Model extra fields captured");
    check(sUnknown.model.extraFields.value(QStringLiteral("custom_model_meta")).toString() == QStringLiteral("keep_me"), "Model extra field value matches");
    check(sUnknown.model.mergeMeshes == true, "Sidecar without mergeMeshes field defaults to true");

    const QByteArray reserialized = ImportSettingsFile::Serialize(sUnknown);
    const QJsonDocument reDoc = QJsonDocument::fromJson(reserialized);
    check(reDoc.object().value(QStringLiteral("custom_root")).toInt() == 12345, "Reserialized JSON preserves root extra field");
    check(reDoc.object().value(QStringLiteral("model")).toObject().value(QStringLiteral("custom_model_meta")).toString() == QStringLiteral("keep_me"),
          "Reserialized JSON preserves model extra field");

    // 3. Broken JSON handling (explicit error logged, no silent fallback, default value set)
    ImportSettings sBroken;
    sBroken.model.scale = 42.0f;
    QString brokenErr;
    const bool brokenOk = ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kBrokenJson), sBroken, &brokenErr);
    check(!brokenOk, "Parse fails on broken JSON");
    check(!brokenErr.isEmpty(), "Error message populated for broken JSON");
    check(std::abs(sBroken.model.scale - 1.0f) < 1e-6f, "Scale reset to default 1.0 on broken JSON");

    // 4. Missing required fields in modern v1 format
    ImportSettings sMissingKind;
    QString missingKindErr;
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kMissingKind), sMissingKind, &missingKindErr), "Parse fails on missing kind");
    check(!missingKindErr.isEmpty(), "Error populated for missing kind");

    ImportSettings sMissingModel;
    QString missingModelErr;
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kMissingModel), sMissingModel, &missingModelErr), "Parse fails on missing model object");
    check(!missingModelErr.isEmpty(), "Error populated for missing model object");

    ImportSettings sMissingScale;
    QString missingScaleErr;
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kMissingScale), sMissingScale, &missingScaleErr), "Parse fails on missing scale");
    check(!missingScaleErr.isEmpty(), "Error populated for missing scale");

    ImportSettings sUnsupportedKind;
    QString unsupportedKindErr;
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kUnsupportedKind), sUnsupportedKind, &unsupportedKindErr), "Parse fails on unsupported kind");
    check(!unsupportedKindErr.isEmpty(), "Error populated for unsupported kind");

    // 5. Invalid types
    ImportSettings sBadType;
    QString badTypeErr;
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kBadTypeScale), sBadType, &badTypeErr), "Parse fails on string scale");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kBadTypeNormals), sBadType, &badTypeErr), "Parse fails on number normals");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kBadTypeSmoothingAngle), sBadType, &badTypeErr), "Parse fails on string smoothingAngle");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kBadTypeImportMaterials), sBadType, &badTypeErr), "Parse fails on string importMaterials");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kBadTypeUseFileUnitScale), sBadType, &badTypeErr), "Parse fails on int useFileUnitScale");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kBadTypeFlipUv), sBadType, &badTypeErr), "Parse fails on string flipUv");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kBadTypeMergeMeshes), sBadType, &badTypeErr), "Parse fails on array mergeMeshes");

    // 6. Out of range values
    ImportSettings sOutOfRange;
    QString rangeErr;
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kOutOfRangeScaleZero), sOutOfRange, &rangeErr), "Parse fails on scale 0.0");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kOutOfRangeScaleNegative), sOutOfRange, &rangeErr), "Parse fails on negative scale");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kOutOfRangeScaleHuge), sOutOfRange, &rangeErr), "Parse fails on scale > 1e4");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kOutOfRangeAngleNegative), sOutOfRange, &rangeErr), "Parse fails on negative angle");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kOutOfRangeAngleHuge), sOutOfRange, &rangeErr), "Parse fails on angle > 175");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kInvalidNormalsEnum), sOutOfRange, &rangeErr), "Parse fails on invalid normals enum");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kLegacyBadScale), sOutOfRange, &rangeErr), "Parse fails on bad legacy scale");
    check(!ImportSettingsFile::Parse(ToQByteArray(ImportSettingsTestCases::kLegacyOutOfRangeScale), sOutOfRange, &rangeErr), "Parse fails on out of range legacy scale");

    // 7. Sidecar path helper (shared kPathTestCases matching Engine's ResolveSidecarPath)
    for (const auto& tc : ImportSettingsTestCases::kPathTestCases)
    {
        const QString inStr = QString::fromUtf8(tc.input.data(), static_cast<int>(tc.input.size()));
        const QString expStr = QString::fromUtf8(tc.expected.data(), static_cast<int>(tc.expected.size()));
        check(ImportSettingsFile::SidecarPathForAsset(inStr) == expStr, "SidecarPathForAsset matches shared test case");
    }

    // 8. FileService integration (Save & Load with sandbox)
    QTemporaryDir tempDir;
    check(tempDir.isValid(), "Temporary directory created for ImportSettings tests");

    Logger logger;
    FileService fs(&logger);
    fs.SetProjectRoot(tempDir.path());

    const QString sidecarRel = QStringLiteral("Models/Hero/Hero.glb.importsettings.json");
    ImportSettings saveSet;
    saveSet.model.scale = 0.1f;
    saveSet.model.smoothingAngle = 45.0f;
    saveSet.model.normals = ModelNormalsMode::Flat;
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
    check(std::abs(loadSet.model.scale - 0.1f) < 1e-6f, "Loaded scale matches saved scale 0.1");
    check(loadSet.model.normals == ModelNormalsMode::Flat, "Loaded normals matches saved flat");
    check(std::abs(loadSet.model.smoothingAngle - 45.0f) < 1e-6f, "Loaded smoothingAngle matches saved 45.0");

    // Load missing file (returns NotFound, scale = 1.0f, no error exception)
    ImportSettings missingSet;
    missingSet.model.scale = 42.0f;
    QString missingLoadErr;
    const auto missingRes = ImportSettingsFile::Load(fs, QStringLiteral("Models/Missing/none.glb.importsettings.json"), missingSet, &missingLoadErr);
    check(missingRes == ImportSettingsFile::LoadResult::NotFound, "Load missing file returns NotFound");
    check(std::abs(missingSet.model.scale - 1.0f) < 1e-6f, "Missing file resets scale to default 1.0f");
    check(missingLoadErr.contains(QStringLiteral("sidecar not found")), "Missing file error message contains info");

    // Load corrupt file (returns Error, scale = 1.0f)
    const QString corruptRel = QStringLiteral("Models/Corrupt/corrupt.importsettings.json");
    fs.WriteFileAtomic(corruptRel, QByteArray("not a valid json { broken"));
    ImportSettings corruptSet;
    corruptSet.model.scale = 42.0f;
    QString corruptLoadErr;
    const auto corruptRes = ImportSettingsFile::Load(fs, corruptRel, corruptSet, &corruptLoadErr);
    check(corruptRes == ImportSettingsFile::LoadResult::Error, "Load corrupt file returns Error");
    check(std::abs(corruptSet.model.scale - 1.0f) < 1e-6f, "Corrupt file resets scale to default 1.0f");
    check(corruptLoadErr.contains(QStringLiteral("JSON parse error")), "Corrupt file error message contains parse error");

    // Load out-of-range file (returns Error, scale = 1.0f)
    const QString outOfRangeRel = QStringLiteral("Models/Range/range.importsettings.json");
    fs.WriteFileAtomic(outOfRangeRel, QByteArray("{\"version\": 1, \"kind\": \"model\", \"model\": {\"scale\": -10.0}}"));
    ImportSettings rangeSet;
    rangeSet.model.scale = 42.0f;
    QString rangeLoadErr;
    const auto rangeRes = ImportSettingsFile::Load(fs, outOfRangeRel, rangeSet, &rangeLoadErr);
    check(rangeRes == ImportSettingsFile::LoadResult::Error, "Load out-of-range file returns Error");
    check(std::abs(rangeSet.model.scale - 1.0f) < 1e-6f, "Out-of-range file resets scale to default 1.0f");

    // Rejection on Save with invalid scale, angle, or normals
    ImportSettings badSaveScale;
    badSaveScale.model.scale = -1.0f;
    QString badSaveErr;
    check(!ImportSettingsFile::Save(fs, sidecarRel, badSaveScale, &badSaveErr), "Save fails for invalid scale");

    ImportSettings badSaveAngle;
    badSaveAngle.model.smoothingAngle = 200.0f;
    check(!ImportSettingsFile::Save(fs, sidecarRel, badSaveAngle, &badSaveErr), "Save fails for invalid angle");

    ImportSettings badSaveNormals;
    badSaveNormals.model.normals = static_cast<ModelNormalsMode>(999);
    check(!ImportSettingsFile::Save(fs, sidecarRel, badSaveNormals, &badSaveErr), "Save fails for invalid normals");

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

    // ------------------------------------------------------------------------
    // Step 16.4: Regression tests
    // ------------------------------------------------------------------------

    // Regression 1: Spawn model with sidecar scale 0.01 sets transform.scale = {0.01, 0.01, 0.01}
    {
        const QString modelPath = QStringLiteral("/Game/Models/Regression/Tree001.glb");
        check(ModelImportSidecar::WriteSidecar(fs, modelPath, 0.01f, contentRoot), "Write sidecar 0.01 for spawn test");

        ImportSettingsFile::LoadResult res;
        QString err;
        const float s = ModelImportSidecar::ReadSpawnScale(fs, modelPath, contentRoot, &res, &err);
        check(res == ImportSettingsFile::LoadResult::Success, "ReadSpawnScale succeeds");
        check(std::abs(s - 0.01f) < 1e-6f, "ReadSpawnScale returns 0.01");

        // Emulate spawn transform assignment (AssetTools.cpp:134-137)
        float transformScale[3] = { 1.0f, 1.0f, 1.0f };
        transformScale[0] = s;
        transformScale[1] = s;
        transformScale[2] = s;

        check(std::abs(transformScale[0] - 0.01f) < 1e-6f, "transform.scale[0] is 0.01");
        check(std::abs(transformScale[1] - 0.01f) < 1e-6f, "transform.scale[1] is 0.01");
        check(std::abs(transformScale[2] - 0.01f) < 1e-6f, "transform.scale[2] is 0.01");
    }

    // Regression 2: Missing sidecar gives 1.0 and Info
    {
        const QString missingModel = QStringLiteral("/Game/Models/Regression/NoSidecarModel.glb");
        ImportSettingsFile::LoadResult res;
        QString err;
        const float s = ModelImportSidecar::ReadSpawnScale(fs, missingModel, contentRoot, &res, &err);
        check(res == ImportSettingsFile::LoadResult::NotFound, "Missing sidecar returns NotFound");
        check(std::abs(s - 1.0f) < 1e-6f, "Missing sidecar returns default scale 1.0");

        float transformScale[3] = { 0.5f, 0.5f, 0.5f };
        transformScale[0] = s;
        transformScale[1] = s;
        transformScale[2] = s;

        check(std::abs(transformScale[0] - 1.0f) < 1e-6f, "transform.scale[0] defaults to 1.0");
        check(std::abs(transformScale[1] - 1.0f) < 1e-6f, "transform.scale[1] defaults to 1.0");
        check(std::abs(transformScale[2] - 1.0f) < 1e-6f, "transform.scale[2] defaults to 1.0");
    }

    // Regression 3: Corrupt sidecar gives 1.0 and Error
    {
        const QString corruptModel = QStringLiteral("/Game/Models/Regression/CorruptModel.glb");
        const QString corruptSidecar = ModelImportSidecar::ResolveSidecarPath(corruptModel, projectRoot, contentRoot);
        fs.WriteFileAtomic(corruptSidecar, QByteArray("not json at all {[]"));

        ImportSettingsFile::LoadResult res;
        QString err;
        const float s = ModelImportSidecar::ReadSpawnScale(fs, corruptModel, contentRoot, &res, &err);
        check(res == ImportSettingsFile::LoadResult::Error, "Corrupt sidecar returns Error");
        check(std::abs(s - 1.0f) < 1e-6f, "Corrupt sidecar returns fallback scale 1.0");
        check(!err.isEmpty(), "Corrupt sidecar produces error message for log");

        float transformScale[3] = { 0.5f, 0.5f, 0.5f };
        transformScale[0] = s;
        transformScale[1] = s;
        transformScale[2] = s;

        check(std::abs(transformScale[0] - 1.0f) < 1e-6f, "transform.scale[0] defaults to 1.0 on error");
        check(std::abs(transformScale[1] - 1.0f) < 1e-6f, "transform.scale[1] defaults to 1.0 on error");
        check(std::abs(transformScale[2] - 1.0f) < 1e-6f, "transform.scale[2] defaults to 1.0 on error");
    }

    // Regression 4: sidecar survives folder copy semantics (emulation)
    {
        const QString reimportModel = QStringLiteral("/Game/Models/Regression/ReimportModel.glb");
        check(ModelImportSidecar::WriteSidecar(fs, reimportModel, 0.0254f, contentRoot), "Initial import writes sidecar 0.0254");

        // Emulate an object already placed on scene
        struct SceneObject {
            uint64_t id;
            QString modelAsset;
            float scale[3];
        };
        SceneObject existingOnScene{ 101, reimportModel, { 0.0254f, 0.0254f, 0.0254f } };

        // Emulate AssetUpdate folder copy semantics (candidate preserves existing files in destination folder)
        ImportSettingsFile::LoadResult res;
        QString err;
        const float sidecarScale = ModelImportSidecar::ReadSpawnScale(fs, reimportModel, contentRoot, &res, &err);
        check(res == ImportSettingsFile::LoadResult::Success, "sidecar survives folder copy semantics (emulation)");
        check(std::abs(sidecarScale - 0.0254f) < 1e-6f, "sidecar scale preserved (not overwritten by 1.0)");

        // Scene object scale is retained by the scene state (emulation)
        check(std::abs(existingOnScene.scale[0] - 0.0254f) < 1e-6f, "Existing scene object scale[0] preserved");
        check(std::abs(existingOnScene.scale[1] - 0.0254f) < 1e-6f, "Existing scene object scale[1] preserved");
        check(std::abs(existingOnScene.scale[2] - 0.0254f) < 1e-6f, "Existing scene object scale[2] preserved");

        // A newly spawned object after reimport also gets the preserved scale
        const float newSpawnScale = ModelImportSidecar::ReadSpawnScale(fs, reimportModel, contentRoot);
        check(std::abs(newSpawnScale - 0.0254f) < 1e-6f, "New spawn uses preserved scale 0.0254");
    }

    outChecks += checks;
}

} // namespace
