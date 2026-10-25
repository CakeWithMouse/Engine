#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <glm/glm.hpp>
#include <Engine/Render/MeshData.h>

class Game;
class Material;

/** Angles captured in the atlas. Defaults suit a strategy camera looking down at the field. */
struct ImpostorAtlasSettings
{
    int YawCount = 16;             // tiles around the model
    int PitchCount = 3;            // tiles from the horizon upwards
    int TileSize = 128;            // pixels per tile
    float MinPitchDegrees = 0.0f;  // lowest captured elevation
    float MaxPitchDegrees = 60.0f; // highest captured elevation
};

/**
 * Pre-rendered views of a model used as its farthest detail level.
 * Built once from the mesh the designers provide: nothing has to be authored by hand.
 * Stores colour with coverage and an object-space normal, so impostors are lit like meshes.
 */
class ImpostorAtlas
{
public:
    /**
     * material: bake the surface of this material (its albedo, normal and clip) instead of the mesh
     * colour and texture. A material that cannot bake (errors) falls back to the plain bake.
     */
    bool Build(Game& game,
               const MeshLodData& mesh,
               ID3D11ShaderResourceView* modelTexture,
               const glm::vec4& baseColor,
               const ImpostorAtlasSettings& settings,
               Material* material = nullptr);

    bool IsValid() const { return AlbedoSRV != nullptr && NormalSRV != nullptr; }
    ID3D11ShaderResourceView* GetAlbedoSRV() const { return AlbedoSRV.Get(); }
    ID3D11ShaderResourceView* GetNormalSRV() const { return NormalSRV.Get(); }
    const ImpostorAtlasSettings& GetSettings() const { return Settings; }
    const MeshBounds& GetBounds() const { return Bounds; }
    /** Half of a texel in tile space: keeps sampling away from the border of a tile. */
    float GetTileInset() const { return TileInset; }

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> AlbedoTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AlbedoSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> NormalTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> NormalSRV;
    ImpostorAtlasSettings Settings;
    MeshBounds Bounds;
    float TileInset = 0.0f;
};
