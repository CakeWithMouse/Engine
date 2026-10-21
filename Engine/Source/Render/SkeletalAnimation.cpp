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
                XMFLOAT3 translation = InterpolateVectorKey(channel->PositionKeys, animTime);
                XMFLOAT4 rotation = InterpolateQuatKey(channel->RotationKeys, animTime);
                XMFLOAT3 scale = channel->ScalingKeys.empty()
                    ? XMFLOAT3{1.0f, 1.0f, 1.0f}
                    : InterpolateVectorKey(channel->ScalingKeys, animTime);

                XMMATRIX sMat = XMMatrixScaling(scale.x, scale.y, scale.z);
                XMMATRIX rMat = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));
                XMMATRIX tMat = XMMatrixTranslation(translation.x, translation.y, translation.z);

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
