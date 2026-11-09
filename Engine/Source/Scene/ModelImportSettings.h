#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

struct ModelImportSettings
{
    int version = 1;
    std::string kind = "model";

    float scale = 1.0f;
    bool useFileUnitScale = false;
    std::string normals = "file"; // "file" | "flat" | "smooth"
    float smoothingAngle = 60.0f; // [0.0, 175.0]
    bool importMaterials = true;
    bool importTextures = true;
    bool flipUv = false;
    bool mergeMeshes = true;

    // R1: version >= 2 means Engine bakes model.scale into geometry at load time and the
    // spawned Transform stays at 1. Version 1 / legacy keeps scale on the Transform only.
    bool AppliesGeometryScale() const { return version >= 2; }

    static constexpr float MinScale = 1e-4f;
    static constexpr float MaxScale = 1e4f;
    static constexpr float DefaultScale = 1.0f;

    static constexpr float MinAngle = 0.0f;
    static constexpr float MaxAngle = 175.0f;
    static constexpr float DefaultSmoothingAngle = 60.0f;

    static bool IsValidScale(float scale);
    static bool IsValidSmoothingAngle(float angle);
    static bool IsValidNormals(std::string_view normals);

    // Resolves sidecar virtual path for an asset (e.g. "path/model.glb" -> "path/model.glb.importsettings.json").
    // If the path already ends with ".importsettings.json", it is returned unchanged.
    static std::string ResolveSidecarPath(std::string_view assetPath);

    // Parses JSON string into ModelImportSettings. Returns true on success, false on error.
    // On error, outSettings is reset to default settings and error details are written to outError (if provided).
    static bool Parse(std::string_view jsonText, ModelImportSettings& outSettings, std::string* outError = nullptr);

    // Reads sidecar for an asset via VFS (VfsRead::Active()).
    // If sidecar does not exist, returns default ModelImportSettings without error.
    // If sidecar exists but is corrupt or invalid, logs [Error] to stderr and returns default settings without throwing.
    static ModelImportSettings ReadFromVfs(std::string_view assetPath);
};

namespace ModelImport
{
    using Settings = ::ModelImportSettings;
}

