#include <Engine/Render/SkeletalBaker.h>
#include <Engine/Render/SkeletalData.h>
#include <Engine/Render/SkeletalAnimation.h>
#include <Engine/Render/SkeletalLoader.h>
#include "../../Source/Resources/VfsRead.h"
#include <directxmath.h>
#include <directxpackedvector.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace DirectX;

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << (msg) << " (" #cond ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while (false)

namespace
{
SkeletalModel BuildSyntheticModelWithTwoClips()
{
    SkeletalModel model;

    Bone root;
    root.Name = "Root";
    root.ParentIndex = -1;
    XMStoreFloat4x4(&root.LocalTransform, XMMatrixIdentity());
    XMStoreFloat4x4(&root.InverseBindPose, XMMatrixIdentity());
    root.RestTranslation = {0.0f, 0.0f, 0.0f};
    root.RestRotation = {0.0f, 0.0f, 0.0f, 1.0f};
    root.RestScale = {1.0f, 1.0f, 1.0f};
    model.ModelSkeleton.Bones.push_back(root);

    Bone child;
    child.Name = "Child";
    child.ParentIndex = 0;
    XMStoreFloat4x4(&child.LocalTransform, XMMatrixTranslation(0.0f, 1.0f, 0.0f));
    XMStoreFloat4x4(&child.InverseBindPose, XMMatrixTranslation(0.0f, -1.0f, 0.0f));
    child.RestTranslation = {0.0f, 1.0f, 0.0f};
    child.RestRotation = {0.0f, 0.0f, 0.0f, 1.0f};
    child.RestScale = {1.0f, 1.0f, 1.0f};
    model.ModelSkeleton.Bones.push_back(child);

    // Clip 0: Walk (1.0s)
    AnimationClip clip0;
    clip0.Name = "Walk";
    clip0.Duration = 1.0f;
    clip0.TicksPerSecond = 30.0f;
    {
        NodeAnimationChannel ch;
        ch.NodeName = "Child";
        ch.BoneIndex = 1;
        ch.PositionKeys.push_back({0.0f, {0.0f, 1.0f, 0.0f}});
        ch.PositionKeys.push_back({0.5f, {0.0f, 1.5f, 0.0f}});
        ch.PositionKeys.push_back({1.0f, {0.0f, 1.0f, 0.0f}});

        XMFLOAT4 q0, q1;
        XMStoreFloat4(&q0, XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), 0.0f));
        XMStoreFloat4(&q1, XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XM_PIDIV4));
        ch.RotationKeys.push_back({0.0f, q0});
        ch.RotationKeys.push_back({0.5f, q1});
        ch.RotationKeys.push_back({1.0f, q0});

        clip0.Channels.push_back(ch);
    }
    model.Clips.push_back(clip0);

    // Clip 1: Run (0.5s)
    AnimationClip clip1;
    clip1.Name = "Run";
    clip1.Duration = 0.5f;
    clip1.TicksPerSecond = 30.0f;
    {
        NodeAnimationChannel ch;
        ch.NodeName = "Child";
        ch.BoneIndex = 1;
        ch.PositionKeys.push_back({0.0f, {0.0f, 1.0f, 0.0f}});
        ch.PositionKeys.push_back({0.25f, {0.0f, 1.8f, 0.2f}});
        ch.PositionKeys.push_back({0.5f, {0.0f, 1.0f, 0.0f}});
        clip1.Channels.push_back(ch);
    }
    model.Clips.push_back(clip1);

    return model;
}
} // namespace

bool TestCrowdSkinnedVertex()
{
    static_assert(sizeof(SkinnedVertex) == 64, "SkinnedVertex must be 64 bytes");
    static_assert(sizeof(CrowdSkinnedVertex) == 32, "CrowdSkinnedVertex must be 32 bytes");

    std::cout << "[CrowdSkinnedVertex] Size reduction: " << sizeof(SkinnedVertex)
              << " bytes -> " << sizeof(CrowdSkinnedVertex) << " bytes (50.0% bandwidth savings)" << std::endl;

    SkinnedVertex sv;
    sv.Position = {1.2345f, -2.3456f, 3.4567f};
    sv.Normal = {0.57735f, 0.57735f, 0.57735f};
    sv.TexCoord = {0.25f, 0.75f};
    sv.BoneIndices[0] = 3;
    sv.BoneIndices[1] = 12;
    sv.BoneIndices[2] = 0;
    sv.BoneIndices[3] = 0;
    sv.BoneWeights[0] = 0.65f;
    sv.BoneWeights[1] = 0.35f;
    sv.BoneWeights[2] = 0.0f;
    sv.BoneWeights[3] = 0.0f;

    CrowdSkinnedVertex cv = ConvertToCrowdVertex(sv);

    // Check position identical
    TEST_CHECK(cv.Position.x == sv.Position.x && cv.Position.y == sv.Position.y && cv.Position.z == sv.Position.z,
               "Position must be preserved exactly in CrowdSkinnedVertex");

    // Check weights sum to 255 (1.0f in UNORM)
    uint32_t weightSum = static_cast<uint32_t>(cv.BoneWeights[0]) + cv.BoneWeights[1] + cv.BoneWeights[2] + cv.BoneWeights[3];
    TEST_CHECK(weightSum == 255, "Quantized bone weights must sum strictly to 255 (1.0 in UNORM)");

    // Check normal precision
    float nx = PackedVector::XMConvertHalfToFloat(cv.Normal[0]);
    float ny = PackedVector::XMConvertHalfToFloat(cv.Normal[1]);
    float nz = PackedVector::XMConvertHalfToFloat(cv.Normal[2]);
    float normErr = std::abs(nx - sv.Normal.x) + std::abs(ny - sv.Normal.y) + std::abs(nz - sv.Normal.z);
    TEST_CHECK(normErr < 0.005f, "Normal error in half4 conversion must be < 0.005");

    // Check indices
    TEST_CHECK(cv.BoneIndices[0] == 3 && cv.BoneIndices[1] == 12, "Bone indices must match");

    std::cout << "PASS: crowd skinned vertex format and quantization" << std::endl;
    return true;
}

bool TestSkeletalBakerSynthetic()
{
    SkeletalModel model = BuildSyntheticModelWithTwoClips();

    const float fps = 30.0f;
    SkeletalBakeResult bakeResult = BakeSkeletalModel(model, fps);
    TEST_CHECK(bakeResult.Success, "Baking synthetic model must succeed");

    const auto& tex = bakeResult.Texture;
    TEST_CHECK(tex.IsValid(), "Baked texture must be valid");
    TEST_CHECK(tex.BoneCount == 2, "BoneCount must be 2");
    TEST_CHECK(tex.Width == 2 * 3, "Width must be boneCount * 3 (6)");
    TEST_CHECK(tex.Clips.size() == 2, "Baked texture must contain 2 clips");

    // Clip 0: 1.0s * 30fps = 30 frames
    TEST_CHECK(tex.Clips[0].FrameCount == 30, "Clip 0 frame count must be 30");
    TEST_CHECK(tex.Clips[0].StartRow == 0, "Clip 0 must start at row 0");

    // Clip 1: 0.5s * 30fps = 15 frames
    TEST_CHECK(tex.Clips[1].FrameCount == 15, "Clip 1 frame count must be 15");
    TEST_CHECK(tex.Clips[1].StartRow == 30, "Clip 1 must start at row 30");
    TEST_CHECK(tex.Height == 45, "Total height must be 30 + 15 = 45");

    // Verify baked matrix rows match EvaluateSkeletalAnimation on multiple frames
    for (size_t clipIdx = 0; clipIdx < 2; ++clipIdx)
    {
        const auto& clip = tex.Clips[clipIdx];
        for (uint32_t k = 0; k < clip.FrameCount; k += 5)
        {
            float t = static_cast<float>(k) / fps;
            std::vector<XMFLOAT4X4> cpuPalette;
            bool evOk = EvaluateSkeletalAnimation(model, clipIdx, t, cpuPalette, false);
            TEST_CHECK(evOk, "EvaluateSkeletalAnimation must succeed");

            const uint32_t rowY = clip.StartRow + k;
            for (size_t b = 0; b < tex.BoneCount; ++b)
            {
                const float* texRow0 = tex.GetTexel(static_cast<uint32_t>(b * 3 + 0), rowY);
                const float* texRow1 = tex.GetTexel(static_cast<uint32_t>(b * 3 + 1), rowY);
                const float* texRow2 = tex.GetTexel(static_cast<uint32_t>(b * 3 + 2), rowY);
                TEST_CHECK(texRow0 && texRow1 && texRow2, "Texel pointers must not be null");

                // Transpose cpu matrix to compare with baked rows:
                // row0 of M^T is (m00, m10, m20, m30)
                // row1 of M^T is (m01, m11, m21, m31)
                // row2 of M^T is (m02, m12, m22, m32)
                const auto& m = cpuPalette[b];

                TEST_CHECK(std::abs(texRow0[0] - m._11) < 1e-6f, "texRow0.x == m._11");
                TEST_CHECK(std::abs(texRow0[1] - m._21) < 1e-6f, "texRow0.y == m._21");
                TEST_CHECK(std::abs(texRow0[2] - m._31) < 1e-6f, "texRow0.z == m._31");
                TEST_CHECK(std::abs(texRow0[3] - m._41) < 1e-6f, "texRow0.w == m._41");

                TEST_CHECK(std::abs(texRow1[0] - m._12) < 1e-6f, "texRow1.x == m._12");
                TEST_CHECK(std::abs(texRow1[1] - m._22) < 1e-6f, "texRow1.y == m._22");
                TEST_CHECK(std::abs(texRow1[2] - m._32) < 1e-6f, "texRow1.z == m._32");
                TEST_CHECK(std::abs(texRow1[3] - m._42) < 1e-6f, "texRow1.w == m._42");

                TEST_CHECK(std::abs(texRow2[0] - m._13) < 1e-6f, "texRow2.x == m._13");
                TEST_CHECK(std::abs(texRow2[1] - m._23) < 1e-6f, "texRow2.y == m._23");
                TEST_CHECK(std::abs(texRow2[2] - m._33) < 1e-6f, "texRow2.z == m._33");
                TEST_CHECK(std::abs(texRow2[3] - m._43) < 1e-6f, "texRow2.w == m._43");

                // Test normal transformation equivalence:
                // CPU normal: n_cpu = mul(n, (float3x3)M)
                // GPU shader formula:
                // n_gpu.x = dot(n, texRow0.xyz)
                // n_gpu.y = dot(n, texRow1.xyz)
                // n_gpu.z = dot(n, texRow2.xyz)
                XMFLOAT3 testNormal = {0.3f, 0.7f, -0.65f};
                XMVECTOR nVec = XMLoadFloat3(&testNormal);
                XMMATRIX mMat = XMLoadFloat4x4(&m);
                XMVECTOR nCpuTransformed = XMVector3TransformNormal(nVec, mMat);
                XMFLOAT3 nCpu;
                XMStoreFloat3(&nCpu, nCpuTransformed);

                float nGpuX = testNormal.x * texRow0[0] + testNormal.y * texRow0[1] + testNormal.z * texRow0[2];
                float nGpuY = testNormal.x * texRow1[0] + testNormal.y * texRow1[1] + testNormal.z * texRow1[2];
                float nGpuZ = testNormal.x * texRow2[0] + testNormal.y * texRow2[1] + testNormal.z * texRow2[2];

                TEST_CHECK(std::abs(nGpuX - nCpu.x) < 1e-6f, "Normal X transformation must match CPU");
                TEST_CHECK(std::abs(nGpuY - nCpu.y) < 1e-6f, "Normal Y transformation must match CPU");
                TEST_CHECK(std::abs(nGpuZ - nCpu.z) < 1e-6f, "Normal Z transformation must match CPU");
            }
        }
    }

    // Limit rejection tests:
    SkeletalModel emptyBones;
    TEST_CHECK(!BakeSkeletalModel(emptyBones).Success, "Model with 0 bones must fail");

    SkeletalModel tooManyBones = model;
    tooManyBones.ModelSkeleton.Bones.resize(65);
    TEST_CHECK(!BakeSkeletalModel(tooManyBones).Success, "Model with >64 bones must fail");

    SkeletalModel noClips = model;
    noClips.Clips.clear();
    TEST_CHECK(!BakeSkeletalModel(noClips).Success, "Model with 0 clips must fail");

    std::cout << "PASS: skeletal baker synthetic model and limits" << std::endl;
    return true;
}

bool TestSkeletalBakerCesiumMan()
{
#if !defined(ENGINE_ENABLE_ASSIMP)
    std::cout << "[SkeletalBakerTests] SKIP: CesiumMan requires Assimp" << std::endl;
    return true;
#else
    std::string vpath = "/Game/Models/CesiumMan.glb";
    auto bytes = VfsRead::Active().ReadBytes(vpath);
    if (bytes.empty())
    {
        std::cerr << "SKIP: " << vpath << " not found in VFS" << std::endl;
        return true;
    }

    SkeletalLoadResult loadResult = LoadSkeletalModelFromMemory(bytes.data(), bytes.size(), "glb");
    TEST_CHECK(loadResult.Success && loadResult.Model, "CesiumMan must load successfully");

    const auto& model = *loadResult.Model;
    const float fps = 30.0f;
    SkeletalBakeResult bakeResult = BakeSkeletalModel(model, fps);
    TEST_CHECK(bakeResult.Success, "Bake CesiumMan must succeed");

    const auto& tex = bakeResult.Texture;
    TEST_CHECK(tex.IsValid(), "Baked texture must be valid");
    TEST_CHECK(tex.BoneCount == 19, "CesiumMan has 19 bones");
    TEST_CHECK(tex.Width == 19 * 3, "Width must be 19 * 3 = 57");
    TEST_CHECK(tex.Clips.size() >= 1, "Must contain at least 1 clip");

    const auto& clip = tex.Clips[0];
    TEST_CHECK(clip.FrameCount == 60, "2.0s clip at 30 fps must yield 60 frames");
    TEST_CHECK(tex.Height == clip.FrameCount, "Height must match total frames");

    std::cout << "[SkeletalBakerTests] CesiumMan baked texture: " << tex.Width << "x" << tex.Height
              << " (" << (tex.RawData.size() * sizeof(float)) / 1024 << " KB RGBA32F)" << std::endl;

    // Check multiple frames against EvaluateSkeletalAnimation
    const uint32_t sampleFrames[] = { 0, 15, 30, 45, 59 };
    for (uint32_t frame : sampleFrames)
    {
        float t = static_cast<float>(frame) / fps;
        std::vector<XMFLOAT4X4> cpuPalette;
        bool ev = EvaluateSkeletalAnimation(model, 0, t, cpuPalette, false);
        TEST_CHECK(ev, "EvaluateSkeletalAnimation on CesiumMan must succeed");

        for (size_t b = 0; b < tex.BoneCount; ++b)
        {
            const float* r0 = tex.GetTexel(static_cast<uint32_t>(b * 3 + 0), frame);
            const float* r1 = tex.GetTexel(static_cast<uint32_t>(b * 3 + 1), frame);
            const float* r2 = tex.GetTexel(static_cast<uint32_t>(b * 3 + 2), frame);
            const auto& m = cpuPalette[b];

            TEST_CHECK(std::abs(r0[0] - m._11) < 1e-5f, "CesiumMan r0._11 match");
            TEST_CHECK(std::abs(r0[1] - m._21) < 1e-5f, "CesiumMan r0._21 match");
            TEST_CHECK(std::abs(r0[2] - m._31) < 1e-5f, "CesiumMan r0._31 match");
            TEST_CHECK(std::abs(r0[3] - m._41) < 1e-5f, "CesiumMan r0._41 match");

            TEST_CHECK(std::abs(r1[0] - m._12) < 1e-5f, "CesiumMan r1._12 match");
            TEST_CHECK(std::abs(r1[1] - m._22) < 1e-5f, "CesiumMan r1._22 match");
            TEST_CHECK(std::abs(r1[2] - m._32) < 1e-5f, "CesiumMan r1._32 match");
            TEST_CHECK(std::abs(r1[3] - m._42) < 1e-5f, "CesiumMan r1._42 match");

            TEST_CHECK(std::abs(r2[0] - m._13) < 1e-5f, "CesiumMan r2._13 match");
            TEST_CHECK(std::abs(r2[1] - m._23) < 1e-5f, "CesiumMan r2._23 match");
            TEST_CHECK(std::abs(r2[2] - m._33) < 1e-5f, "CesiumMan r2._33 match");
            TEST_CHECK(std::abs(r2[3] - m._43) < 1e-5f, "CesiumMan r2._43 match");

            // Normal transformation check
            XMFLOAT3 testNormal = {0.0f, 1.0f, 0.0f};
            XMVECTOR nVec = XMLoadFloat3(&testNormal);
            XMMATRIX mMat = XMLoadFloat4x4(&m);
            XMVECTOR nCpuTransformed = XMVector3TransformNormal(nVec, mMat);
            XMFLOAT3 nCpu;
            XMStoreFloat3(&nCpu, nCpuTransformed);

            float nGpuX = testNormal.x * r0[0] + testNormal.y * r0[1] + testNormal.z * r0[2];
            float nGpuY = testNormal.x * r1[0] + testNormal.y * r1[1] + testNormal.z * r1[2];
            float nGpuZ = testNormal.x * r2[0] + testNormal.y * r2[1] + testNormal.z * r2[2];

            TEST_CHECK(std::abs(nGpuX - nCpu.x) < 1e-5f, "CesiumMan normal X match");
            TEST_CHECK(std::abs(nGpuY - nCpu.y) < 1e-5f, "CesiumMan normal Y match");
            TEST_CHECK(std::abs(nGpuZ - nCpu.z) < 1e-5f, "CesiumMan normal Z match");
        }
    }

    std::cout << "PASS: skeletal baker CesiumMan validation" << std::endl;
    return true;
#endif
}

bool RunSkeletalBakerSuite()
{
    std::cout << "--- Running Skeletal Baker and Crowd Vertex Tests ---" << std::endl;
    if (!TestCrowdSkinnedVertex()) return false;
    if (!TestSkeletalBakerSynthetic()) return false;
    if (!TestSkeletalBakerCesiumMan()) return false;
    std::cout << "PASS: all skeletal baker tests passed" << std::endl;
    return true;
}
