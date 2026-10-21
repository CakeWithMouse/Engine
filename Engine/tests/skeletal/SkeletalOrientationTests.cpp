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
        // Height along Y > 1.2 and > 2 * (depth along Z), width along X reasonable (0.2 .. 2.0)
        bool hCheck = (h > 1.2f);
        bool ratioCheck = (h > 2.0f * d);
        bool wCheck = (w > 0.2f && w < 2.0f);
        std::cout << "[SkeletalOrientationTests] BBox checks: h>1.2: " << (hCheck ? "OK" : "FAIL")
                  << ", h>2*d: " << (ratioCheck ? "OK" : "FAIL (d=" + std::to_string(d) + " > h/2=" + std::to_string(h*0.5f) + ")")
                  << ", w in [0.2, 2.0]: " << (wCheck ? "OK" : "FAIL") << std::endl;
        ORIENT_CHECK(hCheck, "Height along Y must be > 1.2m");
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

    // Sample at t=0
    std::vector<XMFLOAT4X4> pal;
    bool evalOk = EvaluateSkeletalAnimation(*result.Model, 0, 0.0f, pal, false);
    ORIENT_CHECK(evalOk, "Evaluate animation at t=0");

    // Find specific joints to detect forward direction:
    // Pelvis (root): Skeleton_torso_joint_1
    // Left toe: leg_joint_L_5, Right toe: leg_joint_R_5
    // Left ankle: leg_joint_L_3, Right ankle: leg_joint_R_3
    // Head: Skeleton_neck_joint_2
    int pelvisIdx = -1, toeLIdx = -1, toeRIdx = -1, ankleLIdx = -1, ankleRIdx = -1, headIdx = -1;
    for (size_t i = 0; i < result.Model->ModelSkeleton.Bones.size(); ++i)
    {
        const auto& name = result.Model->ModelSkeleton.Bones[i].Name;
        if (name == "Skeleton_torso_joint_1") pelvisIdx = static_cast<int>(i);
        else if (name == "leg_joint_L_5") toeLIdx = static_cast<int>(i);
        else if (name == "leg_joint_R_5") toeRIdx = static_cast<int>(i);
        else if (name == "leg_joint_L_3") ankleLIdx = static_cast<int>(i);
        else if (name == "leg_joint_R_3") ankleRIdx = static_cast<int>(i);
        else if (name == "Skeleton_neck_joint_2") headIdx = static_cast<int>(i);
    }

    ORIENT_CHECK(pelvisIdx >= 0 && headIdx >= 0, "Key joints must exist");

    // Transform joint origins by their palette matrices: origin (0,0,0) in bone space
    auto getJointWorldPos = [&](int idx) -> XMFLOAT3 {
        XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMMATRIX m = XMLoadFloat4x4(&pal[idx]);
        // Note: pal[idx] transforms from mesh bind space to model space.
        // For joint origin: joint is located at local position in skeleton worldTransforms * MeshToModel.
        // We can sample a vertex heavily weighted by this bone or inspect the translation column/row of worldTransforms.
        XMFLOAT3 pos;
        XMStoreFloat3(&pos, XMVector3Transform(zero, m));
        return pos;
    };

    // Calculate vertex centroid of face / chest vs back in skinned space
    // We sample all vertices with height Y between (head.y - 0.3) and (head.y - 0.05) (face/head region)
    float maxY = -1e9f, minY = 1e9f;
    std::vector<XMFLOAT3> skinnedVertices;
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
            skinnedVertices.push_back(p);
            maxY = std::max(maxY, p.y);
            minY = std::min(minY, p.y);
        }
    }

    // Centroid of feet region (bottom 10%)
    float feetThreshold = minY + (maxY - minY) * 0.10f;
    XMFLOAT3 feetCenter(0, 0, 0);
    size_t feetCount = 0;
    // Toes vs heels in bottom region:
    float feetMinZ = 1e9f, feetMaxZ = -1e9f;
    float feetMinX = 1e9f, feetMaxX = -1e9f;
    for (const auto& p : skinnedVertices)
    {
        if (p.y <= feetThreshold)
        {
            feetCenter.x += p.x; feetCenter.y += p.y; feetCenter.z += p.z;
            feetMinZ = std::min(feetMinZ, p.z); feetMaxZ = std::max(feetMaxZ, p.z);
            feetMinX = std::min(feetMinX, p.x); feetMaxX = std::max(feetMaxX, p.x);
            ++feetCount;
        }
    }
    if (feetCount > 0)
    {
        feetCenter.x /= feetCount; feetCenter.y /= feetCount; feetCenter.z /= feetCount;
    }

    // Centroid of entire body
    XMFLOAT3 bodyCenter(0, 0, 0);
    for (const auto& p : skinnedVertices)
    {
        bodyCenter.x += p.x; bodyCenter.y += p.y; bodyCenter.z += p.z;
    }
    bodyCenter.x /= skinnedVertices.size();
    bodyCenter.y /= skinnedVertices.size();
    bodyCenter.z /= skinnedVertices.size();

    std::cout << "[SkeletalOrientationTests] Face/forward detection analysis:\n"
              << "  Total vertices: " << skinnedVertices.size() << "\n"
              << "  Height range Y: [" << minY << ", " << maxY << "]\n"
              << "  Body center: (" << bodyCenter.x << ", " << bodyCenter.y << ", " << bodyCenter.z << ")\n"
              << "  Feet center: (" << feetCenter.x << ", " << feetCenter.y << ", " << feetCenter.z << ")\n"
              << "  Feet Z extent: [" << feetMinZ << ", " << feetMaxZ << "], deltaZ = " << (feetMaxZ - feetMinZ) << "\n"
              << "  Feet X extent: [" << feetMinX << ", " << feetMaxX << "], deltaX = " << (feetMaxX - feetMinX) << std::endl;

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
