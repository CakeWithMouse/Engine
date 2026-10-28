#include "ModelImport.h"
#include <Engine/Scene/SceneAssets.h>
#include <Engine/Render/TextureLoader.h>
#include "../Resources/VfsRead.h"
#include "../../ThirdParty/nlohmann/json.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <set>

#if defined(ENGINE_ENABLE_ASSIMP)
#include "../Render/AssimpVfsIO.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/scene.h>
#endif

namespace fs = std::filesystem;
using Json = nlohmann::json;
namespace
{
constexpr size_t MaxFileSize = 256u * 1024 * 1024;
void Write(const fs::path& path, const void* data, size_t size)
{
    std::ofstream out(path, std::ios::binary);
    out.write(static_cast<const char*>(data), size);
    out.close();
    if (!out) throw std::runtime_error("Cannot write imported asset: " + path.u8string());
}
void Write(const fs::path& path, const std::string& text) { Write(path, text.data(), text.size()); }

#if defined(ENGINE_ENABLE_ASSIMP)
std::vector<uint8_t> ReadImage(const fs::path& path)
{
    const auto size = fs::file_size(path);
    if (!size || size > MaxFileSize) throw std::runtime_error("Texture is empty or exceeds 256 MiB: " + path.u8string());
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    std::ifstream in(path, std::ios::binary);
    if (!in.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
        throw std::runtime_error("Cannot read texture: " + path.u8string());
    return bytes;
}

fs::path ExternalTexture(const fs::path& source, std::string reference)
{
    std::replace(reference.begin(), reference.end(), '\\', '/');
    // Blender's // paths are relative to the .blend, not network shares.
    if (reference.rfind("//", 0) == 0) reference.erase(0, 2);
    const auto path = fs::u8path(reference), folder = source.parent_path();
    // FBX often retains an author's absolute path. Prefer the delivered relative file,
    // then the original path, then the usual sibling and FBX media folders.
    const fs::path candidates[] = {
        path.is_relative() ? folder / path : fs::path{}, path.is_absolute() ? path : fs::path{},
        folder / path.filename(), folder / (source.stem().wstring() + L".fbm") / path.filename(),
        folder / "Textures" / path.filename(), folder / "textures" / path.filename()
    };
    for (const auto& candidate : candidates)
        if (!candidate.empty() && fs::is_regular_file(candidate)) return candidate;
    throw std::runtime_error("Missing base-color texture '" + reference + "' for " + source.u8string());
}

std::string ImageExtension(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() >= 8 && bytes[0] == 137 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') return ".png";
    if (bytes.size() >= 3 && bytes[0] == 255 && bytes[1] == 216 && bytes[2] == 255) return ".jpg";
    if (bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M') return ".bmp";
    // STB validates the actual encoding. Other supported images (including TGA) need
    // no conversion; material textures are decoded from their bytes, not their suffix.
    if (bytes.size() >= 18 && (bytes[2] == 2 || bytes[2] == 10) && (bytes[16] == 24 || bytes[16] == 32)) return ".tga";
    return ".image";
}

std::vector<uint8_t> EmbeddedImage(const aiTexture& texture)
{
    if (!texture.pcData) throw std::runtime_error("Empty embedded texture");
    if (!texture.mHeight)
    {
        if (!texture.mWidth || texture.mWidth > MaxFileSize) throw std::runtime_error("Embedded image exceeds 256 MiB or is empty");
        const auto* data = reinterpret_cast<const uint8_t*>(texture.pcData);
        return {data, data + texture.mWidth};
    }
    const uint64_t pixels = uint64_t(texture.mWidth) * texture.mHeight;
    if (!pixels || texture.mWidth > 16384 || texture.mHeight > 16384 || pixels * 4 + 18 > MaxFileSize)
        throw std::runtime_error("Oversized embedded texture");
    // Uncompressed Assimp texels are BGRA. A top-left-origin 32-bit TGA preserves them.
    std::vector<uint8_t> bytes(static_cast<size_t>(18 + pixels * 4), 0);
    bytes[2] = 2; bytes[12] = uint8_t(texture.mWidth); bytes[13] = uint8_t(texture.mWidth >> 8);
    bytes[14] = uint8_t(texture.mHeight); bytes[15] = uint8_t(texture.mHeight >> 8);
    bytes[16] = 32; bytes[17] = 0x28;
    for (size_t i = 0; i < pixels; ++i)
    {
        bytes[18+i*4] = texture.pcData[i].b; bytes[19+i*4] = texture.pcData[i].g;
        bytes[20+i*4] = texture.pcData[i].r; bytes[21+i*4] = texture.pcData[i].a;
    }
    return bytes;
}

const char* Surface = R"(// Basic imported material. Edit Tint and BaseMap in the material editor.
cbuffer MaterialParams : register(b1) { float4 Tint = float4(1, 1, 1, 1); };
Texture2D BaseMap : register(t6);
void Surface(MaterialInput i, inout MaterialOutput o)
{
    float4 color = BaseMap.Sample(MaterialSampler, i.UV) * Tint;
    o.Albedo *= color.rgb;
    o.Alpha *= color.a;
}
)";
#endif
}

void ModelImport::CreateMaterials(const fs::path& source, const fs::path& destination, const std::string& modelPath)
{
#if defined(ENGINE_ENABLE_ASSIMP)
    Assimp::Importer importer;
    importer.SetIOHandler(new AssimpVfsIO(modelPath));
    const aiScene* scene = importer.ReadFile(modelPath, 0);
    if (!scene || !scene->mRootNode) throw std::runtime_error("Cannot read model materials: " + std::string(importer.GetErrorString()));
    fs::create_directory(destination / "Materials");
    fs::create_directory(destination / "Textures");
    Write(destination / "Materials/Imported.hlsl", Surface);
    std::map<std::string, std::string> textures;
    std::set<unsigned> written;
    Json manifest = {{"version", 1}, {"materials", Json::array()}};
    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        const auto index = scene->mMeshes[m]->mMaterialIndex;
        if (!written.insert(index).second) continue;
        if (index >= scene->mNumMaterials) throw std::runtime_error("Invalid model material index");
        const auto& material = *scene->mMaterials[index];
        aiColor4D tint(1, 1, 1, 1);
        if (material.Get(AI_MATKEY_BASE_COLOR, tint) != AI_SUCCESS) material.Get(AI_MATKEY_COLOR_DIFFUSE, tint);
        aiString name, reference;
        material.Get(AI_MATKEY_NAME, name);
        const auto type = material.GetTextureCount(aiTextureType_BASE_COLOR) ? aiTextureType_BASE_COLOR : aiTextureType_DIFFUSE;
        unsigned uv = 0;
        std::string texturePath;
        if (material.GetTexture(type, 0, &reference, nullptr, &uv) == AI_SUCCESS)
        {
            if (uv != 0) throw std::runtime_error("Base-color texture requires UV channel " + std::to_string(uv) + "; only UV0 is supported");
            const aiTexture* embedded = scene->GetEmbeddedTexture(reference.C_Str());
            const auto external = embedded ? fs::path{} : ExternalTexture(source, reference.C_Str());
            const auto key = embedded ? std::string("embedded:") + reference.C_Str() : fs::weakly_canonical(external).u8string();
            auto found = textures.find(key);
            if (found == textures.end())
            {
                auto bytes = embedded ? EmbeddedImage(*embedded) : ReadImage(external);
                if (!TextureLoader::ValidateImage(bytes)) throw std::runtime_error("Invalid or oversized base-color texture: " + std::string(reference.C_Str()));
                texturePath = "Textures/BaseColor_" + std::to_string(textures.size()) + ImageExtension(bytes);
                Write(destination / fs::u8path(texturePath), bytes.data(), bytes.size());
                textures.emplace(key, texturePath);
            }
            else texturePath = found->second;
        }
        const auto relative = "Materials/Material_" + std::to_string(index) + ".mat";
        Json mat = {{"version", 1}, {"shader", "Imported.hlsl"},
                    {"params", {{"Tint", {tint.r, tint.g, tint.b, tint.a}}}},
                    {"textures", {{"BaseMap", texturePath.empty() ? Json(nullptr) : Json("../" + texturePath)}}},
                    {"sourceMaterial", std::string(name.C_Str())}};
        Write(destination / fs::u8path(relative), mat.dump(2) + "\n");
        if (!manifest.contains("defaultMaterial")) manifest["defaultMaterial"] = relative;
        manifest["materials"].push_back(relative);
    }
    if (written.size() > 1)
        manifest["note"] = "One material per object: the first mesh material is assigned; other imported materials can be assigned manually.";
    Write(fs::u8path((destination / fs::u8path(modelPath).filename()).u8string() + ".import.json"), manifest.dump(2) + "\n");
#else
    (void)source; (void)destination; (void)modelPath;
    throw std::runtime_error("Model import requires an engine built with Assimp");
#endif
}

std::string ModelImport::DefaultMaterial(const std::string& modelPath)
{
    const auto sidecar = modelPath + ".import.json";
    if (!VfsRead::Active().Exists(sidecar)) return {};
    const auto bytes = VfsRead::Active().ReadBytes(sidecar);
    if (bytes.size() > 1024 * 1024) throw std::runtime_error("Model import metadata is too large");
    const auto json = Json::parse(bytes.begin(), bytes.end());
    if (json.at("version") != 1) throw std::runtime_error("Unsupported model import metadata version");
    const auto relative = json.value("defaultMaterial", std::string{});
    if (relative.empty()) return {};
    if (!SceneAssets::ValidPath("/Game/" + relative)) throw std::runtime_error("Invalid imported material path");
    return modelPath.substr(0, modelPath.find_last_of('/') + 1) + relative;
}
