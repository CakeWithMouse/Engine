#pragma once

#include <Engine/Render/SkeletalData.h>
#include <vector>

/**
 * Interpolates vector keys using linear interpolation (lerp).
 */
DirectX::XMFLOAT3 InterpolateVectorKey(const std::vector<VectorKey>& keys, float time);

/**
 * Interpolates quaternion keys using spherical linear interpolation (slerp).
 */
DirectX::XMFLOAT4 InterpolateQuatKey(const std::vector<QuatKey>& keys, float time);

/**
 * Evaluates bone palette matrices for a given skeletal model and animation clip at time t.
 *
 * M_world = M_local * M_parent_world
 * M_final = M_invBind * M_world
 *
 * In bind pose (t=0 with default bind keys), M_final equals the identity matrix.
 *
 * @param model Input skeletal model containing skeleton and clips.
 * @param clipIndex Index of animation clip to sample.
 * @param timeInSeconds Sample time (automatically looped within clip duration).
 * @param outPalette Receives palette matrices (one per bone, up to MaxSkeletalBones).
 * @param transposeForGpu If true, transposes matrices for HLSL column-major constant buffer packing.
 * @return True on success, false if model has no bones or clipIndex is invalid.
 */
bool EvaluateSkeletalAnimation(
    const SkeletalModel& model,
    size_t clipIndex,
    float timeInSeconds,
    std::vector<DirectX::XMFLOAT4X4>& outPalette,
    bool transposeForGpu = false);

/**
 * Evaluates the rest pose (bind pose) bone palette directly from bone LocalTransform hierarchy.
 * In an ideal bind pose, all skin palette matrices equal the identity matrix (I_4x4).
 */
bool EvaluateSkeletalBindPose(
    const Skeleton& skeleton,
    std::vector<DirectX::XMFLOAT4X4>& outPalette,
    bool transposeForGpu = false);

