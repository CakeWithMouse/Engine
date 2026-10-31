#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
class GameComponent;

namespace SceneAssets
{
    inline constexpr const char* ModelType = "Engine.Model";
    enum Kind : unsigned { Model = 0, Texture = 1 };
    // Scene references are portable, canonical /Game paths. Empty means no asset.
    bool ValidPath(std::string_view path);
    void ValidateTexture(std::string_view path);
    // A canonical /Game path of a .mat file; the file itself is not read.
    void ValidateMaterialPath(std::string_view path);
    // A readable .mat file (its shader is compiled later, by the game that draws it).
    void ValidateMaterial(std::string_view path);
    // Imported defaults are only for placement. Scene restore supplies its own saved material.
    std::unique_ptr<GameComponent> CreateModel(const std::string& path, bool useImportedMaterial = true);
    void Apply(GameComponent& object, const std::string& model, const std::string& texture,
               const std::string& material = {});
    // Copies into a new uniquely named location; never overwrites existing assets.
    std::string Import(const std::filesystem::path& content, const std::filesystem::path& source, Kind kind);
}
