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
    std::unique_ptr<GameComponent> CreateModel(const std::string& path);
    void Apply(GameComponent& object, const std::string& model, const std::string& texture);
    // Copies into a new uniquely named location; never overwrites existing assets.
    std::string Import(const std::filesystem::path& content, const std::filesystem::path& source, Kind kind);
}
