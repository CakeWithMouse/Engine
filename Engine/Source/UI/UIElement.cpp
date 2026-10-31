// min and max arrive as macros from windows.h and would eat std::min / std::max below.
#define NOMINMAX
#include <Engine/UI/UIElement.h>

#include <algorithm>
#include <windows.h>

void UIDrawList::Resolve(const UIRect& rect, float& outX, float& outY) const
{
    float originX = 0.0f;
    float originY = 0.0f;
    float directionX = 1.0f;
    float directionY = 1.0f;

    switch (rect.Anchor)
    {
    case UIAnchor::TopLeft:      originX = 0.0f;              originY = 0.0f;               break;
    case UIAnchor::TopCenter:    originX = ScreenWidth * 0.5f; originY = 0.0f;              break;
    case UIAnchor::TopRight:     originX = ScreenWidth;       originY = 0.0f;               directionX = -1.0f; break;
    case UIAnchor::CenterLeft:   originX = 0.0f;              originY = ScreenHeight * 0.5f; break;
    case UIAnchor::Center:       originX = ScreenWidth * 0.5f; originY = ScreenHeight * 0.5f; break;
    case UIAnchor::CenterRight:  originX = ScreenWidth;       originY = ScreenHeight * 0.5f; directionX = -1.0f; break;
    case UIAnchor::BottomLeft:   originX = 0.0f;              originY = ScreenHeight;       directionY = -1.0f; break;
    case UIAnchor::BottomCenter: originX = ScreenWidth * 0.5f; originY = ScreenHeight;      directionY = -1.0f; break;
    case UIAnchor::BottomRight:  originX = ScreenWidth;       originY = ScreenHeight;       directionX = -1.0f; directionY = -1.0f; break;
    }

    // An anchor on the right or the bottom measures inwards, and the rectangle grows the same
    // way, so its far edge is the one that stays put.
    outX = directionX > 0.0f ? originX + rect.X : originX - rect.X - rect.Width;
    outY = directionY > 0.0f ? originY + rect.Y : originY - rect.Y - rect.Height;

    if (rect.Anchor == UIAnchor::TopCenter || rect.Anchor == UIAnchor::Center ||
        rect.Anchor == UIAnchor::BottomCenter)
    {
        outX = originX + rect.X - rect.Width * 0.5f;
    }
    if (rect.Anchor == UIAnchor::CenterLeft || rect.Anchor == UIAnchor::Center ||
        rect.Anchor == UIAnchor::CenterRight)
    {
        outY = originY + rect.Y - rect.Height * 0.5f;
    }
}

void UIDrawList::AddQuad(float x, float y, float width, float height,
                         float u0, float v0, float u1, float v1, const glm::vec4& color)
{
    if (width <= 0.0f || height <= 0.0f || color.a <= 0.0f)
    {
        return;
    }

    const DirectX::XMFLOAT4 rgba(color.r, color.g, color.b, color.a);
    const float x1 = x + width;
    const float y1 = y + height;

    UIVertex topLeft{DirectX::XMFLOAT4(x, y, u0, v0), rgba};
    UIVertex topRight{DirectX::XMFLOAT4(x1, y, u1, v0), rgba};
    UIVertex bottomLeft{DirectX::XMFLOAT4(x, y1, u0, v1), rgba};
    UIVertex bottomRight{DirectX::XMFLOAT4(x1, y1, u1, v1), rgba};

    Vertices.push_back(topLeft);
    Vertices.push_back(bottomLeft);
    Vertices.push_back(topRight);
    Vertices.push_back(topRight);
    Vertices.push_back(bottomLeft);
    Vertices.push_back(bottomRight);
}

void UIDrawList::AddRect(float x, float y, float width, float height, const glm::vec4& color)
{
    float whiteU = 0.0f;
    float whiteV = 0.0f;
    if (Font != nullptr)
    {
        Font->GetWhiteTexel(whiteU, whiteV);
    }
    AddQuad(x, y, width, height, whiteU, whiteV, whiteU, whiteV, color);
}

void UIPanel::Build(UIDrawList& drawList) const
{
    if (!bVisible)
    {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    drawList.Resolve(Rect, x, y);
    drawList.AddRect(x, y, Rect.Width, Rect.Height, Color);

    if (BorderWidth > 0.0f)
    {
        const float border = std::min(BorderWidth, std::min(Rect.Width, Rect.Height) * 0.5f);
        drawList.AddRect(x, y, Rect.Width, border, BorderColor);
        drawList.AddRect(x, y + Rect.Height - border, Rect.Width, border, BorderColor);
        drawList.AddRect(x, y + border, border, Rect.Height - border * 2.0f, BorderColor);
        drawList.AddRect(x + Rect.Width - border, y + border, border, Rect.Height - border * 2.0f, BorderColor);
    }
}

void UILabel::SetText(const std::string& utf8)
{
    Text.clear();
    if (utf8.empty())
    {
        return;
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0)
    {
        return;
    }
    Text.resize(static_cast<size_t>(needed));
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &Text[0], needed);
}

void UILabel::Build(UIDrawList& drawList) const
{
    if (!bVisible || Text.empty() || drawList.Font == nullptr)
    {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    drawList.Resolve(Rect, x, y);
    if (bAlignRight)
    {
        x += Rect.Width - drawList.Font->MeasureWidth(Text);
    }

    float pen = x;
    for (wchar_t character : Text)
    {
        const FontGlyph* glyph = drawList.Font->FindGlyph(character);
        if (glyph == nullptr)
        {
            continue;
        }
        drawList.AddQuad(pen, y, glyph->Width, glyph->Height,
                         glyph->U0, glyph->V0, glyph->U1, glyph->V1, Color);
        pen += glyph->Advance;
    }
}

void UIBar::Build(UIDrawList& drawList) const
{
    if (!bVisible)
    {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    drawList.Resolve(Rect, x, y);
    drawList.AddRect(x, y, Rect.Width, Rect.Height, TrackColor);

    const float filled = Rect.Width * std::max(0.0f, std::min(Fill, 1.0f));
    drawList.AddRect(x, y, filled, Rect.Height, FillColor);
}
