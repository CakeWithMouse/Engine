#pragma once

#include <d3d11.h>
#include <string>
#include <wrl/client.h>
#include <glm/glm.hpp>
#include <Engine/Render/MeshData.h>

class Game;

/**
 * Angles captured in the atlas. Defaults suit a strategy camera looking down at the field.
 * The shader blends the two neighbouring elevation rows, so a few rows are enough: a hard switch
 * between them would draw rings on the field, because on flat ground the elevation of the view
 * depends only on the distance to the camera.
 */
struct ImpostorAtlasSettings
{
    int YawCount = 16;             // tiles around the model
    int PitchCount = 4;            // tiles from the horizon upwards
    int TileSize = 128;            // pixels per tile
    float MinPitchDegrees = 0.0f;  // lowest captured elevation
    float MaxPitchDegrees = 75.0f; // highest captured elevation
};

/**
 * Pre-rendered views of a model used as its farthest detail level.
 * Built once from the mesh the designers provide: nothing has to be authored by hand.
 * Stores colour with coverage and an object-space normal, so impostors are lit like meshes.
 */
class ImpostorAtlas
{
public:
    bool Build(Game& game,
               const MeshLodData& mesh,
               ID3D11ShaderResourceView* modelTexture,
               const glm::vec4& baseColor,
               const ImpostorAtlasSettings& settings);

    bool IsValid() const { return AlbedoSRV != nullptr && NormalSRV != nullptr; }
    ID3D11ShaderResourceView* GetAlbedoSRV() const { return AlbedoSRV.Get(); }
    ID3D11ShaderResourceView* GetNormalSRV() const { return NormalSRV.Get(); }
    const ImpostorAtlasSettings& GetSettings() const { return Settings; }
    const MeshBounds& GetBounds() const { return Bounds; }
    /** Coarsest mip the shader may use; below it a tile would be too small to keep a silhouette. */
    float GetMaxMipLevel() const { return static_cast<float>(MipLevels - 1); }
    /** Writes every mip of both atlas layers as BMP files, to see what the sprites sample. */
    bool Dump(Game& game, const std::string& pathPrefix) const;

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> AlbedoTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AlbedoSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> NormalTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> NormalSRV;
    ImpostorAtlasSettings Settings;
    MeshBounds Bounds;
    int MipLevels = 1;
};
