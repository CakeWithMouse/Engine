// min and max arrive as macros from windows.h and would eat std::min / std::max below.
#define NOMINMAX
#include <Engine/UI/UIElement.h>

#include <algorithm>
#include <windows.h>

namespace
{
    /** UTF-8 to the wide string the glyph lookup uses. Empty on invalid input, never garbage. */
    std::wstring FromUtf8(const std::string& utf8)
    {
        if (utf8.empty())
        {
            return {};
        }
        const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        if (needed <= 0)
        {
            return {};
        }
        std::wstring wide(static_cast<size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &wide[0], needed);
        return wide;
    }

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

    /** Draws a line of glyphs from the pen position. */
    void DrawText(UIDrawList& drawList, const std::wstring& text, float x, float y, const glm::vec4& color)
    {
        if (text.empty() || drawList.Font == nullptr)
        {
            return;
        }
        float pen = x;
        for (wchar_t character : text)
        {
            const FontGlyph* glyph = drawList.Font->FindGlyph(character);
            if (glyph == nullptr)
            {
                continue;
            }
            drawList.AddQuad(pen, y, glyph->Width, glyph->Height,
                             glyph->U0, glyph->V0, glyph->U1, glyph->V1, color);
            pen += glyph->Advance;
        }
    }

    /** The four edges of a rectangle, drawn inside it so a border never grows the element. */
    void DrawBorder(UIDrawList& drawList, const UIBox& box, float width, const glm::vec4& color)
    {
        if (width <= 0.0f)
        {
            return;
        }
        const float border = std::min(width, std::min(box.Width, box.Height) * 0.5f);
        drawList.AddRect(box.X, box.Y, box.Width, border, color);
        drawList.AddRect(box.X, box.Y + box.Height - border, box.Width, border, color);
        drawList.AddRect(box.X, box.Y + border, border, box.Height - border * 2.0f, color);
        drawList.AddRect(box.X + box.Width - border, box.Y + border, border, box.Height - border * 2.0f, color);
    }
}

UIBox UIBox::Deflate(const UIEdges& edges) const
{
    UIBox result;
    result.X = X + edges.Left;
    result.Y = Y + edges.Top;
    result.Width = std::max(0.0f, Width - edges.Left - edges.Right);
    result.Height = std::max(0.0f, Height - edges.Top - edges.Bottom);
    return result;
}

UIBox ResolveUIRect(const UIRect& rect, const UIBox& parent)
{
    float originX = 0.0f;
    float originY = 0.0f;
    float directionX = 1.0f;
    float directionY = 1.0f;

    switch (rect.Anchor)
    {
    case UIAnchor::TopLeft:      originX = 0.0f;                 originY = 0.0f;                   break;
    case UIAnchor::TopCenter:    originX = parent.Width * 0.5f;   originY = 0.0f;                   break;
    case UIAnchor::TopRight:     originX = parent.Width;          originY = 0.0f;                   directionX = -1.0f; break;
    case UIAnchor::CenterLeft:   originX = 0.0f;                 originY = parent.Height * 0.5f;    break;
    case UIAnchor::Center:       originX = parent.Width * 0.5f;   originY = parent.Height * 0.5f;   break;
    case UIAnchor::CenterRight:  originX = parent.Width;          originY = parent.Height * 0.5f;   directionX = -1.0f; break;
    case UIAnchor::BottomLeft:   originX = 0.0f;                 originY = parent.Height;          directionY = -1.0f; break;
    case UIAnchor::BottomCenter: originX = parent.Width * 0.5f;   originY = parent.Height;          directionY = -1.0f; break;
    case UIAnchor::BottomRight:  originX = parent.Width;          originY = parent.Height;          directionX = -1.0f; directionY = -1.0f; break;
    }

    // A size of zero means "to the opposite edge of the parent": it is what lets a label
    // right-align against a panel, or a row fill it, without repeating the parent's width.
    UIBox box;
    box.Width = rect.Width > 0.0f ? rect.Width
                                  : std::max(0.0f, (directionX > 0.0f ? parent.Width - originX - rect.X
                                                                      : originX - rect.X));
    box.Height = rect.Height > 0.0f ? rect.Height
                                    : std::max(0.0f, (directionY > 0.0f ? parent.Height - originY - rect.Y
                                                                        : originY - rect.Y));

    // An anchor on the right or the bottom measures inwards, and the rectangle grows the same
    // way, so its far edge is the one that stays put.
    box.X = directionX > 0.0f ? originX + rect.X : originX - rect.X - box.Width;
    box.Y = directionY > 0.0f ? originY + rect.Y : originY - rect.Y - box.Height;

    if (rect.Anchor == UIAnchor::TopCenter || rect.Anchor == UIAnchor::Center ||
        rect.Anchor == UIAnchor::BottomCenter)
    {
        box.X = originX + rect.X - box.Width * 0.5f;
    }
    if (rect.Anchor == UIAnchor::CenterLeft || rect.Anchor == UIAnchor::Center ||
        rect.Anchor == UIAnchor::CenterRight)
    {
        box.Y = originY + rect.Y - box.Height * 0.5f;
    }

    box.X += parent.X;
    box.Y += parent.Y;
    return box;
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

UIElement* UIElement::Find(const std::string& name)
{
    if (!name.empty() && Name == name)
    {
        return this;
    }
    for (const std::unique_ptr<UIElement>& child : Children)
    {
        if (UIElement* found = child->Find(name))
        {
            return found;
        }
    }
    return nullptr;
}

void UIElement::Layout(const UIBox& parentContent)
{
    Box = ResolveUIRect(Rect, parentContent);
    const UIBox content = GetContentBox();
    for (const std::unique_ptr<UIElement>& child : Children)
    {
        child->Layout(content);
    }
}

void UIElement::Build(UIDrawList& drawList) const
{
    if (!bVisible)
    {
        return;
    }
    BuildSelf(drawList);
    // Tree order is draw order, so a child always lands on top of its parent.
    for (const std::unique_ptr<UIElement>& child : Children)
    {
        child->Build(drawList);
    }
}

UIElement* UIElement::HitTest(float pixelX, float pixelY)
{
    if (!bVisible)
    {
        return nullptr;
    }
    // Backwards: the element drawn last is the one on top, and it takes the click.
    for (auto child = Children.rbegin(); child != Children.rend(); ++child)
    {
        if (UIElement* hit = (*child)->HitTest(pixelX, pixelY))
        {
            return hit;
        }
    }
    return bInteractive && Box.Contains(pixelX, pixelY) ? this : nullptr;
}

void UIInputRouter::ClearFlags(UIElement& element)
{
    element.SetInputFlags(false, false);
    for (const std::unique_ptr<UIElement>& child : element.GetChildren())
    {
        ClearFlags(*child);
    }
}

void UIInputRouter::Reset()
{
    Hovered = nullptr;
    Captured = nullptr;
    bWasMouseDown = false;
}

UIElement* UIInputRouter::Update(UIElement& root, const UIInputState& input)
{
    ClearFlags(root);

    UIElement* hit = root.HitTest(input.MouseX, input.MouseY);
    Hovered = hit;

    const bool bPressedNow = input.bMouseDown && !bWasMouseDown;
    const bool bReleasedNow = !input.bMouseDown && bWasMouseDown;
    bWasMouseDown = input.bMouseDown;

    if (bPressedNow)
    {
        Captured = hit;
    }

    if (Captured != nullptr)
    {
        // The capture survives the cursor leaving the element, so the press is only shown while
        // the cursor is still on it: that is the feedback that tells the player a release there
        // will not count.
        Captured->SetInputFlags(Captured == hit, Captured == hit);
    }
    if (hit != nullptr && Captured != hit)
    {
        hit->SetInputFlags(true, false);
    }

    if (bReleasedNow)
    {
        UIElement* clicked = Captured;
        Captured = nullptr;
        if (clicked != nullptr && clicked == hit && clicked->OnClick)
        {
            clicked->OnClick();
        }
    }

    return hit;
}

void UIPanel::BuildSelf(UIDrawList& drawList) const
{
    drawList.AddRect(Box.X, Box.Y, Box.Width, Box.Height, Color);
    DrawBorder(drawList, Box, BorderWidth, BorderColor);
}

void UILabel::SetText(const std::string& utf8)
{
    Text = FromUtf8(utf8);
}

std::string UILabel::GetText() const
{
    return ToUtf8(Text);
}

void UILabel::BuildSelf(UIDrawList& drawList) const
{
    if (Text.empty() || drawList.Font == nullptr)
    {
        return;
    }
    const float x = bAlignRight ? Box.X + Box.Width - drawList.Font->MeasureWidth(Text) : Box.X;
    DrawText(drawList, Text, x, Box.Y, Color);
}

void UIBar::BuildSelf(UIDrawList& drawList) const
{
    drawList.AddRect(Box.X, Box.Y, Box.Width, Box.Height, TrackColor);
    const float filled = Box.Width * std::max(0.0f, std::min(Fill, 1.0f));
    drawList.AddRect(Box.X, Box.Y, filled, Box.Height, FillColor);
}

UIButton::UIButton()
{
    bInteractive = true;
}

void UIButton::SetText(const std::string& utf8)
{
    Text = FromUtf8(utf8);
}

std::string UIButton::GetText() const
{
    return ToUtf8(Text);
}

void UIButton::BuildSelf(UIDrawList& drawList) const
{
    const glm::vec4& background = IsPressed() ? PressedColor : (IsHovered() ? HoverColor : Color);
    drawList.AddRect(Box.X, Box.Y, Box.Width, Box.Height, background);
    DrawBorder(drawList, Box, BorderWidth, BorderColor);

    if (Text.empty() || drawList.Font == nullptr)
    {
        return;
    }
    const float textWidth = drawList.Font->MeasureWidth(Text);
    const float textHeight = drawList.Font->GetLineHeight();
    DrawText(drawList, Text,
             Box.X + (Box.Width - textWidth) * 0.5f,
             Box.Y + (Box.Height - textHeight) * 0.5f,
             TextColor);
}
