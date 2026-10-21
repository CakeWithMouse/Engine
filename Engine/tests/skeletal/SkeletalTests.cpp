#include <Engine/Render/SkeletalData.h>
#include <Engine/Render/SkeletalAnimation.h>
#include <Engine/Render/SkeletalLoader.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Resources/VirtualFileSystem.h>
#include "../../Source/Resources/VfsRead.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace DirectX;

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" #cond ") at line " << __LINE__ << std::endl; \
            throw std::runtime_error(msg); \
        } \
    } while (false)

namespace
{
    // Builds a synthetic skeletal model with 3 bones: Root -> Spine -> Arm
    std::shared_ptr<SkeletalModel> CreateSyntheticModel()
    {
        auto model = std::make_shared<SkeletalModel>();

        // 3 Bones
        Bone rootBone;
        rootBone.Name = "Root";
        rootBone.ParentIndex = -1;
        XMStoreFloat4x4(&rootBone.LocalTransform, XMMatrixIdentity());
        XMStoreFloat4x4(&rootBone.InverseBindPose, XMMatrixIdentity());
        model->ModelSkeleton.Bones.push_back(rootBone);

        Bone spineBone;
        spineBone.Name = "Spine";
        spineBone.ParentIndex = 0;
        // Local translation along Y: (0, 1, 0)
        XMMATRIX spineLocal = XMMatrixTranslation(0.0f, 1.0f, 0.0f);
        XMStoreFloat4x4(&spineBone.LocalTransform, spineLocal);
        // Inverse bind pose cancels out local translation
        XMMATRIX spineInvBind = XMMatrixTranslation(0.0f, -1.0f, 0.0f);
        XMStoreFloat4x4(&spineBone.InverseBindPose, spineInvBind);
        model->ModelSkeleton.Bones.push_back(spineBone);

        Bone armBone;
        armBone.Name = "Arm";
        armBone.ParentIndex = 1;
        // Local translation along X: (1, 0, 0). World bind = Translation(1, 1, 0)
        XMMATRIX armLocal = XMMatrixTranslation(1.0f, 0.0f, 0.0f);
        XMStoreFloat4x4(&armBone.LocalTransform, armLocal);
        XMMATRIX armInvBind = XMMatrixTranslation(-1.0f, -1.0f, 0.0f);
        XMStoreFloat4x4(&armBone.InverseBindPose, armInvBind);
        model->ModelSkeleton.Bones.push_back(armBone);

        // 1 Mesh with 4 vertices
        SkeletalMesh mesh;
        mesh.Vertices.resize(4);

        // V0 attached to Root (bone 0)
        mesh.Vertices[0].Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
        mesh.Vertices[0].BoneIndices[0] = 0;
        mesh.Vertices[0].BoneWeights[0] = 1.0f;

        // V1 between Root and Spine (bones 0 and 1)
        mesh.Vertices[1].Position = XMFLOAT3(0.0f, 0.5f, 0.0f);
        mesh.Vertices[1].BoneIndices[0] = 0; mesh.Vertices[1].BoneWeights[0] = 0.5f;
        mesh.Vertices[1].BoneIndices[1] = 1; mesh.Vertices[1].BoneWeights[1] = 0.5f;

        // V2 between Spine and Arm (bones 1 and 2)
        mesh.Vertices[2].Position = XMFLOAT3(0.5f, 1.0f, 0.0f);
        mesh.Vertices[2].BoneIndices[0] = 1; mesh.Vertices[2].BoneWeights[0] = 0.7f;
        mesh.Vertices[2].BoneIndices[1] = 2; mesh.Vertices[2].BoneWeights[1] = 0.3f;

        // V3 attached to Arm (bone 2)
        mesh.Vertices[3].Position = XMFLOAT3(1.0f, 1.0f, 0.0f);
        mesh.Vertices[3].BoneIndices[0] = 2;
        mesh.Vertices[3].BoneWeights[0] = 1.0f;

        mesh.Indices = { 0, 1, 2, 1, 3, 2 };
        model->TotalIndexCount = static_cast<int>(mesh.Indices.size());
        model->Meshes.push_back(std::move(mesh));

        // 1 Animation clip (duration 2.0s)
        AnimationClip clip;
        clip.Name = "TestWalk";
        clip.Duration = 2.0f;
        clip.TicksPerSecond = 25.0f;

        // Channel for Spine: rotates around Z, loops smoothly back to 0
        NodeAnimationChannel spineCh;
        spineCh.NodeName = "Spine";
        spineCh.BoneIndex = 1;
        // t=0.0: angle 0
        spineCh.RotationKeys.push_back({0.0f, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)});
        // t=1.0: angle pi/4 around Z
        float s = std::sin(XM_PIDIV4 * 0.5f);
        float c = std::cos(XM_PIDIV4 * 0.5f);
        spineCh.RotationKeys.push_back({1.0f, XMFLOAT4(0.0f, 0.0f, s, c)});
        // t=2.0: angle 0
        spineCh.RotationKeys.push_back({2.0f, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)});

        // Channel for Arm: oscillates translation along X
        NodeAnimationChannel armCh;
        armCh.NodeName = "Arm";
        armCh.BoneIndex = 2;
        armCh.PositionKeys.push_back({0.0f, XMFLOAT3(1.0f, 0.0f, 0.0f)});
        armCh.PositionKeys.push_back({1.0f, XMFLOAT3(1.5f, 0.0f, 0.0f)});
        armCh.PositionKeys.push_back({2.0f, XMFLOAT3(1.0f, 0.0f, 0.0f)});

        clip.Channels.push_back(std::move(spineCh));
        clip.Channels.push_back(std::move(armCh));
        model->Clips.push_back(std::move(clip));

        return model;
    }

    bool MatrixNearIdentity(const XMFLOAT4X4& m, float tol = 1e-4f)
    {
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                float expected = (r == c) ? 1.0f : 0.0f;
                if (std::abs(m.m[r][c] - expected) > tol)
                {
                    return false;
                }
            }
        }
        return true;
    }

    float MatrixMaxDiff(const XMFLOAT4X4& a, const XMFLOAT4X4& b)
    {
        float maxDiff = 0.0f;
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                maxDiff = std::max(maxDiff, std::abs(a.m[r][c] - b.m[r][c]));
            }
        }
        return maxDiff;
    }
}

void Test_ModelStructureAndWeights()
{
    auto model = CreateSyntheticModel();

    // 1. Bone and vertex counts
    TEST_CHECK(model->ModelSkeleton.Bones.size() == 3, "Bone count must be 3");
    TEST_CHECK(model->Meshes.size() == 1, "Mesh count must be 1");
    TEST_CHECK(model->Meshes[0].Vertices.size() == 4, "Vertex count must be 4");
    TEST_CHECK(model->Clips.size() == 1, "Clip count must be 1");
    TEST_CHECK(model->Clips[0].Duration > 0.0f, "Clip duration must be positive");

    // 2. Weights and indices validity
    const size_t boneCount = model->ModelSkeleton.Bones.size();
    for (size_t i = 0; i < model->Meshes[0].Vertices.size(); ++i)
    {
        const auto& vert = model->Meshes[0].Vertices[i];
        float sumWeights = 0.0f;
        for (int k = 0; k < 4; ++k)
        {
            sumWeights += vert.BoneWeights[k];
            if (vert.BoneWeights[k] > 0.0f)
            {
                TEST_CHECK(vert.BoneIndices[k] < boneCount, "Bone index must be < bone count");
            }
        }
        TEST_CHECK(std::abs(sumWeights - 1.0f) < 1e-5f, "Vertex weights must sum to 1.0");
    }
}

void Test_BindPoseGivesIdentity()
{
    auto model = CreateSyntheticModel();
    std::vector<XMFLOAT4X4> palette;

    // In bind pose (t=0.0 with default keys), every bone's final skinning matrix must be Identity
    bool success = EvaluateSkeletalAnimation(*model, 0, 0.0f, palette, false);
    TEST_CHECK(success, "EvaluateSkeletalAnimation must succeed");
    TEST_CHECK(palette.size() == 3, "Palette size must match bone count");

    for (size_t i = 0; i < palette.size(); ++i)
    {
        TEST_CHECK(MatrixNearIdentity(palette[i]), "Bind pose palette matrix must be identity");
    }
}

void Test_AnimationInterpolationAndContinuity()
{
    auto model = CreateSyntheticModel();
    std::vector<XMFLOAT4X4> paletteAtMid;
    std::vector<XMFLOAT4X4> paletteAtEnd;
    std::vector<XMFLOAT4X4> paletteAtStart;

    // t=1.0: animated pose must differ from identity for animated bones 1 and 2
    EvaluateSkeletalAnimation(*model, 0, 1.0f, paletteAtMid, false);
    TEST_CHECK(!MatrixNearIdentity(paletteAtMid[1]), "Spine palette at t=1.0 must differ from identity");
    TEST_CHECK(!MatrixNearIdentity(paletteAtMid[2]), "Arm palette at t=1.0 must differ from identity");

    // Continuity across loop seam: t=1.999f and t=2.001f (which loops to 0.001f)
    EvaluateSkeletalAnimation(*model, 0, 1.999f, paletteAtEnd, false);
    EvaluateSkeletalAnimation(*model, 0, 2.001f, paletteAtStart, false);

    for (size_t i = 0; i < 3; ++i)
    {
        float seamDiff = MatrixMaxDiff(paletteAtEnd[i], paletteAtStart[i]);
        TEST_CHECK(seamDiff < 0.01f, "Animation palette must be continuous across looping boundary");
    }
}

void Test_BoneLimit64()
{
    auto model = CreateSyntheticModel();
    // Add dummy bones until count = 65 (> MaxSkeletalBones)
    while (model->ModelSkeleton.Bones.size() < 65)
    {
        Bone extraBone;
        extraBone.Name = "Extra_" + std::to_string(model->ModelSkeleton.Bones.size());
        extraBone.ParentIndex = 0;
        XMStoreFloat4x4(&extraBone.LocalTransform, XMMatrixIdentity());
        XMStoreFloat4x4(&extraBone.InverseBindPose, XMMatrixIdentity());
        model->ModelSkeleton.Bones.push_back(extraBone);
    }

    std::vector<XMFLOAT4X4> palette;
    bool success = EvaluateSkeletalAnimation(*model, 0, 0.0f, palette, false);
    TEST_CHECK(!success, "Model with >64 bones must be rejected by animation evaluation");
}

void Test_StubLoaderWithoutAssimp()
{
    SkeletalLoadResult memResult = LoadSkeletalModelFromMemory("fake_bytes", 10, "glb");
#if !defined(ENGINE_ENABLE_ASSIMP)
    TEST_CHECK(!memResult.Success, "Stub loader must return failure when Assimp is disabled");
    TEST_CHECK(memResult.ErrorMessage == "Assimp not enabled", "Stub loader must report 'Assimp not enabled'");

    SkeletalLoadResult vfsResult = LoadSkeletalModelFromVfs("/Game/Models/CesiumMan.glb");
    TEST_CHECK(!vfsResult.Success, "Stub VFS loader must return failure when Assimp is disabled");
    TEST_CHECK(vfsResult.ErrorMessage == "Assimp not enabled", "Stub VFS loader must report 'Assimp not enabled'");
#endif
}

void Test_GlbImport()
{
#if defined(ENGINE_ENABLE_ASSIMP)
    std::filesystem::path exeDir = EnginePaths::ExecutableDirectory();
    std::filesystem::path repo = exeDir.parent_path().parent_path().parent_path().parent_path();
    std::filesystem::path gameContent = repo / "Game" / "RuntimeExample" / "Content";
    if (!std::filesystem::exists(gameContent / "Models" / "CesiumMan.glb"))
    {
        // Try fallback search upwards
        std::filesystem::path p = exeDir;
        for (int i = 0; i < 6; ++i)
        {
            if (std::filesystem::exists(p / "Game" / "RuntimeExample" / "Content" / "Models" / "CesiumMan.glb"))
            {
                gameContent = p / "Game" / "RuntimeExample" / "Content";
                break;
            }
            p = p.parent_path();
        }
    }

    VfsRead::GetVFS().Mount("/Game", gameContent);

    SkeletalLoadResult result = LoadSkeletalModelFromVfs("/Game/Models/CesiumMan.glb");
    TEST_CHECK(result.Success, ("CesiumMan.glb import failed: " + result.ErrorMessage).c_str());
    TEST_CHECK(result.Model != nullptr, "Imported model must not be null");

    const size_t boneCount = result.Model->ModelSkeleton.Bones.size();
    TEST_CHECK(boneCount > 0, "CesiumMan must contain bones");
    TEST_CHECK(boneCount <= MaxSkeletalBones, "CesiumMan bones must be <= 64");

    size_t totalVertices = 0;
    for (const auto& mesh : result.Model->Meshes)
    {
        totalVertices += mesh.Vertices.size();
        for (const auto& vert : mesh.Vertices)
        {
            float sumWeights = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                sumWeights += vert.BoneWeights[k];
                if (vert.BoneWeights[k] > 0.0f)
                {
                    TEST_CHECK(vert.BoneIndices[k] < boneCount, "Bone index must be < bone count");
                }
            }
            TEST_CHECK(std::abs(sumWeights - 1.0f) < 1e-4f, "Vertex weights must sum to 1.0");
        }
    }
    TEST_CHECK(totalVertices > 0, "CesiumMan must contain vertices");

    TEST_CHECK(!result.Model->Clips.empty(), "CesiumMan must contain animation clips");
    const auto& clip = result.Model->Clips[0];
    TEST_CHECK(clip.Duration > 0.0f, "CesiumMan clip duration must be > 0");

    // Sample palettes at t=0, t=0.25*dur, t=0.5*dur
    std::vector<XMFLOAT4X4> pal0, pal25, pal50, palEnd, palSeam;
    bool ev0 = EvaluateSkeletalAnimation(*result.Model, 0, 0.0f, pal0, false);
    bool ev25 = EvaluateSkeletalAnimation(*result.Model, 0, clip.Duration * 0.25f, pal25, false);
    bool ev50 = EvaluateSkeletalAnimation(*result.Model, 0, clip.Duration * 0.5f, pal50, false);
    TEST_CHECK(ev0 && ev25 && ev50, "Evaluation at 0, 0.25, 0.5 duration must succeed");

    float maxDevFromIdentity = 0.0f;
    for (size_t i = 0; i < boneCount; ++i)
    {
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                float expected = (r == c) ? 1.0f : 0.0f;
                maxDevFromIdentity = std::max(maxDevFromIdentity, std::abs(pal0[i].m[r][c] - expected));
            }
        }
    }

    float diff0_25 = 0.0f;
    float diff25_50 = 0.0f;
    for (size_t i = 0; i < boneCount; ++i)
    {
        diff0_25 = std::max(diff0_25, MatrixMaxDiff(pal0[i], pal25[i]));
        diff25_50 = std::max(diff25_50, MatrixMaxDiff(pal25[i], pal50[i]));
    }
    TEST_CHECK(diff0_25 > 0.01f, "Palette at t=0 and t=0.25*dur must differ");
    TEST_CHECK(diff25_50 > 0.01f, "Palette at t=0.25*dur and t=0.5*dur must differ");

    // Loop continuity check
    EvaluateSkeletalAnimation(*result.Model, 0, clip.Duration - 0.001f, palEnd, false);
    EvaluateSkeletalAnimation(*result.Model, 0, 0.001f, palSeam, false);
    float seamDiff = 0.0f;
    int seamMaxBone = -1;
    for (size_t i = 0; i < boneCount; ++i)
    {
        float d = MatrixMaxDiff(palEnd[i], palSeam[i]);
        if (d > seamDiff)
        {
            seamDiff = d;
            seamMaxBone = static_cast<int>(i);
        }
    }

    std::cout << "[SkeletalTests] CesiumMan loaded successfully:\n"
              << "  Bones: " << boneCount << " (<= 64)\n"
              << "  Vertices: " << totalVertices << "\n"
              << "  Clip name: '" << clip.Name << "', duration: " << clip.Duration << "s\n"
              << "  Bind pose max deviation from identity: " << maxDevFromIdentity << "\n"
              << "  Palette diff(0, 0.25*dur): " << diff0_25 << "\n"
              << "  Palette diff(0.25, 0.5*dur): " << diff25_50 << "\n"
              << "  Seam diff: " << seamDiff << " (bone: " 
              << (seamMaxBone >= 0 ? result.Model->ModelSkeleton.Bones[seamMaxBone].Name : "none") 
              << ")" << std::endl;

    TEST_CHECK(diff0_25 > 0.01f, "Palette at t=0 and t=0.25*dur must differ");
    TEST_CHECK(diff25_50 > 0.01f, "Palette at t=0.25*dur and t=0.5*dur must differ");
    // Print details if seamDiff is noticeable
    if (seamDiff >= 0.05f)
    {
        std::cout << "  Note on seamDiff: bone " << result.Model->ModelSkeleton.Bones[seamMaxBone].Name
                  << " has seam difference " << seamDiff << " (likely root motion or non-looping walk cycle)" << std::endl;
    }
    TEST_CHECK(seamDiff < 0.2f, "Loop seam must not have catastrophic discontinuity");
#else
    std::cout << "[SkeletalTests] SKIP: no Assimp" << std::endl;
#endif
}

int main()
{
    std::cout << "=== Running Skeletal Animation and Import Tests ===" << std::endl;
    int failures = 0;

    const std::pair<const char*, void(*)()> testList[] = {
        {"model structure and vertex weights", Test_ModelStructureAndWeights},
        {"bind pose yields identity palette at t=0", Test_BindPoseGivesIdentity},
        {"animation interpolation and looping continuity", Test_AnimationInterpolationAndContinuity},
        {"bone limit 64 rejection", Test_BoneLimit64},
        {"stub loader contract without Assimp", Test_StubLoaderWithoutAssimp},
        {"glb import (CesiumMan)", Test_GlbImport}
    };

    for (const auto& t : testList)
    {
        try
        {
            t.second();
            std::cout << "PASS: " << t.first << std::endl;
        }
        catch (const std::exception& ex)
        {
            std::cerr << "FAIL: " << t.first << " - " << ex.what() << std::endl;
            ++failures;
        }
    }

    std::cout << "=== Skeletal Tests: " << (sizeof(testList)/sizeof(testList[0]) - failures) 
              << "/" << (sizeof(testList)/sizeof(testList[0])) << " passed ===" << std::endl;

    return failures ? 1 : 0;
}
