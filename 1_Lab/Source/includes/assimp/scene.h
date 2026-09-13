#pragma once

#define AI_SCENE_FLAGS_INCOMPLETE 0x1
#define AI_MATKEY_COLOR_DIFFUSE "$clr.diffuse", 0, 0

struct aiVector3D
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct aiColor4D
{
    aiColor4D(float inR, float inG, float inB, float inA)
        : r(inR), g(inG), b(inB), a(inA)
    {
    }

    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct aiMatrix4x4
{
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, a4 = 0.0f;
    float b1 = 0.0f, b2 = 1.0f, b3 = 0.0f, b4 = 0.0f;
    float c1 = 0.0f, c2 = 0.0f, c3 = 1.0f, c4 = 0.0f;
    float d1 = 0.0f, d2 = 0.0f, d3 = 0.0f, d4 = 1.0f;
};

struct aiFace
{
    unsigned int mNumIndices = 0;
    unsigned int* mIndices = nullptr;
};

struct aiMaterial
{
    int Get(const char*, unsigned int, unsigned int, aiColor4D& color) const
    {
        color = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
        return 0;
    }
};

struct aiMesh
{
    unsigned int mNumVertices = 0;
    aiVector3D* mVertices = nullptr;
    aiVector3D* mNormals = nullptr;
    aiVector3D** mTextureCoords = nullptr;
    unsigned int mNumFaces = 0;
    aiFace* mFaces = nullptr;
    unsigned int mMaterialIndex = 0;

    bool HasNormals() const
    {
        return mNormals != nullptr;
    }

    bool HasTextureCoords(unsigned int channel) const
    {
        return mTextureCoords != nullptr && mTextureCoords[channel] != nullptr;
    }
};

struct aiNode
{
    unsigned int mNumMeshes = 0;
    unsigned int* mMeshes = nullptr;
    unsigned int mNumChildren = 0;
    aiNode** mChildren = nullptr;
};

struct aiScene
{
    unsigned int mFlags = 0;
    aiNode* mRootNode = nullptr;
    aiMesh** mMeshes = nullptr;
    aiMaterial** mMaterials = nullptr;
};
