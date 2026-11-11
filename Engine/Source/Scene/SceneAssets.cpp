#include <Engine/Scene/SceneAssets.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/MaterialAsset.h>
#include <Engine/Render/TextureLoader.h>
#include <Engine/Render/SkeletalLoader.h>
#include "ModelImport.h"
#include "../Resources/VfsRead.h"
#include "../../ThirdParty/nlohmann/json.hpp"
#include <fstream>
#include <set>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;
namespace ModelImport
{
// Defined in ModelImport.cpp: one-shot aiScene capture between CreateModel and
// CreateMaterials (declared here to keep ModelImport.h untouched).
void ArmSceneCapture(const std::string& modelPath);
void ClearSceneCapture();
}
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
bool ValidPathUnder(std::string_view path, std::string_view root)
{
    if (path.empty()) return true;
    if (path.size() > 2048 || path.rfind(root, 0) != 0 || path.size() == root.size() ||
        path.find_first_of("\\:\0", 0, 3) != std::string_view::npos || path.back() == '/') return false;
    size_t begin = root.size();
    while (begin < path.size())
    {
        auto end = path.find('/', begin); if (end == std::string_view::npos) end = path.size();
        auto part = path.substr(begin, end - begin);
        if (part.empty() || part == "." || part == "..") return false;
        begin = end + 1;
    }
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), nullptr, 0) != 0;
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
    return ValidPathUnder(path, "/Game/");
}
void ValidateTexture(std::string_view path)
{
    if (!ValidPath(path)) throw std::runtime_error("Expected a texture path inside /Game");
    if (!path.empty() && !TextureLoader::ValidateImage(VfsRead::Active().ReadBytes(path)))
        throw std::runtime_error("Missing or invalid texture: " + std::string(path));
}
std::unique_ptr<GameComponent> CreateModel(const std::string& path, bool useImportedMaterial)
{
    const std::string stagingPrefix = std::string(StagingRoot) + "/";
    if (path.empty() || !(ValidPath(path) || ValidPathUnder(path, stagingPrefix)))
        throw std::runtime_error("Expected a model path inside /Game");
    auto model = LoadSkeletalModelFromVfs(path);
    if (!model.Success || !model.Model)
    {
        std::string err = model.ErrorMessage;
        if (err == "Assimp not enabled")
        {
            err += " (Сборка выполнена без поддержки Assimp. Установите зависимости в Engine/vcpkg_installed и соберите с Assimp / Build with -EnableAssimp)";
        }
        throw std::runtime_error("Cannot load model '" + path + "': " + err);
    }
    auto object = std::make_unique<SkeletalMeshComponent>();
    // Keep the asset reference and the loaded CPU model without importing twice.
    if (!object->SetAssetModel(path, std::move(model.Model))) throw std::runtime_error("Invalid model");
    if(useImportedMaterial) {
        const auto slots=ModelImport::MaterialSlots(path);
        if(slots.size()>1)for(const auto& slot:slots)
            if(!object->SetMaterialSlot(slot.first,slot.second))throw std::runtime_error("Imported material slot does not match geometry");
        // Preserve the ordinary single-material property for one-slot models.
        if(slots.size()==1)object->SetMaterialAsset(slots.begin()->second);
    }
    return object;
}
void ValidateMaterialPath(std::string_view path)
{
    if (!ValidPath(path) || (!path.empty() && Extension(fs::u8path(std::string(path))) != ".mat"))
        throw std::runtime_error("Expected a .mat path inside /Game");
}
void ValidateMaterial(std::string_view path)
{
    ValidateMaterialPath(path);
    if (path.empty()) return;
    const auto physical = VfsRead::Active().Resolve(path);
    MaterialAsset asset;
    std::string error;
    if (physical.empty() || !MaterialAsset::Load(physical, asset, error))
        throw std::runtime_error("Missing or invalid material: " + (error.empty() ? std::string(path) : error));
}
void Apply(GameComponent& object, const std::string& model, const std::string& texture, const std::string& material)
{
    if (object.GetModelAsset() != model) {
        // A skeletal subclass owns its geometry and animation state directly.
        // A generic visual proxy would leave its slot codec and bone palette empty.
        if (auto* skeletal = dynamic_cast<SkeletalMeshComponent*>(&object); skeletal && !model.empty()) {
            if (!skeletal->LoadFromVfs(model)) throw std::runtime_error("Cannot restore skeletal model: " + model);
        } else object.SetModelAsset(model);
    }
    ValidateTexture(texture);
    if (!object.TrySetTexture(texture)) throw std::runtime_error("Cannot create texture resources");
    // A missing or broken .mat does not stop the scene: the game draws the error material instead.
    ValidateMaterialPath(material);
    if (!object.SetMaterialAsset(material)) throw std::runtime_error("This object cannot take a material");
}
std::string Import(const fs::path& content, const fs::path& source, Kind kind, std::string_view optionsJson,
                   const fs::path& stagingRoot, std::string_view virtualRoot)
{
    const bool staged = !stagingRoot.empty();
    const auto base = staged ? stagingRoot : content;
    if (!source.is_absolute() || !fs::is_regular_file(source) || !fs::is_directory(base))
        throw std::runtime_error("Select an existing file and configure the project Content directory");
    const auto extension = Extension(source);
    if (kind == Texture)
    {
        if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".bmp" && extension != ".tga")
            throw std::runtime_error("Supported textures: PNG, JPG, BMP, TGA");
        if (!TextureLoader::ValidateImage(Read(source))) throw std::runtime_error("Invalid or oversized image");
    }
    else if (kind == Audio)
    {
        if (extension != ".wav" && extension != ".ogg" && extension != ".mp3" && extension != ".flac")
            throw std::runtime_error("Supported audio: WAV, OGG, MP3, FLAC");
    }
    else if (kind != Model || (extension != ".glb" && extension != ".gltf" && extension != ".obj" && extension != ".fbx" && extension != ".blend"))
        throw std::runtime_error("Supported models: GLB, glTF, OBJ, FBX, BLEND (requires Blender)");
    ModelImportSettings settings{};
    if (kind == Model && !optionsJson.empty())
    {
        std::string parseErr;
        if (!ModelImportSettings::Parse(optionsJson, settings, &parseErr))
            throw std::runtime_error("Invalid model import options: " + parseErr);
    }
    CheckedSize(source);
    const auto group = base / (kind == Model ? "Models" : kind == Texture ? "Textures" : "Audio");
    if (!Inside(base, group)) throw std::runtime_error("Asset directory is outside project Content");
    fs::create_directories(group);
    for (unsigned suffix = 0; suffix < 10000; ++suffix)
    {
        const auto name = source.stem().u8string() + (suffix ? "_" + std::to_string(suffix) : "");
        const auto target = group / fs::u8path(kind == Model ? name : name + extension);
        if (!Inside(base, target)) throw std::runtime_error("Invalid asset destination");
        if (fs::exists(target)) continue;
        const auto filename = extension == ".blend" ? fs::u8path(source.stem().u8string() + ".glb") : source.filename();
        const auto file = kind == Model ? target / filename : target;
        const auto virtualPath = std::string(virtualRoot) + "/" + file.lexically_relative(base).generic_u8string();
        const std::string stagingPrefix = std::string(StagingRoot) + "/";
        if (!(staged ? ValidPathUnder(virtualPath, stagingPrefix) : ValidPath(virtualPath)))
            throw std::runtime_error("Asset filename is not a valid project path");
        if (kind == Model && !fs::create_directory(target)) continue;
        bool copied = false;
        try
        {
            if (extension == ".blend") ModelImport::ConvertBlend(source, file);
            else fs::copy_file(source, file);
            copied = true;
            if (kind == Model)
            {
                if (extension == ".gltf") CopyGltfDependencies(source, target);
                const auto sidecarPath = fs::u8path(file.u8string() + ".importsettings.json");
                nlohmann::json sidecarDoc = {
                    {"version", settings.version > 0 ? settings.version : 1},
                    {"kind", settings.kind.empty() ? "model" : settings.kind},
                    {"model", {
                        {"scale", settings.scale},
                        {"useFileUnitScale", settings.useFileUnitScale},
                        {"normals", settings.normals},
                        {"smoothingAngle", settings.smoothingAngle},
                        {"importMaterials", settings.importMaterials},
                        {"importTextures", settings.importTextures},
                        {"flipUv", settings.flipUv},
                        {"mergeMeshes", settings.mergeMeshes}
                    }}
                };
                std::ofstream sidecarOut(sidecarPath, std::ios::binary);
                sidecarOut.exceptions(std::ios::failbit | std::ios::badbit);
                sidecarOut << sidecarDoc.dump(2) << "\n";
                sidecarOut.close();

                // CreateModel must run before CreateMaterials: it validates geometry
                // while .import.json does not exist yet, so MaterialSlots stays empty
                // and no /Game material validation hits transient staging paths.
                ModelImport::ArmSceneCapture(virtualPath);
                CreateModel(virtualPath); // Validate geometry before reporting success.
                if (extension == ".fbx" || extension == ".blend" || extension == ".glb" || extension == ".gltf")
                    ModelImport::CreateMaterials(source, target, virtualPath, settings);
                ModelImport::ClearSceneCapture();
            }
            std::ofstream metadata(fs::u8path(file.u8string()+".source.json"),std::ios::binary);
            metadata.exceptions(std::ios::failbit|std::ios::badbit);
            metadata << nlohmann::json{{"version",1},{"source",source.u8string()},{"kind",static_cast<unsigned>(kind)}}.dump(2);
            metadata.close();
            return virtualPath;
        }
        catch (...)
        {
            // Only the new path created by this import is rolled back.
            ModelImport::ClearSceneCapture();
            std::error_code ignored;
            if (kind == Model) fs::remove(fs::u8path(file.u8string() + ".importsettings.json"), ignored);
            if ((kind == Model || copied) && Inside(base, target)) fs::remove_all(target, ignored);
            if (kind == Texture && copied) fs::remove(fs::u8path(file.u8string()+".source.json"), ignored);
            throw;
        }
    }
    throw std::runtime_error("Too many assets with the same name");
}
}
