#include <Engine/Render/SkeletalLoader.h>

#if defined(ENGINE_ENABLE_ASSIMP)

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "../Resources/VfsRead.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    DirectX::XMFLOAT4X4 ConvertAssimpMatrix(const aiMatrix4x4& mat)
    {
        DirectX::XMFLOAT4X4 m;
        m.m[0][0] = mat.a1; m.m[0][1] = mat.b1; m.m[0][2] = mat.c1; m.m[0][3] = mat.d1;
        m.m[1][0] = mat.a2; m.m[1][1] = mat.b2; m.m[1][2] = mat.c2; m.m[1][3] = mat.d2;
        m.m[2][0] = mat.a3; m.m[2][1] = mat.b3; m.m[2][2] = mat.c3; m.m[2][3] = mat.d3;
        m.m[3][0] = mat.a4; m.m[3][1] = mat.b4; m.m[3][2] = mat.c4; m.m[3][3] = mat.d4;
        return m;
    }

    const aiNode* FindNodeRecursive(const aiNode* node, const std::string& name)
    {
        if (!node) return nullptr;
        if (node->mName.C_Str() == name) return node;
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
        {
            if (const aiNode* found = FindNodeRecursive(node->mChildren[i], name))
            {
                return found;
            }
        }
        return nullptr;
    }
}

SkeletalLoadResult LoadSkeletalModelFromMemory(const void* data, size_t size, std::string_view hint)
{
    SkeletalLoadResult result;
    if (data == nullptr || size == 0)
    {
        result.ErrorMessage = "Empty input data for skeletal model";
        return result;
    }

    try
    {
        Assimp::Importer importer;
        const unsigned int flags =
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_LimitBoneWeights |
            aiProcess_JoinIdenticalVertices |
            aiProcess_ConvertToLeftHanded;

        const std::string hintStr(hint);
        const aiScene* scene = importer.ReadFileFromMemory(data, size, flags, hintStr.c_str());
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        {
            result.ErrorMessage = importer.GetErrorString();
            return result;
        }

        auto model = std::make_shared<SkeletalModel>();
        std::unordered_map<std::string, int32_t> boneNameToIndex;

        // Pass 1: discover all bones across all meshes
        for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
        {
            const aiMesh* mesh = scene->mMeshes[m];
            for (unsigned int b = 0; b < mesh->mNumBones; ++b)
            {
                const aiBone* bone = mesh->mBones[b];
                const std::string boneName = bone->mName.C_Str();
                if (boneNameToIndex.find(boneName) == boneNameToIndex.end())
                {
                    if (model->ModelSkeleton.Bones.size() >= MaxSkeletalBones)
                    {
                        result.ErrorMessage = "Bone count exceeds maximum limit of 64";
                        return result;
                    }

                    int32_t newIndex = static_cast<int32_t>(model->ModelSkeleton.Bones.size());
                    boneNameToIndex[boneName] = newIndex;

                    Bone newBone;
                    newBone.Name = boneName;
                    newBone.InverseBindPose = ConvertAssimpMatrix(bone->mOffsetMatrix);
                    newBone.LocalTransform = ConvertAssimpMatrix(aiMatrix4x4());
                    model->ModelSkeleton.Bones.push_back(newBone);
                }
            }
        }

        // Pass 2: resolve hierarchy and default local transforms from node tree
        for (auto& bone : model->ModelSkeleton.Bones)
        {
            const aiNode* node = FindNodeRecursive(scene->mRootNode, bone.Name);
            if (node)
            {
                bone.LocalTransform = ConvertAssimpMatrix(node->mTransformation);
                DirectX::XMVECTOR s, r, t;
                DirectX::XMMatrixDecompose(&s, &r, &t, DirectX::XMLoadFloat4x4(&bone.LocalTransform));
                DirectX::XMStoreFloat3(&bone.RestScale, s);
                DirectX::XMStoreFloat4(&bone.RestRotation, r);
                DirectX::XMStoreFloat3(&bone.RestTranslation, t);

                const aiNode* parent = node->mParent;
                while (parent != nullptr)
                {
                    auto it = boneNameToIndex.find(parent->mName.C_Str());
                    if (it != boneNameToIndex.end())
                    {
                        bone.ParentIndex = it->second;
                        break;
                    }
                    parent = parent->mParent;
                }
            }
        }

        // Pass 3: load mesh vertices and assign bone influences
        for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
        {
            const aiMesh* mesh = scene->mMeshes[m];
            SkeletalMesh skMesh;
            skMesh.Vertices.resize(mesh->mNumVertices);

            for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
            {
                skMesh.Vertices[v].Position = DirectX::XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
                if (mesh->HasNormals())
                {
                    skMesh.Vertices[v].Normal = DirectX::XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
                }
                if (mesh->HasTextureCoords(0))
                {
                    skMesh.Vertices[v].TexCoord = DirectX::XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
                }
            }

            for (unsigned int b = 0; b < mesh->mNumBones; ++b)
            {
                const aiBone* bone = mesh->mBones[b];
                int32_t boneIndex = boneNameToIndex[bone->mName.C_Str()];
                for (unsigned int w = 0; w < bone->mNumWeights; ++w)
                {
                    unsigned int vId = bone->mWeights[w].mVertexId;
                    float weightVal = bone->mWeights[w].mWeight;
                    if (vId < skMesh.Vertices.size())
                    {
                        SkinnedVertex& vert = skMesh.Vertices[vId];
                        for (int slot = 0; slot < 4; ++slot)
                        {
                            if (vert.BoneWeights[slot] == 0.0f)
                            {
                                vert.BoneIndices[slot] = static_cast<uint32_t>(boneIndex);
                                vert.BoneWeights[slot] = weightVal;
                                break;
                            }
                        }
                    }
                }
            }

            // Normalize weights
            for (auto& vert : skMesh.Vertices)
            {
                float sum = vert.BoneWeights[0] + vert.BoneWeights[1] + vert.BoneWeights[2] + vert.BoneWeights[3];
                if (sum > 1e-6f)
                {
                    vert.BoneWeights[0] /= sum;
                    vert.BoneWeights[1] /= sum;
                    vert.BoneWeights[2] /= sum;
                    vert.BoneWeights[3] /= sum;
                }
            }

            for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
            {
                const aiFace& face = mesh->mFaces[f];
                if (face.mNumIndices == 3)
                {
                    skMesh.Indices.push_back(face.mIndices[0]);
                    skMesh.Indices.push_back(face.mIndices[1]);
                    skMesh.Indices.push_back(face.mIndices[2]);
                }
            }

            model->TotalIndexCount += static_cast<int>(skMesh.Indices.size());
            model->Meshes.push_back(std::move(skMesh));
        }

        // Pass 4: animation clips
        for (unsigned int a = 0; a < scene->mNumAnimations; ++a)
        {
            const aiAnimation* anim = scene->mAnimations[a];
            AnimationClip clip;
            clip.Name = anim->mName.C_Str();
            const float tps = (anim->mTicksPerSecond > 0.0) ? static_cast<float>(anim->mTicksPerSecond) : 25.0f;
            clip.TicksPerSecond = tps;
            clip.Duration = static_cast<float>(anim->mDuration / tps);

            for (unsigned int c = 0; c < anim->mNumChannels; ++c)
            {
                const aiNodeAnim* channel = anim->mChannels[c];
                NodeAnimationChannel nodeChannel;
                nodeChannel.NodeName = channel->mNodeName.C_Str();
                auto it = boneNameToIndex.find(nodeChannel.NodeName);
                if (it != boneNameToIndex.end())
                {
                    nodeChannel.BoneIndex = it->second;
                }

                for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k)
                {
                    VectorKey vk;
                    vk.Time = static_cast<float>(channel->mPositionKeys[k].mTime / tps);
                    vk.Value = DirectX::XMFLOAT3(channel->mPositionKeys[k].mValue.x, channel->mPositionKeys[k].mValue.y, channel->mPositionKeys[k].mValue.z);
                    nodeChannel.PositionKeys.push_back(vk);
                }

                for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k)
                {
                    QuatKey qk;
                    qk.Time = static_cast<float>(channel->mRotationKeys[k].mTime / tps);
                    qk.Value = DirectX::XMFLOAT4(channel->mRotationKeys[k].mValue.x, channel->mRotationKeys[k].mValue.y, channel->mRotationKeys[k].mValue.z, channel->mRotationKeys[k].mValue.w);
                    nodeChannel.RotationKeys.push_back(qk);
                }

                for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k)
                {
                    VectorKey sk;
                    sk.Time = static_cast<float>(channel->mScalingKeys[k].mTime / tps);
                    sk.Value = DirectX::XMFLOAT3(channel->mScalingKeys[k].mValue.x, channel->mScalingKeys[k].mValue.y, channel->mScalingKeys[k].mValue.z);
                    nodeChannel.ScalingKeys.push_back(sk);
                }

                clip.Channels.push_back(std::move(nodeChannel));
            }

            model->Clips.push_back(std::move(clip));
        }

        result.Success = true;
        result.Model = model;
    }
    catch (const std::exception& ex)
    {
        result.Success = false;
        result.ErrorMessage = ex.what();
    }
    catch (...)
    {
        result.Success = false;
        result.ErrorMessage = "Unknown exception during skeletal import";
    }

    return result;
}

SkeletalLoadResult LoadSkeletalModelFromVfs(std::string_view vpath)
{
    SkeletalLoadResult result;
    const std::vector<uint8_t> bytes = VfsRead::Active().ReadBytes(vpath);
    if (bytes.empty())
    {
        result.Success = false;
        result.ErrorMessage = "Failed to read file from VFS: " + std::string(vpath);
        return result;
    }

    return LoadSkeletalModelFromMemory(bytes.data(), bytes.size(), "glb");
}

#endif
