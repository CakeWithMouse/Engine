#include <Engine/Scene/SceneAssets.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/TextureLoader.h>
#include <Engine/Render/SkeletalLoader.h>
#include "../Resources/VfsRead.h"
#include "../../ThirdParty/nlohmann/json.hpp"
#include <fstream>
#include <set>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;
namespace
{
std::string Extension(const fs::path& path)
{
    auto value = path.extension().u8string();
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}
uintmax_t CheckedSize(const fs::path& path)
{
    const auto size = fs::file_size(path);
    if (!size) throw std::runtime_error("Asset file is empty (0 bytes): " + path.u8string());
    if (size > 256u * 1024 * 1024)
        throw std::runtime_error("Asset exceeds the import limit of 256 MiB (268435456 bytes): " +
            path.u8string() + " (" + std::to_string(size) + " bytes)");
    return size;
}
std::vector<uint8_t> Read(const fs::path& path)
{
    const auto size = CheckedSize(path);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) throw std::runtime_error("Cannot read asset");
    return bytes;
}
bool Inside(const fs::path& root, const fs::path& path)
{
    const auto relative = fs::weakly_canonical(path).lexically_relative(fs::canonical(root));
    if (relative.empty() || relative.is_absolute()) return false;
    for (const auto& part : relative) if (part == "..") return false;
    return true;
}
std::string DecodeUri(const std::string& uri)
{
    std::string out;
    auto hex = [](char c) -> int { if (c >= '0' && c <= '9') return c - '0'; c = char(std::tolower((unsigned char)c)); return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
    for (size_t i = 0; i < uri.size(); ++i)
    {
        if (uri[i] != '%') { out += uri[i]; continue; }
        if (i + 2 >= uri.size() || hex(uri[i+1]) < 0 || hex(uri[i+2]) < 0) throw std::runtime_error("Invalid glTF URI");
        out += char(hex(uri[i+1]) * 16 + hex(uri[i+2])); i += 2;
    }
    return out;
}
void CopyGltfDependencies(const fs::path& source, const fs::path& destination)
{
    const auto bytes = Read(source);
    const auto json = nlohmann::json::parse(bytes.begin(), bytes.end());
    std::set<std::string> copied;
    for (const auto* key : {"buffers", "images"})
    {
        if (!json.contains(key)) continue;
        for (const auto& entry : json.at(key))
        {
            if (!entry.contains("uri")) continue;
            const auto uri = entry.at("uri").get<std::string>();
            if (uri.rfind("data:", 0) == 0) continue;
            auto relative = DecodeUri(uri);
            if (!SceneAssets::ValidPath("/Game/" + relative)) throw std::runtime_error("glTF dependencies must be relative files inside the model folder");
            if (!copied.insert(relative).second) continue;
            const auto path = fs::u8path(relative);
            const auto from = source.parent_path() / path, to = destination / path;
            if (!Inside(source.parent_path(), from) || !fs::is_regular_file(from))
                throw std::runtime_error("Missing or external glTF dependency: " + relative);
            CheckedSize(from); // validate without allocating a second copy of the dependency
            fs::create_directories(to.parent_path());
            fs::copy_file(from, to); // never replace another file
        }
    }
}
}
namespace SceneAssets
{
bool ValidPath(std::string_view path)
{
    if (path.empty()) return true;
    if (path.size() > 2048 || path.rfind("/Game/", 0) != 0 || path.size() == 6 ||
        path.find_first_of("\\:\0", 0, 3) != std::string_view::npos || path.back() == '/') return false;
    size_t begin = 6;
    while (begin < path.size())
    {
        auto end = path.find('/', begin); if (end == std::string_view::npos) end = path.size();
        auto part = path.substr(begin, end - begin);
        if (part.empty() || part == "." || part == "..") return false;
        begin = end + 1;
    }
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), nullptr, 0) != 0;
}
void ValidateTexture(std::string_view path)
{
    if (!ValidPath(path)) throw std::runtime_error("Expected a texture path inside /Game");
    if (!path.empty() && !TextureLoader::ValidateImage(VfsRead::Active().ReadBytes(path)))
        throw std::runtime_error("Missing or invalid texture: " + std::string(path));
}
std::unique_ptr<GameComponent> CreateModel(const std::string& path)
{
    if (path.empty() || !ValidPath(path)) throw std::runtime_error("Expected a model path inside /Game");
    auto model = LoadSkeletalModelFromVfs(path);
    if (!model.Success || !model.Model) throw std::runtime_error("Cannot load model '" + path + "': " + model.ErrorMessage);
    auto object = std::make_unique<SkeletalMeshComponent>();
    // Keep the asset reference and the loaded CPU model without importing twice.
    if (!object->SetAssetModel(path, std::move(model.Model))) throw std::runtime_error("Invalid model");
    return object;
}
void Apply(GameComponent& object, const std::string& model, const std::string& texture)
{
    if (object.GetModelAsset() != model) throw std::runtime_error("Model asset does not match component type");
    ValidateTexture(texture);
    if (!object.TrySetTexture(texture)) throw std::runtime_error("Cannot create texture resources");
}
std::string Import(const fs::path& content, const fs::path& source, Kind kind)
{
    if (!source.is_absolute() || !fs::is_regular_file(source) || !fs::is_directory(content))
        throw std::runtime_error("Select an existing file and configure the project Content directory");
    const auto extension = Extension(source);
    if (kind == Texture)
    {
        if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".bmp" && extension != ".tga")
            throw std::runtime_error("Supported textures: PNG, JPG, BMP, TGA");
        if (!TextureLoader::ValidateImage(Read(source))) throw std::runtime_error("Invalid or oversized image");
    }
    else if (kind != Model || (extension != ".glb" && extension != ".gltf" && extension != ".obj" && extension != ".fbx"))
        throw std::runtime_error("Supported models: GLB, glTF, OBJ, FBX");
    CheckedSize(source);
    const auto group = content / (kind == Model ? "Models" : "Textures");
    if (!Inside(content, group)) throw std::runtime_error("Asset directory is outside project Content");
    fs::create_directories(group);
    for (unsigned suffix = 0; suffix < 10000; ++suffix)
    {
        const auto name = source.stem().u8string() + (suffix ? "_" + std::to_string(suffix) : "");
        const auto target = group / fs::u8path(kind == Model ? name : name + extension);
        if (!Inside(content, target)) throw std::runtime_error("Invalid asset destination");
        if (fs::exists(target)) continue;
        const auto file = kind == Model ? target / source.filename() : target;
        const auto virtualPath = "/Game/" + file.lexically_relative(content).generic_u8string();
        if (!ValidPath(virtualPath)) throw std::runtime_error("Asset filename is not a valid project path");
        if (kind == Model && !fs::create_directory(target)) continue;
        bool copied = false;
        try
        {
            fs::copy_file(source, file);
            copied = true;
            if (kind == Model)
            {
                if (extension == ".gltf") CopyGltfDependencies(source, target);
                CreateModel(virtualPath); // Validate geometry before reporting success.
            }
            return virtualPath;
        }
        catch (...)
        {
            // Only the new path created by this import is rolled back.
            std::error_code ignored;
            if ((kind == Model || copied) && Inside(content, target)) fs::remove_all(target, ignored);
            throw;
        }
    }
    throw std::runtime_error("Too many assets with the same name");
}
}
