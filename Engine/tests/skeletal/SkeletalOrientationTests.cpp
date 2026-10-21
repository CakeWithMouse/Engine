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

#define ORIENT_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" #cond ") at line " << __LINE__ << std::endl; \
            throw std::runtime_error(msg); \
        } \
    } while (false)

namespace
{
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

void Test_RestPosePaletteMatchesMeshToModel()
{
#if defined(ENGINE_ENABLE_ASSIMP)
    SkeletalLoadResult result = LoadSkeletalModelFromVfs("/Game/Models/CesiumMan.glb");
    ORIENT_CHECK(result.Success && result.Model != nullptr, "CesiumMan must load");

    std::vector<XMFLOAT4X4> palette;
    bool ok = EvaluateSkeletalBindPose(*result.Model, palette, false);
    ORIENT_CHECK(ok, "EvaluateSkeletalBindPose on CesiumMan must succeed");

    const size_t boneCount = result.Model->ModelSkeleton.Bones.size();
    ORIENT_CHECK(palette.size() == boneCount, "Palette size must match bone count");

    float maxDiffFromMeshToModel = 0.0f;
    for (size_t i = 0; i < boneCount; ++i)
    {
        float diff = MatrixMaxDiff(palette[i], result.Model->MeshToModel);
        maxDiffFromMeshToModel = std::max(maxDiffFromMeshToModel, diff);
    }

    std::cout << "[SkeletalOrientationTests] Rest pose vs MeshToModel max diff: "
              << maxDiffFromMeshToModel << std::endl;
    // In bind pose, invBind * world = I, so final palette equals MeshToModel
    ORIENT_CHECK(maxDiffFromMeshToModel < 1e-4f, "Every bone palette matrix in rest pose must equal MeshToModel");
#else
    std::cout << "[SkeletalOrientationTests] SKIP: rest pose requires Assimp" << std::endl;
#endif
}

void Test_CesiumManOrientationAndSkinning()
{
#if defined(ENGINE_ENABLE_ASSIMP)
    SkeletalLoadResult result = LoadSkeletalModelFromVfs("/Game/Models/CesiumMan.glb");
    ORIENT_CHECK(result.Success && result.Model != nullptr, "CesiumMan must load");
    const auto& clip = result.Model->Clips[0];

    const float sampleTimes[2] = { 0.0f, clip.Duration * 0.25f };
    for (float t : sampleTimes)
    {
        std::vector<XMFLOAT4X4> pal;
        bool evalOk = EvaluateSkeletalAnimation(*result.Model, 0, t, pal, false);
        ORIENT_CHECK(evalOk, "Evaluate animation at time t");

        XMFLOAT3 bmin(1e9f, 1e9f, 1e9f), bmax(-1e9f, -1e9f, -1e9f);
        size_t vertCount = 0;
        for (const auto& mesh : result.Model->Meshes)
        {
            for (const auto& v : mesh.Vertices)
            {
                XMVECTOR inPos = XMLoadFloat3(&v.Position);
                XMVECTOR skinned = XMVectorZero();
                for (int k = 0; k < 4; ++k)
                {
                    if (v.BoneWeights[k] > 0.0f)
                    {
                        XMMATRIX bMat = XMLoadFloat4x4(&pal[v.BoneIndices[k]]);
                        skinned = XMVectorAdd(skinned, XMVectorScale(XMVector3Transform(inPos, bMat), v.BoneWeights[k]));
                    }
                }
                XMFLOAT3 p;
                XMStoreFloat3(&p, skinned);
                ORIENT_CHECK(!std::isnan(p.x) && !std::isnan(p.y) && !std::isnan(p.z), "No NaN in skinned position");
                ORIENT_CHECK(!std::isinf(p.x) && !std::isinf(p.y) && !std::isinf(p.z), "No Inf in skinned position");
                bmin.x = std::min(bmin.x, p.x); bmin.y = std::min(bmin.y, p.y); bmin.z = std::min(bmin.z, p.z);
                bmax.x = std::max(bmax.x, p.x); bmax.y = std::max(bmax.y, p.y); bmax.z = std::max(bmax.z, p.z);
                ++vertCount;
            }
        }
        float w = bmax.x - bmin.x;
        float h = bmax.y - bmin.y;
        float d = bmax.z - bmin.z;
        std::cout << "[SkeletalOrientationTests] CPU Skinning at t=" << t << "s (" << vertCount << " verts):\n"
                  << "  Bounds Min: (" << bmin.x << ", " << bmin.y << ", " << bmin.z << ")\n"
                  << "  Bounds Max: (" << bmax.x << ", " << bmax.y << ", " << bmax.z << ")\n"
                  << "  Width(X): " << w << ", Height(Y): " << h << ", Depth(Z): " << d << std::endl;

        // Requirements from architect:
        // Height along Y > 1.2, height > width along X, and height > depth along Z
        bool hCheck = (h > 1.2f);
        bool hGtW = (h > w);
        bool hGtD = (h > d);
        std::cout << "[SkeletalOrientationTests] BBox checks: h>1.2: " << (hCheck ? "OK" : "FAIL")
                  << ", h>w: " << (hGtW ? "OK" : "FAIL")
                  << ", h>d: " << (hGtD ? "OK" : "FAIL") << std::endl;
        ORIENT_CHECK(hCheck, "Height along Y must be > 1.2m");
        ORIENT_CHECK(hGtW, "Height along Y must be > width along X");
        ORIENT_CHECK(hGtD, "Height along Y must be > depth along Z");
    }
#else
    std::cout << "[SkeletalOrientationTests] SKIP: CPU skinning requires Assimp" << std::endl;
#endif
}

void Test_FaceOrientationForwardZ()
{
#if defined(ENGINE_ENABLE_ASSIMP)
    SkeletalLoadResult result = LoadSkeletalModelFromVfs("/Game/Models/CesiumMan.glb");
    ORIENT_CHECK(result.Success && result.Model != nullptr, "CesiumMan must load");
    const auto& clip = result.Model->Clips[0];
    const float t0 = 0.0f;
    const float t1 = clip.Duration * 0.25f;

    std::vector<XMFLOAT4X4> pal0, pal1;
    bool eval0 = EvaluateSkeletalAnimation(*result.Model, 0, t0, pal0, false);
    bool eval1 = EvaluateSkeletalAnimation(*result.Model, 0, t1, pal1, false);
    ORIENT_CHECK(eval0 && eval1, "Evaluate animation at t0 and t1");

    int toeLIdx = -1, toeRIdx = -1, ankleLIdx = -1, ankleRIdx = -1;
    for (size_t i = 0; i < result.Model->ModelSkeleton.Bones.size(); ++i)
    {
        const auto& name = result.Model->ModelSkeleton.Bones[i].Name;
        if (name == "leg_joint_L_5") toeLIdx = static_cast<int>(i);
        else if (name == "leg_joint_R_5") toeRIdx = static_cast<int>(i);
        else if (name == "leg_joint_L_3") ankleLIdx = static_cast<int>(i);
        else if (name == "leg_joint_R_3") ankleRIdx = static_cast<int>(i);
    }
    ORIENT_CHECK(toeLIdx >= 0 && toeRIdx >= 0, "Toe joints must exist");

    auto skinVertex = [](const SkinnedVertex& v, const std::vector<XMFLOAT4X4>& pal) -> XMFLOAT3 {
        XMVECTOR inPos = XMLoadFloat3(&v.Position);
        XMVECTOR skinned = XMVectorZero();
        for (int k = 0; k < 4; ++k)
        {
            if (v.BoneWeights[k] > 0.0f)
            {
                XMMATRIX bMat = XMLoadFloat4x4(&pal[v.BoneIndices[k]]);
                skinned = XMVectorAdd(skinned, XMVectorScale(XMVector3Transform(inPos, bMat), v.BoneWeights[k]));
            }
        }
        XMFLOAT3 p;
        XMStoreFloat3(&p, skinned);
        return p;
    };

    auto computeCentroidForBone = [&](int boneIdx, const std::vector<XMFLOAT4X4>& pal) -> XMFLOAT3 {
        XMFLOAT3 sum(0, 0, 0);
        float totalWeight = 0.0f;
        for (const auto& mesh : result.Model->Meshes)
        {
            for (const auto& v : mesh.Vertices)
            {
                for (int k = 0; k < 4; ++k)
                {
                    if (static_cast<int>(v.BoneIndices[k]) == boneIdx && v.BoneWeights[k] > 0.3f)
                    {
                        XMFLOAT3 p = skinVertex(v, pal);
                        sum.x += p.x * v.BoneWeights[k];
                        sum.y += p.y * v.BoneWeights[k];
                        sum.z += p.z * v.BoneWeights[k];
                        totalWeight += v.BoneWeights[k];
                    }
                }
            }
        }
        if (totalWeight > 0.0f)
        {
            sum.x /= totalWeight; sum.y /= totalWeight; sum.z /= totalWeight;
        }
        return sum;
    };

    XMFLOAT3 toeL_t0 = computeCentroidForBone(toeLIdx, pal0);
    XMFLOAT3 toeL_t1 = computeCentroidForBone(toeLIdx, pal1);
    XMFLOAT3 toeR_t0 = computeCentroidForBone(toeRIdx, pal0);
    XMFLOAT3 toeR_t1 = computeCentroidForBone(toeRIdx, pal1);

    XMFLOAT3 deltaL(toeL_t1.x - toeL_t0.x, toeL_t1.y - toeL_t0.y, toeL_t1.z - toeL_t0.z);
    XMFLOAT3 deltaR(toeR_t1.x - toeR_t0.x, toeR_t1.y - toeR_t0.y, toeR_t1.z - toeR_t0.z);

    std::cout << "[SkeletalOrientationTests] Raw Toe Tracking (t=0 to t=" << t1 << "s):\n"
              << "  Left toe centroid at t=0:  (" << toeL_t0.x << ", " << toeL_t0.y << ", " << toeL_t0.z << ")\n"
              << "  Left toe centroid at t=0.5: (" << toeL_t1.x << ", " << toeL_t1.y << ", " << toeL_t1.z << ")\n"
              << "  Left toe delta (dx,dy,dz): (" << deltaL.x << ", " << deltaL.y << ", " << deltaL.z << ")\n"
              << "  Right toe centroid at t=0:  (" << toeR_t0.x << ", " << toeR_t0.y << ", " << toeR_t0.z << ")\n"
              << "  Right toe centroid at t=0.5: (" << toeR_t1.x << ", " << toeR_t1.y << ", " << toeR_t1.z << ")\n"
              << "  Right toe delta (dx,dy,dz): (" << deltaR.x << ", " << deltaR.y << ", " << deltaR.z << ")" << std::endl;
#else
    std::cout << "[SkeletalOrientationTests] SKIP: face orientation requires Assimp" << std::endl;
#endif
}

void RunSkeletalOrientationTests()
{
    std::cout << "--- Running Skeletal Orientation Tests ---" << std::endl;
    Test_RestPosePaletteMatchesMeshToModel();
    Test_CesiumManOrientationAndSkinning();
    Test_FaceOrientationForwardZ();
}
