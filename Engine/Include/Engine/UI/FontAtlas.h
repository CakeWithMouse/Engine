#pragma once

#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <wrl/client.h>

/** Which face to rasterise, and how big. Sizes are in pixels, as everything in the UI layer is. */
struct FontAtlasSettings
{
    std::wstring Face = L"Consolas";
    int PixelHeight = 18;
    int Weight = 400;
};

/** One character in the atlas: where it sits and how far the pen moves after it. */
struct FontGlyph
{
    float U0 = 0.0f;
    float V0 = 0.0f;
    float U1 = 0.0f;
    float V1 = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
    float Advance = 0.0f;
};

/**
 * A font rendered once into a texture, plus a solid white block the rest of the UI draws with.
 *
 * The block is why everything the UI draws - panels, bars, text - is one texture and one draw
 * call: a panel samples the white texel and keeps its own colour, a glyph samples its own cell
 * and takes the coverage from it.
 *
 * Glyphs are rasterised by GDI, which is what gives the atlas hinting, antialiasing and Cyrillic
 * without carrying a font file or a shaping library in the repository.
 */
class FontAtlas
{
public:
    bool Build(ID3D11Device* device, const FontAtlasSettings& settings = FontAtlasSettings{});

    bool IsValid() const { return TextureSRV != nullptr; }
    ID3D11ShaderResourceView* GetSRV() const { return TextureSRV.Get(); }
    /** Height of one line, including the space above and below the letters. */
    float GetLineHeight() const { return LineHeight; }
    /** Centre of the solid block: a panel uses it as its only texture coordinate. */
    void GetWhiteTexel(float& outU, float& outV) const { outU = WhiteU; outV = WhiteV; }
    /** Nothing is returned for a character the atlas does not hold; the caller skips it. */
    const FontGlyph* FindGlyph(wchar_t character) const;
    /** Width of a string in pixels at the size it was rasterised. */
    float MeasureWidth(const std::wstring& text) const;

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TextureSRV;
    std::unordered_map<wchar_t, FontGlyph> Glyphs;
    float LineHeight = 0.0f;
    float WhiteU = 0.0f;
    float WhiteV = 0.0f;
};
