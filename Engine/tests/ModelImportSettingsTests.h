#pragma once

#include "../Source/Scene/ModelImportSettings.h"
#include "ImportSettingsTestCases.h"
#include "../Source/Resources/VfsRead.h"
#include <Engine/Render/SkeletalLoader.h>
#include <Engine/Render/SkeletalData.h>

#include <cmath>
#include <cstring>
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
        check(s.mergeMeshes == true, "Default mergeMeshes is true");
    }

    // 4. Legacy with unknown fields: {"scale": 0.01, "foo": 1}
    {
        ModelImportSettings s;
        std::string err;
        check(ModelImportSettings::Parse(kLegacyWithUnknown, s, &err), "Parse legacy with unknown succeeded");
        check(std::abs(s.scale - 0.01f) < 1e-6f, "Legacy with unknown scale is 0.01");
        check(s.mergeMeshes == true, "Legacy with unknown mergeMeshes defaults to true");
    }

    // 5. Valid v1 with unknown fields
    {
        ModelImportSettings s;
        std::string err;
        check(ModelImportSettings::Parse(kValidWithUnknownFields, s, &err), "Parse v1 with unknown fields succeeded");
        check(std::abs(s.scale - 2.0f) < 1e-6f, "Scale is 2.0");
        check(s.mergeMeshes == true, "v1 without mergeMeshes field defaults to true");
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

    // 10. Path resolution helper (shared kPathTestCases matching Editor's SidecarPathForAsset)
    for (const auto& tc : kPathTestCases)
    {
        check(ModelImportSettings::ResolveSidecarPath(tc.input) == tc.expected, "ResolveSidecarPath matches shared test case");
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

#if defined(ENGINE_ENABLE_ASSIMP)
    // 12. Assimp flags integration via VFS
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / ("EngineAssimpFlagsTest_" + std::to_string(std::clock()));
        std::filesystem::create_directories(tempRoot);

        try
        {
            VfsRead::GetVFS().Mount("/TestFlagsVfs", tempRoot);

            const char* kTestObj =
                "o MeshA\n"
                "usemtl SharedMat\n"
                "v 0 0 0\n"
                "v 1 0 0\n"
                "v 0 1 0\n"
                "v 0 0 1\n"
                "vt 0.1 0.3\n"
                "vt 0.5 0.3\n"
                "vt 0.9 0.3\n"
                "vt 0.5 0.8\n"
                "f 1/1 2/2 3/3\n"
                "f 1/1 3/3 4/4\n"
                "o MeshB\n"
                "usemtl SharedMat\n"
                "v 2 0 0\n"
                "v 3 0 0\n"
                "v 2 1 0\n"
                "vt 0.1 0.3\n"
                "vt 0.5 0.3\n"
                "vt 0.1 0.8\n"
                "f 5/1 6/2 7/5\n";

            // Write test model
            {
                std::ofstream out(tempRoot / "box.obj");
                out << kTestObj;
            }

            // 12a. Absent sidecar: default flags (flipUv=false -> V ~ 0.7, mergeMeshes=true -> 1 mesh, 9 vertices)
            const SkeletalLoadResult resDefault = LoadSkeletalModelFromVfs("/TestFlagsVfs/box.obj");
            check(resDefault.Success, "Load default box.obj succeeded");
            check(resDefault.Model != nullptr, "resDefault Model is not null");
            check(resDefault.Model->Meshes.size() == 1, "Default sidecar merges 2 meshes into 1 (mergeMeshes=true by default)");
            check(resDefault.Model->Meshes[0].Vertices.size() == 9, "Default sidecar produces 9 vertices (identical to pre-ea10cf0 baseline)");
            check(resDefault.Model->TotalIndexCount == 9, "Total index count is 9 (6 from MeshA + 3 from MeshB)");
            check(std::abs(resDefault.Model->Meshes[0].Vertices[0].TexCoord.y - 0.7f) < 0.01f, "Default flipUv=false flips UV: 1.0 - 0.3 = 0.7");

            // 12b. Explicit default sidecar (mergeMeshes=true) must yield identical output to absent sidecar
            {
                std::ofstream out(tempRoot / "box.obj.importsettings.json");
                out << "{\"version\":1,\"kind\":\"model\",\"model\":{\"scale\":1.0,\"useFileUnitScale\":false,\"normals\":\"file\",\"smoothingAngle\":60.0,\"importMaterials\":true,\"importTextures\":true,\"flipUv\":false,\"mergeMeshes\":true}}";
            }
            const SkeletalLoadResult resExplicitDefault = LoadSkeletalModelFromVfs("/TestFlagsVfs/box.obj");
            check(resExplicitDefault.Success, "Load with explicit default sidecar succeeded");
            check(resExplicitDefault.Model->Meshes.size() == resDefault.Model->Meshes.size(), "Mesh count matches between absent and default sidecar (1)");
            check(resExplicitDefault.Model->Meshes[0].Vertices.size() == resDefault.Model->Meshes[0].Vertices.size(), "Vertex count matches (9)");
            check(std::memcmp(resExplicitDefault.Model->Meshes[0].Vertices.data(), resDefault.Model->Meshes[0].Vertices.data(),
                              resDefault.Model->Meshes[0].Vertices.size() * sizeof(SkinnedVertex)) == 0,
                  "Vertices bit-for-bit identical with default sidecar");

            // 12c. Sidecar without mergeMeshes field defaults to mergeMeshes=true (1 mesh, 9 vertices, memcmp)
            {
                std::ofstream out(tempRoot / "box.obj.importsettings.json");
                out << "{\"version\":1,\"kind\":\"model\",\"model\":{\"scale\":1.0}}";
            }
            const SkeletalLoadResult resNoMergeField = LoadSkeletalModelFromVfs("/TestFlagsVfs/box.obj");
            check(resNoMergeField.Success, "Load with sidecar missing mergeMeshes succeeded");
            check(resNoMergeField.Model->Meshes.size() == 1, "Sidecar without mergeMeshes yields 1 merged mesh");
            check(resNoMergeField.Model->Meshes[0].Vertices.size() == 9, "Sidecar without mergeMeshes yields 9 vertices");
            check(std::memcmp(resNoMergeField.Model->Meshes[0].Vertices.data(), resDefault.Model->Meshes[0].Vertices.data(),
                              resDefault.Model->Meshes[0].Vertices.size() * sizeof(SkinnedVertex)) == 0,
                  "Sidecar without mergeMeshes is bit-for-bit identical to default");

            // 12d. mergeMeshes=false: preserves 2 distinct meshes (KEEP_HIERARCHY=1)
            {
                std::ofstream out(tempRoot / "box.obj.importsettings.json");
                out << "{\"version\":1,\"kind\":\"model\",\"model\":{\"scale\":1.0,\"useFileUnitScale\":false,\"normals\":\"file\",\"smoothingAngle\":60.0,\"importMaterials\":true,\"importTextures\":true,\"flipUv\":false,\"mergeMeshes\":false}}";
            }
            const SkeletalLoadResult resSeparate = LoadSkeletalModelFromVfs("/TestFlagsVfs/box.obj");
            check(resSeparate.Success, "Load with mergeMeshes=false succeeded");
            check(resSeparate.Model->Meshes.size() == 2, "mergeMeshes=false preserves 2 distinct meshes");
            check(resSeparate.Model->Meshes[0].Vertices.size() == 6, "Mesh 0 has 6 vertices (MeshA)");
            check(resSeparate.Model->Meshes[1].Vertices.size() == 3, "Mesh 1 has 3 vertices (MeshB)");

            // 12e. flipUv=true: V coordinate is NOT flipped (V ~ 0.3)
            {
                std::ofstream out(tempRoot / "box.obj.importsettings.json");
                out << "{\"version\":1,\"kind\":\"model\",\"model\":{\"scale\":1.0,\"useFileUnitScale\":false,\"normals\":\"file\",\"smoothingAngle\":60.0,\"importMaterials\":true,\"importTextures\":true,\"flipUv\":true,\"mergeMeshes\":true}}";
            }
            const SkeletalLoadResult resFlipUv = LoadSkeletalModelFromVfs("/TestFlagsVfs/box.obj");
            check(resFlipUv.Success, "Load with flipUv=true succeeded");
            check(std::abs(resFlipUv.Model->Meshes[0].Vertices[0].TexCoord.y - 0.3f) < 0.01f, "flipUv=true keeps original V: 0.3");

            // 12f. normals="flat" vs normals="smooth"
            {
                std::ofstream out(tempRoot / "box.obj.importsettings.json");
                out << "{\"version\":1,\"kind\":\"model\",\"model\":{\"scale\":1.0,\"useFileUnitScale\":false,\"normals\":\"flat\",\"smoothingAngle\":60.0,\"importMaterials\":true,\"importTextures\":true,\"flipUv\":false,\"mergeMeshes\":true}}";
            }
            const SkeletalLoadResult resFlat = LoadSkeletalModelFromVfs("/TestFlagsVfs/box.obj");
            check(resFlat.Success, "Load with normals='flat' succeeded");

            {
                std::ofstream out(tempRoot / "box.obj.importsettings.json");
                out << "{\"version\":1,\"kind\":\"model\",\"model\":{\"scale\":1.0,\"useFileUnitScale\":false,\"normals\":\"smooth\",\"smoothingAngle\":100.0,\"importMaterials\":true,\"importTextures\":true,\"flipUv\":false,\"mergeMeshes\":true}}";
            }
            const SkeletalLoadResult resSmooth = LoadSkeletalModelFromVfs("/TestFlagsVfs/box.obj");
            check(resSmooth.Success, "Load with normals='smooth' succeeded");

            check(resFlat.Model->Meshes[0].Vertices.size() > resSmooth.Model->Meshes[0].Vertices.size(),
                  "normals='flat' has more vertices than normals='smooth' due to unwelded sharp edge");

            VfsRead::GetVFS().Unmount("/TestFlagsVfs");
            std::filesystem::remove_all(tempRoot);
        }
        catch (...)
        {
            VfsRead::GetVFS().Unmount("/TestFlagsVfs");
            std::filesystem::remove_all(tempRoot);
            throw;
        }
    }
#endif
}

} // namespace ModelImportSettingsTests

