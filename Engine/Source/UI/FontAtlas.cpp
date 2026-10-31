// min and max arrive as macros from windows.h and would eat std::min / std::max below.
#define NOMINMAX
#include <Engine/UI/FontAtlas.h>

#include <algorithm>
#include <iostream>
#include <vector>
#include <windows.h>

namespace
{
    /** Latin, digits and punctuation, then Cyrillic: enough for the labels a scene writes. */
    struct GlyphRange
    {
        wchar_t First;
        wchar_t Last;
    };

    constexpr GlyphRange GlyphRanges[] = {
        {L' ', L'~'},        // ASCII
        {0x0401, 0x0401},    // Ё
        {0x0410, 0x044F},    // А-я
        {0x0451, 0x0451}     // ё
    };

    // The solid block the panels sample, and the margin kept around every glyph so that bilinear
    // filtering cannot pull a neighbouring cell into it.
    constexpr int WhiteBlockSize = 4;
    constexpr int CellPadding = 1;

    /** A font name for a message. Narrowing the wide string by hand turns a Cyrillic name to mush. */
    std::string ToUtf8(const std::wstring& wide)
    {
        if (wide.empty())
        {
            return {};
        }
        const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                               nullptr, 0, nullptr, nullptr);
        if (needed <= 0)
        {
            return {};
        }
        std::string utf8(static_cast<size_t>(needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                            &utf8[0], needed, nullptr, nullptr);
        return utf8;
    }

    struct DeviceContextScope
    {
        HDC Dc = nullptr;
        HBITMAP Bitmap = nullptr;
        HGDIOBJ PreviousBitmap = nullptr;
        HFONT Font = nullptr;
        HGDIOBJ PreviousFont = nullptr;

        ~DeviceContextScope()
        {
            if (Dc != nullptr && PreviousFont != nullptr) SelectObject(Dc, PreviousFont);
            if (Font != nullptr) DeleteObject(Font);
            if (Dc != nullptr && PreviousBitmap != nullptr) SelectObject(Dc, PreviousBitmap);
            if (Bitmap != nullptr) DeleteObject(Bitmap);
            if (Dc != nullptr) DeleteDC(Dc);
        }
    };
}

bool FontAtlas::Build(ID3D11Device* device, const FontAtlasSettings& settings)
{
    if (device == nullptr)
    {
        return false;
    }

    Glyphs.clear();
    TextureSRV.Reset();
    Texture.Reset();

    const int pixelHeight = std::max(8, settings.PixelHeight);

    DeviceContextScope scope;
    scope.Dc = CreateCompatibleDC(nullptr);
    if (scope.Dc == nullptr)
    {
        std::cout << "FontAtlas: no device context." << std::endl;
        return false;
    }

    scope.Font = CreateFontW(-pixelHeight, 0, 0, 0, settings.Weight, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, settings.Face.c_str());
    if (scope.Font == nullptr)
    {
        std::cout << "FontAtlas: font '" << ToUtf8(settings.Face)
                  << "' could not be created." << std::endl;
        return false;
    }
    scope.PreviousFont = SelectObject(scope.Dc, scope.Font);

    TEXTMETRICW metrics = {};
    GetTextMetricsW(scope.Dc, &metrics);
    LineHeight = static_cast<float>(metrics.tmHeight);
    const int cellHeight = metrics.tmHeight + CellPadding * 2;

    // Lay the cells out first, so the texture is only as big as it has to be.
    struct PendingGlyph
    {
        wchar_t Character;
        int X;
        int Y;
        int Width;
        int Advance;
    };
    std::vector<PendingGlyph> pending;

    const int atlasWidth = 512;
    int penX = WhiteBlockSize + CellPadding * 2;
    int penY = 0;
    int usedHeight = cellHeight;

    for (const GlyphRange& range : GlyphRanges)
    {
        for (wchar_t character = range.First; character <= range.Last; ++character)
        {
            SIZE extent = {};
            if (!GetTextExtentPoint32W(scope.Dc, &character, 1, &extent) || extent.cx <= 0)
            {
                continue;
            }
            const int cellWidth = extent.cx + CellPadding * 2;
            if (penX + cellWidth > atlasWidth)
            {
                penX = 0;
                penY += cellHeight;
                usedHeight = penY + cellHeight;
            }
            pending.push_back({character, penX, penY, extent.cx, extent.cx});
            penX += cellWidth;
        }
    }

    // Power of two keeps the texture friendly to every sampler setting.
    int atlasHeight = 1;
    while (atlasHeight < usedHeight)
    {
        atlasHeight *= 2;
    }

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = atlasWidth;
    // Negative height: rows run top down, the same way the texture does.
    bitmapInfo.bmiHeader.biHeight = -atlasHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    scope.Bitmap = CreateDIBSection(scope.Dc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (scope.Bitmap == nullptr || bits == nullptr)
    {
        std::cout << "FontAtlas: could not create the glyph bitmap." << std::endl;
        return false;
    }
    scope.PreviousBitmap = SelectObject(scope.Dc, scope.Bitmap);

    const RECT fullRect = {0, 0, atlasWidth, atlasHeight};
    FillRect(scope.Dc, &fullRect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    const RECT whiteRect = {0, 0, WhiteBlockSize, WhiteBlockSize};
    FillRect(scope.Dc, &whiteRect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    SetBkMode(scope.Dc, TRANSPARENT);
    SetTextColor(scope.Dc, RGB(255, 255, 255));

    const float inverseWidth = 1.0f / static_cast<float>(atlasWidth);
    const float inverseHeight = 1.0f / static_cast<float>(atlasHeight);

    for (const PendingGlyph& glyph : pending)
    {
        TextOutW(scope.Dc, glyph.X + CellPadding, glyph.Y + CellPadding, &glyph.Character, 1);

        FontGlyph entry;
        entry.U0 = static_cast<float>(glyph.X + CellPadding) * inverseWidth;
        entry.V0 = static_cast<float>(glyph.Y + CellPadding) * inverseHeight;
        entry.U1 = static_cast<float>(glyph.X + CellPadding + glyph.Width) * inverseWidth;
        entry.V1 = static_cast<float>(glyph.Y + CellPadding + metrics.tmHeight) * inverseHeight;
        entry.Width = static_cast<float>(glyph.Width);
        entry.Height = static_cast<float>(metrics.tmHeight);
        entry.Advance = static_cast<float>(glyph.Advance);
        Glyphs.emplace(glyph.Character, entry);
    }

    GdiFlush();

    WhiteU = static_cast<float>(WhiteBlockSize) * 0.5f * inverseWidth;
    WhiteV = static_cast<float>(WhiteBlockSize) * 0.5f * inverseHeight;

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = static_cast<UINT>(atlasWidth);
    textureDesc.Height = static_cast<UINT>(atlasHeight);
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initial = {};
    initial.pSysMem = bits;
    initial.SysMemPitch = static_cast<UINT>(atlasWidth * 4);

    if (FAILED(device->CreateTexture2D(&textureDesc, &initial, Texture.ReleaseAndGetAddressOf())) ||
        FAILED(device->CreateShaderResourceView(Texture.Get(), nullptr, TextureSRV.ReleaseAndGetAddressOf())))
    {
        std::cout << "FontAtlas: could not upload the atlas." << std::endl;
        Texture.Reset();
        TextureSRV.Reset();
        return false;
    }

    std::cout << "FontAtlas: " << Glyphs.size() << " glyphs at " << pixelHeight << " px in a "
              << atlasWidth << "x" << atlasHeight << " atlas." << std::endl;
    return true;
}

const FontGlyph* FontAtlas::FindGlyph(wchar_t character) const
{
    const auto found = Glyphs.find(character);
    return found == Glyphs.end() ? nullptr : &found->second;
}

float FontAtlas::MeasureWidth(const std::wstring& text) const
{
    float width = 0.0f;
    for (wchar_t character : text)
    {
        if (const FontGlyph* glyph = FindGlyph(character))
        {
            width += glyph->Advance;
        }
    }
    return width;
}
