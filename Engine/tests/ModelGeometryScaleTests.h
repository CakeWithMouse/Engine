#pragma once

#include "../Source/Scene/ModelImportSettings.h"
#include "../Source/Resources/VfsRead.h"
#include "ImportSettingsTestCases.h"
#include "TestSkipped.h"
#include <Engine/Render/SkeletalLoader.h>
#include <Engine/Render/SkeletalData.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace ModelGeometryScaleTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

inline void WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary);
    out << text;
    Check(static_cast<bool>(out), "fixture write succeeded");
}

struct Box
{
    float mn[3] = {1e30f, 1e30f, 1e30f};
    float mx[3] = {-1e30f, -1e30f, -1e30f};
};

inline Box ModelBox(const SkeletalModel& model)
{
    Box b;
    for (const auto& mesh : model.Meshes)
    {
        for (const auto& v : mesh.Vertices)
        {
            b.mn[0] = (std::min)(b.mn[0], v.Position.x); b.mx[0] = (std::max)(b.mx[0], v.Position.x);
            b.mn[1] = (std::min)(b.mn[1], v.Position.y); b.mx[1] = (std::max)(b.mx[1], v.Position.y);
            b.mn[2] = (std::min)(b.mn[2], v.Position.z); b.mx[2] = (std::max)(b.mx[2], v.Position.z);
        }
    }
    return b;
}

inline float Size(const Box& b, int k) { return b.mx[k] - b.mn[k]; }

inline bool Near(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps * (1.0f + std::fabs(b));
}

inline bool VerticesEqual(const SkeletalModel& a, const SkeletalModel& b)
{
    if (a.Meshes.size() != b.Meshes.size()) return false;
    for (size_t m = 0; m < a.Meshes.size(); ++m)
    {
        const auto& va = a.Meshes[m].Vertices;
        const auto& vb = b.Meshes[m].Vertices;
        if (va.size() != vb.size()) return false;
        if (!va.empty() && std::memcmp(va.data(), vb.data(), va.size() * sizeof(SkinnedVertex)) != 0) return false;
    }
    return true;
}

inline bool BonesEqual(const SkeletalModel& a, const SkeletalModel& b)
{
    const auto& ba = a.ModelSkeleton.Bones;
    const auto& bb = b.ModelSkeleton.Bones;
    if (ba.size() != bb.size()) return false;
    for (size_t i = 0; i < ba.size(); ++i)
    {
        if (ba[i].Name != bb[i].Name || ba[i].ParentIndex != bb[i].ParentIndex) return false;
        if (std::memcmp(&ba[i].InverseBindPose, &bb[i].InverseBindPose, sizeof(DirectX::XMFLOAT4X4)) != 0) return false;
        if (std::memcmp(&ba[i].LocalTransform, &bb[i].LocalTransform, sizeof(DirectX::XMFLOAT4X4)) != 0) return false;
        if (std::memcmp(&ba[i].RestTranslation, &bb[i].RestTranslation, sizeof(DirectX::XMFLOAT3)) != 0) return false;
        if (std::memcmp(&ba[i].RestRotation, &bb[i].RestRotation, sizeof(DirectX::XMFLOAT4)) != 0) return false;
        if (std::memcmp(&ba[i].RestScale, &bb[i].RestScale, sizeof(DirectX::XMFLOAT3)) != 0) return false;
    }
    return true;
}

inline SkeletalLoadResult Load(const std::string& vpath)
{
    auto res = LoadSkeletalModelFromVfs(vpath);
    Check(res.Success && res.Model, "model loaded through VFS");
    return res;
}

inline void Run()
{
    // Parser parity on the shared cases: v2 sets the geometry-scale sign, v1/legacy do not.
    {
        using namespace ImportSettingsTestCases;
        ModelImportSettings s;
        std::string err;
        Check(ModelImportSettings::Parse(kValidV2, s, &err), "Engine parses valid v2");
        Check(s.version == 2 && s.AppliesGeometryScale(), "v2 applies geometry scale");
        Check(std::fabs(s.scale - 0.01f) < 1e-6f && s.useFileUnitScale, "v2 scale and useFileUnitScale read");

        Check(ModelImportSettings::Parse(kValidV2WithUnknownFields, s, &err), "Engine parses v2 with unknown fields");
        Check(s.version == 2 && std::fabs(s.scale - 2.0f) < 1e-6f, "v2 unknown-field case keeps version and scale");

        Check(ModelImportSettings::Parse(kValidV2Minimal, s, &err), "Engine parses minimal v2");
        Check(s.version == 2 && s.normals == "file" && s.mergeMeshes, "minimal v2 falls back to defaults");

        Check(ModelImportSettings::Parse(kValidV1, s, &err), "Engine parses valid v1");
        Check(s.version == 1 && !s.AppliesGeometryScale(), "v1 does not apply geometry scale");

        Check(ModelImportSettings::Parse(kLegacyScale, s, &err), "Engine parses legacy scale");
        Check(s.version == 1 && !s.AppliesGeometryScale(), "legacy migrates to v1 without geometry scale");
    }

#if !defined(ENGINE_ENABLE_ASSIMP)
    throw TestSkipped("geometry scale checks require -EnableAssimp");
#else
    namespace fs = std::filesystem;
    const auto stamp = std::to_string(static_cast<unsigned long long>(std::time(nullptr))) + "_" + std::to_string(std::clock());

    // ---- Static OBJ: version 1 ignores scale; version 2 bakes it into geometry ----
    {
        const auto tempRoot = fs::temp_directory_path() / ("GeomScaleStatic_" + stamp);
        fs::create_directories(tempRoot);
        try
        {
            VfsRead::GetVFS().Mount("/GeomScaleStatic", tempRoot);
            WriteText(tempRoot / "quad.obj",
                "v 0 0 0\nv 10 0 0\nv 10 10 0\nv 0 10 0\nf 1 2 3\nf 1 3 4\n");
            const std::string vpath = "/GeomScaleStatic/quad.obj";
            const auto sidecar = tempRoot / "quad.obj.importsettings.json";

            WriteText(sidecar, R"({"version":1,"kind":"model","model":{"scale":1.0}})");
            const auto base = Load(vpath);
            const Box baseBox = ModelBox(*base.Model);
            Check(Near(Size(baseBox, 0), 10.0f) && Near(Size(baseBox, 1), 10.0f), "static base bbox is 10x10");

            WriteText(sidecar, R"({"version":1,"kind":"model","model":{"scale":0.01}})");
            Check(VerticesEqual(*Load(vpath).Model, *base.Model), "v1 scale=0.01 leaves static geometry bit-for-bit unchanged");
            WriteText(sidecar, R"({"version":1,"kind":"model","model":{"scale":2.0}})");
            Check(VerticesEqual(*Load(vpath).Model, *base.Model), "v1 scale=2 leaves static geometry bit-for-bit unchanged");
            WriteText(sidecar, R"({"scale":0.01})");
            Check(VerticesEqual(*Load(vpath).Model, *base.Model), "legacy scale migrates to v1 and does not scale geometry");

            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":1.0}})");
            Check(VerticesEqual(*Load(vpath).Model, *base.Model), "v2 scale=1 is a no-op (bit-for-bit equal to v1)");

            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":0.01}})");
            const Box smallBox = ModelBox(*Load(vpath).Model);
            Check(Near(Size(smallBox, 0), 0.1f) && Near(Size(smallBox, 1), 0.1f), "v2 scale=0.01 shrinks static bbox 100x");

            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":2.0}})");
            const Box big = ModelBox(*Load(vpath).Model);
            Check(Near(Size(big, 0), 20.0f) && Near(Size(big, 1), 20.0f), "v2 scale=2 doubles static bbox");

            // OBJ carries no unit metadata: useFileUnitScale true/false both normalize by 1.
            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":0.01,"useFileUnitScale":true}})");
            const Box unitTrue = ModelBox(*Load(vpath).Model);
            Check(Near(Size(unitTrue, 0), 0.1f), "v2 useFileUnitScale=true on unit-less OBJ still applies scale only");

            VfsRead::GetVFS().Unmount("/GeomScaleStatic");
            fs::remove_all(tempRoot);
        }
        catch (...)
        {
            VfsRead::GetVFS().Unmount("/GeomScaleStatic");
            fs::remove_all(tempRoot);
            throw;
        }
    }

    // ---- Skeletal CesiumMan: bones, InverseBindPose, rest pose, animation and bounds scale together ----
    {
        const auto fixture = fs::path(__FILE__).parent_path() / ".." / ".." /
            "Game" / "RuntimeExample" / "Content" / "Models" / "CesiumMan.glb";
        Check(fs::exists(fixture), "CesiumMan.glb fixture exists");
        const auto tempRoot = fs::temp_directory_path() / ("GeomScaleSkel_" + stamp);
        fs::create_directories(tempRoot);
        try
        {
            VfsRead::GetVFS().Mount("/GeomScaleSkel", tempRoot);
            fs::copy_file(fixture, tempRoot / "CesiumMan.glb", fs::copy_options::overwrite_existing);
            const std::string vpath = "/GeomScaleSkel/CesiumMan.glb";
            const auto sidecar = tempRoot / "CesiumMan.glb.importsettings.json";

            WriteText(sidecar, R"({"version":1,"kind":"model","model":{"scale":1.0}})");
            const auto base = Load(vpath);
            Check(!base.Model->ModelSkeleton.Bones.empty(), "CesiumMan has bones");
            Check(!base.Model->Clips.empty() && !base.Model->Clips[0].Channels.empty(), "CesiumMan has animation channels");
            const Box baseBox = ModelBox(*base.Model);
            const Bone baseBone = base.Model->ModelSkeleton.Bones[0];
            const DirectX::XMFLOAT3 baseAnim = base.Model->Clips[0].Channels[0].PositionKeys.empty()
                ? DirectX::XMFLOAT3(0, 0, 0)
                : base.Model->Clips[0].Channels[0].PositionKeys[0].Value;
            const DirectX::XMFLOAT4X4 baseMeshToModel = base.Model->MeshToModel;

            WriteText(sidecar, R"({"version":1,"kind":"model","model":{"scale":0.01}})");
            const auto v1Small = Load(vpath);
            Check(VerticesEqual(*v1Small.Model, *base.Model), "v1 scale=0.01 leaves skinned vertices bit-for-bit unchanged");
            Check(BonesEqual(*v1Small.Model, *base.Model), "v1 scale=0.01 leaves skeleton bit-for-bit unchanged");

            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":1.0}})");
            const auto v2One = Load(vpath);
            Check(VerticesEqual(*v2One.Model, *base.Model), "v2 scale=1 skinned vertices equal v1");
            Check(BonesEqual(*v2One.Model, *base.Model), "v2 scale=1 skeleton equal v1");

            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":0.01}})");
            const auto v2Small = Load(vpath);
            const Box smallBox = ModelBox(*v2Small.Model);
            for (int k = 0; k < 3; ++k)
            {
                Check(Near(Size(smallBox, k), Size(baseBox, k) * 0.01f, 1e-3f), "v2 scale=0.01 shrinks skinned bbox 100x");
            }
            const Bone& sb = v2Small.Model->ModelSkeleton.Bones[0];
            Check(Near(sb.InverseBindPose.m[3][0], baseBone.InverseBindPose.m[3][0] * 0.01f, 1e-3f) &&
                  Near(sb.InverseBindPose.m[3][1], baseBone.InverseBindPose.m[3][1] * 0.01f, 1e-3f) &&
                  Near(sb.InverseBindPose.m[3][2], baseBone.InverseBindPose.m[3][2] * 0.01f, 1e-3f),
                  "v2 scale=0.01 scales InverseBindPose translation 100x");
            Check(std::memcmp(&sb.InverseBindPose.m[0][0], &baseBone.InverseBindPose.m[0][0], 3 * sizeof(float)) == 0,
                  "v2 scale leaves InverseBindPose rotation column unchanged");
            Check(Near(sb.RestTranslation.x, baseBone.RestTranslation.x * 0.01f, 1e-3f) &&
                  Near(sb.RestTranslation.y, baseBone.RestTranslation.y * 0.01f, 1e-3f) &&
                  Near(sb.RestTranslation.z, baseBone.RestTranslation.z * 0.01f, 1e-3f),
                  "v2 scale=0.01 scales rest translation 100x");
            Check(std::memcmp(&sb.RestRotation, &baseBone.RestRotation, sizeof(DirectX::XMFLOAT4)) == 0,
                  "v2 scale leaves rest rotation unchanged");
            Check(std::memcmp(&sb.RestScale, &baseBone.RestScale, sizeof(DirectX::XMFLOAT3)) == 0,
                  "v2 scale leaves rest scale unchanged");
            Check(Near(v2Small.Model->MeshToModel.m[3][0], baseMeshToModel.m[3][0] * 0.01f, 1e-3f) &&
                  Near(v2Small.Model->MeshToModel.m[3][1], baseMeshToModel.m[3][1] * 0.01f, 1e-3f) &&
                  Near(v2Small.Model->MeshToModel.m[3][2], baseMeshToModel.m[3][2] * 0.01f, 1e-3f),
                  "v2 scale scales MeshToModel translation");
            if (!v2Small.Model->Clips[0].Channels[0].PositionKeys.empty())
            {
                const DirectX::XMFLOAT3 a = v2Small.Model->Clips[0].Channels[0].PositionKeys[0].Value;
                Check(Near(a.x, baseAnim.x * 0.01f, 1e-3f) && Near(a.y, baseAnim.y * 0.01f, 1e-3f) && Near(a.z, baseAnim.z * 0.01f, 1e-3f),
                      "v2 scale=0.01 scales animation position keys 100x");
            }

            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":2.0}})");
            const auto v2Big = Load(vpath);
            const Box bigBox = ModelBox(*v2Big.Model);
            for (int k = 0; k < 3; ++k)
            {
                Check(Near(Size(bigBox, k), Size(baseBox, k) * 2.0f, 1e-3f), "v2 scale=2 doubles skinned bbox");
            }
            Check(Near(v2Big.Model->ModelSkeleton.Bones[0].InverseBindPose.m[3][0], baseBone.InverseBindPose.m[3][0] * 2.0f, 1e-3f),
                  "v2 scale=2 doubles InverseBindPose translation");

            VfsRead::GetVFS().Unmount("/GeomScaleSkel");
            fs::remove_all(tempRoot);
        }
        catch (...)
        {
            VfsRead::GetVFS().Unmount("/GeomScaleSkel");
            fs::remove_all(tempRoot);
            throw;
        }
    }

    // ---- FBX blender-scale100: useFileUnitScale (false/true) x scale (1.0 / 0.01 / 2.0) ----
    {
        const auto fixture = fs::path(__FILE__).parent_path() / "assets" / "fixtures" / "blender-scale100.fbx";
        Check(fs::exists(fixture), "blender-scale100.fbx fixture exists");
        const auto tempRoot = fs::temp_directory_path() / ("GeomScaleFbx_" + stamp);
        fs::create_directories(tempRoot);
        try
        {
            VfsRead::GetVFS().Mount("/GeomScaleFbx", tempRoot);
            fs::copy_file(fixture, tempRoot / "scale100.fbx", fs::copy_options::overwrite_existing);
            const std::string vpath = "/GeomScaleFbx/scale100.fbx";
            const auto sidecar = tempRoot / "scale100.fbx.importsettings.json";

            // Matrix: useFileUnitScale false/true x scale 1.0 / 0.01 / 2.0
            // Raw Blender box with 100x node scale has dimension 200x200x200 (maxDim = 200.0).
            // useFileUnitScale=true normalizes node scale to 2x2x2 (maxDim = 2.0).
            // version:2 applies geometry scale on top.

            // 1. useFileUnitScale=false, scale=1.0 -> 200.0
            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":1.0,"useFileUnitScale":false}})");
            const Box bFalse1 = ModelBox(*Load(vpath).Model);
            const float dimFalse1 = (std::max)({Size(bFalse1, 0), Size(bFalse1, 1), Size(bFalse1, 2)});
            Check(Near(dimFalse1, 200.0f, 1e-2f), "FBX useFileUnitScale=false scale=1.0 dimension is 200.0");

            // 2. useFileUnitScale=false, scale=0.01 -> 200.0 * 0.01 = 2.0
            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":0.01,"useFileUnitScale":false}})");
            const Box bFalse001 = ModelBox(*Load(vpath).Model);
            const float dimFalse001 = (std::max)({Size(bFalse001, 0), Size(bFalse001, 1), Size(bFalse001, 2)});
            Check(Near(dimFalse001, 2.0f, 1e-2f), "FBX useFileUnitScale=false scale=0.01 dimension is 2.0");

            // 3. useFileUnitScale=false, scale=2.0 -> 200.0 * 2.0 = 400.0
            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":2.0,"useFileUnitScale":false}})");
            const Box bFalse2 = ModelBox(*Load(vpath).Model);
            const float dimFalse2 = (std::max)({Size(bFalse2, 0), Size(bFalse2, 1), Size(bFalse2, 2)});
            Check(Near(dimFalse2, 400.0f, 1e-2f), "FBX useFileUnitScale=false scale=2.0 dimension is 400.0");

            // 4. useFileUnitScale=true, scale=1.0 -> 2.0 (unit normalized)
            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":1.0,"useFileUnitScale":true}})");
            const Box bTrue1 = ModelBox(*Load(vpath).Model);
            const float dimTrue1 = (std::max)({Size(bTrue1, 0), Size(bTrue1, 1), Size(bTrue1, 2)});
            Check(Near(dimTrue1, 2.0f, 1e-2f), "FBX useFileUnitScale=true scale=1.0 dimension is 2.0");

            // 5. useFileUnitScale=true, scale=0.01 -> 2.0 * 0.01 = 0.02
            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":0.01,"useFileUnitScale":true}})");
            const Box bTrue001 = ModelBox(*Load(vpath).Model);
            const float dimTrue001 = (std::max)({Size(bTrue001, 0), Size(bTrue001, 1), Size(bTrue001, 2)});
            Check(Near(dimTrue001, 0.02f, 1e-2f), "FBX useFileUnitScale=true scale=0.01 dimension is 0.02");

            // 6. useFileUnitScale=true, scale=2.0 -> 2.0 * 2.0 = 4.0
            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":2.0,"useFileUnitScale":true}})");
            const Box bTrue2 = ModelBox(*Load(vpath).Model);
            const float dimTrue2 = (std::max)({Size(bTrue2, 0), Size(bTrue2, 1), Size(bTrue2, 2)});
            Check(Near(dimTrue2, 4.0f, 1e-2f), "FBX useFileUnitScale=true scale=2.0 dimension is 4.0");

            // 7. FBX normals flat: verify flat mode on FBX loads successfully and generates geometry
            WriteText(sidecar, R"({"version":2,"kind":"model","model":{"scale":1.0,"normals":"flat"}})");
            const auto fbxFlat = Load(vpath);
            Check(fbxFlat.Success && fbxFlat.Model && !fbxFlat.Model->Meshes.empty(), "FBX loads with flat normals");
            Check(!fbxFlat.Model->Meshes[0].Vertices.empty(), "FBX has vertices with flat normals");

            VfsRead::GetVFS().Unmount("/GeomScaleFbx");
            fs::remove_all(tempRoot);
        }
        catch (...)
        {
            VfsRead::GetVFS().Unmount("/GeomScaleFbx");
            fs::remove_all(tempRoot);
            throw;
        }
    }
#endif
}

} // namespace ModelGeometryScaleTests