#pragma once

#include <directxmath.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <Engine/UI/FontAtlas.h>

/**
 * Corner or edge a rectangle is measured from. A HUD has to survive a resize, and anchoring is
 * what keeps a counter in the top left and a legend in the bottom right without recomputing
 * anything when the window changes.
 */
enum class UIAnchor
{
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

/** Position and size in pixels, measured from the anchor towards the middle of the screen. */
struct UIRect
{
    UIAnchor Anchor = UIAnchor::TopLeft;
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
};

/**
 * One vertex of the UI. It is packed into the engine's existing Primitive layout, so the UI needs
 * no input layout of its own: Position holds the pixel coordinate in xy and the texture
 * coordinate in zw.
 */
struct UIVertex
{
    DirectX::XMFLOAT4 Position{0.0f, 0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 Color{1.0f, 1.0f, 1.0f, 1.0f};
};

/** What the elements write into, and what the component uploads once per frame. */
struct UIDrawList
{
    std::vector<UIVertex> Vertices;
    const FontAtlas* Font = nullptr;
    float ScreenWidth = 0.0f;
    float ScreenHeight = 0.0f;

    /** Resolves an anchored rectangle into absolute pixels, origin at the top left of the screen. */
    void Resolve(const UIRect& rect, float& outX, float& outY) const;
    void AddQuad(float x, float y, float width, float height,
                 float u0, float v0, float u1, float v1, const glm::vec4& color);
    /** Flat colour: uses the white block of the atlas, so panels and text stay one draw call. */
    void AddRect(float x, float y, float width, float height, const glm::vec4& color);
};

/** Anything the UI can draw. Elements own their layout and know nothing about the device. */
class UIElement
{
public:
    virtual ~UIElement() = default;

    UIRect Rect;
    bool bVisible = true;

    virtual void Build(UIDrawList& drawList) const = 0;
};

/** A filled rectangle: the background of a panel, a separator, a frame. */
class UIPanel : public UIElement
{
public:
    glm::vec4 Color{0.0f, 0.0f, 0.0f, 0.55f};
    /** Drawn inside the edges of the rectangle when the width is above zero. */
    float BorderWidth = 0.0f;
    glm::vec4 BorderColor{1.0f, 1.0f, 1.0f, 0.25f};

    void Build(UIDrawList& drawList) const override;
};

/** A line of text. The rectangle gives the top left corner; the size comes from the font. */
class UILabel : public UIElement
{
public:
    std::wstring Text;
    glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
    /** Shifts the line so it ends at the rectangle instead of starting there. */
    bool bAlignRight = false;

    void SetText(const std::string& utf8);
    void Build(UIDrawList& drawList) const override;
};

/** A value between zero and one, drawn as a filled track. */
class UIBar : public UIElement
{
public:
    float Fill = 0.0f;
    glm::vec4 TrackColor{1.0f, 1.0f, 1.0f, 0.15f};
    glm::vec4 FillColor{0.88f, 0.52f, 0.24f, 0.9f};

    void Build(UIDrawList& drawList) const override;
};
