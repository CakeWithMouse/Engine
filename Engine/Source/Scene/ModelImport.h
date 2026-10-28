#pragma once
#include <filesystem>
#include <string>

// Import-time helpers. All output goes into the new model directory; SceneAssets rolls it
// back as a unit on failure. Runtime scenes only depend on the resulting project assets.
namespace ModelImport
{
    void ConvertBlend(const std::filesystem::path& source, const std::filesystem::path& destination);
    void CreateMaterials(const std::filesystem::path& source, const std::filesystem::path& destination,
                         const std::string& modelPath);
    std::string DefaultMaterial(const std::string& modelPath);
}
