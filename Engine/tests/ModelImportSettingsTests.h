#pragma once

#include "../Source/Scene/ModelImportSettings.h"
#include "ImportSettingsTestCases.h"
#include "../Source/Resources/VfsRead.h"

#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace ModelImportSettingsTests
{

inline void CheckCond(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

inline void Run()
{
    using namespace ImportSettingsTestCases;
    auto check = [](bool cond, const char* msg) {
        CheckCond(cond, msg);
    };

    // 1. Validation helpers
    check(ModelImportSettings::IsValidScale(1.0f), "Scale 1.0 is valid");
    check(ModelImportSettings::IsValidScale(0.1f), "Scale 0.1 is valid");
    check(ModelImportSettings::IsValidScale(0.01f), "Scale 0.01 is valid");
    check(ModelImportSettings::IsValidScale(1e-4f), "Scale 1e-4 is valid");
    check(ModelImportSettings::IsValidScale(1e4f), "Scale 1e4 is valid");
    check(!ModelImportSettings::IsValidScale(0.0f), "Scale 0.0 is invalid");
    check(!ModelImportSettings::IsValidScale(-1.0f), "Scale -1.0 is invalid");
    check(!ModelImportSettings::IsValidScale(1e-5f), "Scale < 1e-4 is invalid");
    check(!ModelImportSettings::IsValidScale(1e5f), "Scale > 1e4 is invalid");

    check(ModelImportSettings::IsValidSmoothingAngle(0.0f), "Angle 0.0 is valid");
    check(ModelImportSettings::IsValidSmoothingAngle(60.0f), "Angle 60.0 is valid");
    check(ModelImportSettings::IsValidSmoothingAngle(175.0f), "Angle 175.0 is valid");
    check(!ModelImportSettings::IsValidSmoothingAngle(-0.1f), "Angle -0.1 is invalid");
    check(!ModelImportSettings::IsValidSmoothingAngle(175.1f), "Angle 175.1 is invalid");

    check(ModelImportSettings::IsValidNormals("file"), "Normals 'file' is valid");
    check(ModelImportSettings::IsValidNormals("flat"), "Normals 'flat' is valid");
    check(ModelImportSettings::IsValidNormals("smooth"), "Normals 'smooth' is valid");
    check(!ModelImportSettings::IsValidNormals("unknown"), "Normals 'unknown' is invalid");
    check(!ModelImportSettings::IsValidNormals(""), "Empty normals is invalid");

    // 2. Valid v1 full schema
    {
        ModelImportSettings s;
        std::string err;
        check(ModelImportSettings::Parse(kValidV1, s, &err), "Parse succeeded on valid v1");
        check(err.empty(), "No error on valid parse");
        check(s.version == 1, "Version is 1");
        check(s.kind == "model", "Kind is model");
        check(std::abs(s.scale - 0.05f) < 1e-6f, "Scale matches 0.05");
        check(s.useFileUnitScale == true, "useFileUnitScale is true");
        check(s.normals == "smooth", "Normals is smooth");
        check(std::abs(s.smoothingAngle - 45.0f) < 1e-6f, "Smoothing angle is 45.0");
        check(s.importMaterials == false, "importMaterials is false");
        check(s.importTextures == false, "importTextures is false");
        check(s.flipUv == true, "flipUv is true");
        check(s.mergeMeshes == true, "mergeMeshes is true");
    }

    // 3. Legacy migration: {"scale": 0.01}
    {
        ModelImportSettings s;
        std::string err;
        check(ModelImportSettings::Parse(kLegacyScale, s, &err), "Parse legacy scale succeeded");
        check(s.version == 1, "Legacy version migrated to 1");
        check(s.kind == "model", "Legacy kind migrated to model");
        check(std::abs(s.scale - 0.01f) < 1e-6f, "Legacy scale is 0.01");
        check(s.useFileUnitScale == false, "Default useFileUnitScale is false");
        check(s.normals == "file", "Default normals is file");
        check(std::abs(s.smoothingAngle - 60.0f) < 1e-6f, "Default smoothingAngle is 60.0");
        check(s.importMaterials == true, "Default importMaterials is true");
        check(s.importTextures == true, "Default importTextures is true");
        check(s.flipUv == false, "Default flipUv is false");
        check(s.mergeMeshes == false, "Default mergeMeshes is false");
    }

    // 4. Legacy with unknown fields: {"scale": 0.01, "foo": 1}
    {
        ModelImportSettings s;
        std::string err;
        check(ModelImportSettings::Parse(kLegacyWithUnknown, s, &err), "Parse legacy with unknown succeeded");
        check(std::abs(s.scale - 0.01f) < 1e-6f, "Legacy with unknown scale is 0.01");
    }

    // 5. Valid v1 with unknown fields
    {
        ModelImportSettings s;
        std::string err;
        check(ModelImportSettings::Parse(kValidWithUnknownFields, s, &err), "Parse v1 with unknown fields succeeded");
        check(std::abs(s.scale - 2.0f) < 1e-6f, "Scale is 2.0");
    }

    // 6. Broken JSON
    {
        ModelImportSettings s;
        s.scale = 42.0f;
        std::string err;
        check(!ModelImportSettings::Parse(kBrokenJson, s, &err), "Parse failed on broken JSON");
        check(!err.empty(), "Error message set on broken JSON");
        check(std::abs(s.scale - 1.0f) < 1e-6f, "Scale reset to default 1.0 on broken JSON");
    }

    // 7. Missing required fields
    {
        ModelImportSettings s;
        std::string err;
        check(!ModelImportSettings::Parse(kMissingKind, s, &err), "Parse failed on missing kind");
        check(!ModelImportSettings::Parse(kMissingModel, s, &err), "Parse failed on missing model object");
        check(!ModelImportSettings::Parse(kMissingScale, s, &err), "Parse failed on missing scale");
        check(!ModelImportSettings::Parse(kUnsupportedKind, s, &err), "Parse failed on unsupported kind");
    }

    // 8. Bad types
    {
        ModelImportSettings s;
        std::string err;
        check(!ModelImportSettings::Parse(kBadTypeScale, s, &err), "Parse failed on bad scale type");
        check(!ModelImportSettings::Parse(kBadTypeNormals, s, &err), "Parse failed on bad normals type");
        check(!ModelImportSettings::Parse(kBadTypeSmoothingAngle, s, &err), "Parse failed on bad smoothingAngle type");
        check(!ModelImportSettings::Parse(kBadTypeImportMaterials, s, &err), "Parse failed on bad importMaterials type");
        check(!ModelImportSettings::Parse(kBadTypeUseFileUnitScale, s, &err), "Parse failed on bad useFileUnitScale type");
        check(!ModelImportSettings::Parse(kBadTypeFlipUv, s, &err), "Parse failed on bad flipUv type");
        check(!ModelImportSettings::Parse(kBadTypeMergeMeshes, s, &err), "Parse failed on bad mergeMeshes type");
    }

    // 9. Out of range values
    {
        ModelImportSettings s;
        std::string err;
        check(!ModelImportSettings::Parse(kOutOfRangeScaleZero, s, &err), "Parse failed on scale 0.0");
        check(!ModelImportSettings::Parse(kOutOfRangeScaleNegative, s, &err), "Parse failed on negative scale");
        check(!ModelImportSettings::Parse(kOutOfRangeScaleHuge, s, &err), "Parse failed on scale > 1e4");
        check(!ModelImportSettings::Parse(kOutOfRangeAngleNegative, s, &err), "Parse failed on negative angle");
        check(!ModelImportSettings::Parse(kOutOfRangeAngleHuge, s, &err), "Parse failed on angle > 175");
        check(!ModelImportSettings::Parse(kInvalidNormalsEnum, s, &err), "Parse failed on invalid normals enum");
        check(!ModelImportSettings::Parse(kLegacyBadScale, s, &err), "Parse failed on bad legacy scale");
        check(!ModelImportSettings::Parse(kLegacyOutOfRangeScale, s, &err), "Parse failed on out of range legacy scale");
    }

    // 10. Path resolution helper
    {
        check(ModelImportSettings::ResolveSidecarPath("/Game/Models/Hero/Hero.glb") == "/Game/Models/Hero/Hero.glb.importsettings.json", "ResolveSidecarPath appends suffix");
        check(ModelImportSettings::ResolveSidecarPath("/Game/Models/Hero/Hero.glb.importsettings.json") == "/Game/Models/Hero/Hero.glb.importsettings.json", "ResolveSidecarPath idempotent on existing suffix");
        check(ModelImportSettings::ResolveSidecarPath("").empty(), "ResolveSidecarPath empty for empty path");
    }

    // 11. VFS integration: read non-existent, read valid, read broken
    {
        // 11a. Missing sidecar -> defaults without error
        const ModelImportSettings nonExistent = ModelImportSettings::ReadFromVfs("/Game/Nonexistent/Model.fbx");
        check(std::abs(nonExistent.scale - 1.0f) < 1e-6f, "Nonexistent sidecar returns default scale 1.0");
        check(nonExistent.normals == "file", "Nonexistent sidecar returns default normals 'file'");

        // 11b. Mount temporary directory to VFS and test live read
        const auto tempRoot = std::filesystem::temp_directory_path() / ("EngineImportTest_" + std::to_string(std::clock()));
        std::filesystem::create_directories(tempRoot);

        try
        {
            VfsRead::GetVFS().Mount("/TestImportVfs", tempRoot);

            // Write valid sidecar
            {
                std::ofstream out(tempRoot / "model.glb.importsettings.json");
                out << kValidV1;
            }

            const ModelImportSettings loaded = ModelImportSettings::ReadFromVfs("/TestImportVfs/model.glb");
            check(std::abs(loaded.scale - 0.05f) < 1e-6f, "VFS loaded valid scale 0.05");
            check(loaded.useFileUnitScale == true, "VFS loaded useFileUnitScale true");
            check(loaded.normals == "smooth", "VFS loaded normals smooth");

            // Write broken sidecar
            {
                std::ofstream out(tempRoot / "broken.glb.importsettings.json");
                out << "corrupt { json";
            }

            const ModelImportSettings broken = ModelImportSettings::ReadFromVfs("/TestImportVfs/broken.glb");
            check(std::abs(broken.scale - 1.0f) < 1e-6f, "VFS broken sidecar falls back to default 1.0");

            VfsRead::GetVFS().Unmount("/TestImportVfs");
            std::filesystem::remove_all(tempRoot);
        }
        catch (...)
        {
            VfsRead::GetVFS().Unmount("/TestImportVfs");
            std::filesystem::remove_all(tempRoot);
            throw;
        }
    }
}

} // namespace ModelImportSettingsTests

