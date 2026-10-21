#include <Engine/Render/SkeletalAnimation.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

DirectX::XMFLOAT3 InterpolateVectorKey(const std::vector<VectorKey>& keys, float time)
{
    if (keys.empty())
    {
        return DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f};
    }
    if (keys.size() == 1 || time <= keys.front().Time)
    {
        return keys.front().Value;
    }
    if (time >= keys.back().Time)
    {
        return keys.back().Value;
    }

    // Binary search / find adjacent key pair
    for (size_t i = 0; i + 1 < keys.size(); ++i)
    {
        if (time >= keys[i].Time && time <= keys[i + 1].Time)
        {
            const float dt = keys[i + 1].Time - keys[i].Time;
            const float factor = (dt > 1e-6f) ? (time - keys[i].Time) / dt : 0.0f;
            XMVECTOR v0 = XMLoadFloat3(&keys[i].Value);
            XMVECTOR v1 = XMLoadFloat3(&keys[i + 1].Value);
            XMVECTOR result = XMVectorLerp(v0, v1, factor);
            XMFLOAT3 out;
            XMStoreFloat3(&out, result);
            return out;
        }
    }

    return keys.back().Value;
}

DirectX::XMFLOAT4 InterpolateQuatKey(const std::vector<QuatKey>& keys, float time)
{
    if (keys.empty())
    {
        return DirectX::XMFLOAT4{0.0f, 0.0f, 0.0f, 1.0f};
    }
    if (keys.size() == 1 || time <= keys.front().Time)
    {
        return keys.front().Value;
    }
    if (time >= keys.back().Time)
    {
        return keys.back().Value;
    }

    for (size_t i = 0; i + 1 < keys.size(); ++i)
    {
        if (time >= keys[i].Time && time <= keys[i + 1].Time)
        {
            const float dt = keys[i + 1].Time - keys[i].Time;
            const float factor = (dt > 1e-6f) ? (time - keys[i].Time) / dt : 0.0f;
            XMVECTOR q0 = XMLoadFloat4(&keys[i].Value);
            XMVECTOR q1 = XMLoadFloat4(&keys[i + 1].Value);
            XMVECTOR result = XMQuaternionSlerp(q0, q1, factor);
            result = XMQuaternionNormalize(result);
            XMFLOAT4 out;
            XMStoreFloat4(&out, result);
            return out;
        }
    }

    return keys.back().Value;
}

bool EvaluateSkeletalAnimation(
    const SkeletalModel& model,
    size_t clipIndex,
    float timeInSeconds,
    std::vector<DirectX::XMFLOAT4X4>& outPalette,
    bool transposeForGpu)
{
    const size_t boneCount = model.ModelSkeleton.Bones.size();
    if (boneCount == 0 || boneCount > MaxSkeletalBones)
    {
        return false;
    }

    const bool hasValidClip = (clipIndex < model.Clips.size());
    float animTime = timeInSeconds;
    if (hasValidClip)
    {
        const float duration = model.Clips[clipIndex].Duration;
        if (duration > 0.0f)
        {
            animTime = std::fmod(animTime, duration);
            if (animTime < 0.0f)
            {
                animTime += duration;
            }
        }
    }

    std::vector<XMMATRIX> worldTransforms(boneCount);
    outPalette.resize(boneCount);

    for (size_t i = 0; i < boneCount; ++i)
    {
        const Bone& bone = model.ModelSkeleton.Bones[i];
        XMMATRIX localMatrix = XMLoadFloat4x4(&bone.LocalTransform);

        if (hasValidClip)
        {
            const AnimationClip& clip = model.Clips[clipIndex];
            const NodeAnimationChannel* channel = nullptr;
            for (const auto& ch : clip.Channels)
            {
                if (ch.BoneIndex == static_cast<int32_t>(i) || ch.NodeName == bone.Name)
                {
                    channel = &ch;
                    break;
                }
            }

            if (channel != nullptr)
            {
                XMVECTOR transVec = XMLoadFloat3(&bone.RestTranslation);
                if (!channel->PositionKeys.empty())
                {
                    XMFLOAT3 keyPos = InterpolateVectorKey(channel->PositionKeys, animTime);
                    transVec = XMLoadFloat3(&keyPos);
                }

                XMVECTOR rotQuat = XMLoadFloat4(&bone.RestRotation);
                if (!channel->RotationKeys.empty())
                {
                    XMFLOAT4 keyRot = InterpolateQuatKey(channel->RotationKeys, animTime);
                    rotQuat = XMLoadFloat4(&keyRot);
                }

                XMVECTOR scaleVec = XMLoadFloat3(&bone.RestScale);
                if (!channel->ScalingKeys.empty())
                {
                    XMFLOAT3 keyScale = InterpolateVectorKey(channel->ScalingKeys, animTime);
                    scaleVec = XMLoadFloat3(&keyScale);
                }

                XMMATRIX sMat = XMMatrixScalingFromVector(scaleVec);
                XMMATRIX rMat = XMMatrixRotationQuaternion(rotQuat);
                XMMATRIX tMat = XMMatrixTranslationFromVector(transVec);

                // DirectXMath row-major concatenation: v * S * R * T
                localMatrix = XMMatrixMultiply(XMMatrixMultiply(sMat, rMat), tMat);
            }
        }

        XMMATRIX worldMatrix = localMatrix;
        if (bone.ParentIndex >= 0 && bone.ParentIndex < static_cast<int32_t>(i))
        {
            worldMatrix = XMMatrixMultiply(localMatrix, worldTransforms[bone.ParentIndex]);
        }
        worldTransforms[i] = worldMatrix;

        // Final skinning matrix: M_invBind * M_world
        // Transforms from bind mesh-space to bone-space, then to current animated space
        XMMATRIX invBind = XMLoadFloat4x4(&bone.InverseBindPose);
        XMMATRIX finalMatrix = XMMatrixMultiply(invBind, worldMatrix);

        if (transposeForGpu)
        {
            finalMatrix = XMMatrixTranspose(finalMatrix);
        }

        XMStoreFloat4x4(&outPalette[i], finalMatrix);
    }

    return true;
}

bool EvaluateSkeletalBindPose(
    const Skeleton& skeleton,
    std::vector<DirectX::XMFLOAT4X4>& outPalette,
    bool transposeForGpu)
{
    const size_t boneCount = skeleton.Bones.size();
    if (boneCount == 0 || boneCount > MaxSkeletalBones)
    {
        return false;
    }

    outPalette.resize(boneCount);
    std::vector<XMMATRIX> worldTransforms(boneCount);

    for (size_t i = 0; i < boneCount; ++i)
    {
        const auto& bone = skeleton.Bones[i];
        XMMATRIX localMatrix = XMLoadFloat4x4(&bone.LocalTransform);

        XMMATRIX worldMatrix = localMatrix;
        if (bone.ParentIndex >= 0 && bone.ParentIndex < static_cast<int32_t>(i))
        {
            worldMatrix = XMMatrixMultiply(localMatrix, worldTransforms[bone.ParentIndex]);
        }
        worldTransforms[i] = worldMatrix;

        XMMATRIX invBind = XMLoadFloat4x4(&bone.InverseBindPose);
        XMMATRIX finalMatrix = XMMatrixMultiply(invBind, worldMatrix);

        if (transposeForGpu)
        {
            finalMatrix = XMMatrixTranspose(finalMatrix);
        }

        XMStoreFloat4x4(&outPalette[i], finalMatrix);
    }

    return true;
}
