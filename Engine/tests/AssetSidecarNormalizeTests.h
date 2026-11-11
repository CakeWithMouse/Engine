#pragma once

#include "ModelReimportScaleTests.h"
#include "TestSkipped.h"
#include "../Source/Scene/AssetUpdate.h"
#include "../Source/Scene/ModelImportSettings.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace AssetSidecarNormalizeTests
{

namespace fs = std::filesystem;
using ModelGeometryScaleTests::Check;
using ModelGeometryScaleTests::WriteText;

inline std::string ReadAll(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

inline void Run()
{
#if !defined(ENGINE_ENABLE_ASSIMP)
    throw TestSkipped("sidecar normalization requires -EnableAssimp");
#else
    const auto stamp = std::to_string(static_cast<unsigned long long>(std::time(nullptr))) + "_" + std::to_string(std::clock());
    const auto root = fs::temp_directory_path() / ("SidecarNorm_" + stamp);
    const auto content = root / "Content";
    try
    {
        const auto modelDir = content / "Models" / "m";
        fs::create_directories(modelDir);
        const std::string objText = "v 0 0 0\nv 10 0 0\nv 10 10 0\nv 0 10 0\nf 1 2 3\nf 1 3 4\n";
        WriteText(modelDir / "m.obj", objText);
        WriteText(root / "m.obj", objText);
        const auto asset = modelDir / "m.obj";
        const auto sidecar = fs::path(asset.u8string() + ".importsettings.json");
        const auto noop = [](const std::string&, const std::string&) {};

        // Broken sidecar: reimport normalizes it to defaults and keeps a backup.
        WriteText(sidecar, "{broken json");
        AssetUpdate::Run(content, 0, "/Game/Models/m/m.obj", (root / "m.obj").u8string(), noop);
        ModelImportSettings parsed{};
        Check(ModelImportSettings::Parse(ReadAll(sidecar), parsed), "broken sidecar normalized to defaults");
        bool foundBackup = false;
        for (const auto& entry : fs::recursive_directory_iterator(root / ".painengine" / "AssetBackups"))
            if (entry.path().filename() == "importsettings.broken.json")
            {
                Check(ReadAll(entry.path()) == "{broken json", "broken sidecar backup keeps original bytes");
                foundBackup = true;
            }
        Check(foundBackup, "broken sidecar backed up before normalization");

        // Missing sidecar: reimport writes defaults.
        fs::remove(sidecar);
        AssetUpdate::Run(content, 0, "/Game/Models/m/m.obj", (root / "m.obj").u8string(), noop);
        Check(fs::is_regular_file(sidecar), "missing sidecar created with defaults");
        Check(ModelImportSettings::Parse(ReadAll(sidecar), parsed), "created sidecar parses");

        fs::remove_all(root);
    }
    catch (...)
    {
        fs::remove_all(root);
        throw;
    }
#endif
}

} // namespace AssetSidecarNormalizeTests