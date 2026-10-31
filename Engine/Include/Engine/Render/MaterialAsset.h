#pragma once

#include <Engine/Render/Material.h>

#include <string>
#include <utility>
#include <vector>

/**
 * Contents of a .mat file (JSON, UTF-8):
 *
 *     {
 *       "version": 1,
 *       "shader": "Gold.hlsl",
 *       "render": { "blend": "opaque", "alphaClip": 0.5, "lighting": "lit", "twoSided": false, "castShadows": true },
 *       "params": { "Tint": [1.0, 0.78, 0.34, 1.0], "Gloss": 0.8 },
 *       "textures": { "BaseMap": "Textures/gold.png", "MaskMap": null }
 *     }
 *
 * Only "shader" is required. Paths are relative to the .mat file (absolute paths are accepted).
 * blend: opaque | masked | translucent | additive; lighting: lit | unlit. A parameter is a number
 * or an array of 1-4 numbers; a texture is a path or null (white).
 *
 * Saving rewrites the known fields and keeps everything else of the original file: unknown keys,
 * their order and parameters the shader no longer declares.
 */
class MaterialAsset
{
public:
    static constexpr int CurrentVersion = 1;

    std::string Shader; // as written in the file
    MaterialRenderState Render;
    std::vector<std::pair<std::string, std::vector<float>>> Params;
    std::vector<std::pair<std::string, std::string>> Textures; // empty path: none

    /** Reads and validates a file. On failure returns false and a message naming the file and field. */
    static bool Load(const std::string& matPath, MaterialAsset& asset, std::string& error);
    /** Writes the file (replacing it atomically), merged into the original text the asset was loaded from. */
    bool Save(const std::string& matPath, std::string& error) const;

    /** Absolute path of a path written in the file at matPath. */
    static std::string ResolvePath(const std::string& matPath, const std::string& relativePath);
    /** Path to write into the file at matPath: relative to its folder when possible. */
    static std::string MakeRelativePath(const std::string& matPath, const std::string& absolutePath);

    /**
     * Applies parameters and textures to a compiled material. Names the shader does not declare,
     * unreadable textures and values of the wrong size are reported in warnings and skipped.
     */
    void ApplyTo(Material& material, const std::string& matPath, std::vector<std::string>& warnings) const;
    /**
     * Captures the state of a material for saving at matPath. Values of the material replace those
     * of `base`; the rest of base (unknown fields, parameters the shader dropped) is kept.
     */
    static MaterialAsset FromMaterial(const Material& material, const std::string& matPath, const MaterialAsset& base);

private:
    std::string OriginalText; // the loaded file, so that saving keeps what the engine does not know
};
