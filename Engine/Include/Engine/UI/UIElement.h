#pragma once

#include <directxmath.h>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
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

/** Inset of the area the children are laid out in, in pixels. */
struct UIEdges
{
    float Left = 0.0f;
    float Top = 0.0f;
    float Right = 0.0f;
    float Bottom = 0.0f;
};

/**
 * Position and size in pixels, measured from the anchor of the parent towards its middle.
 * A width or height of zero stretches the element to the opposite edge of the parent, which is
 * what makes a label right-align against a panel without repeating the panel's width.
 */
struct UIRect
{
    UIAnchor Anchor = UIAnchor::TopLeft;
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
};

/** An absolute rectangle in screen pixels, the result of layout. Origin is the top left. */
struct UIBox
{
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;

    bool Contains(float pixelX, float pixelY) const
    {
        return pixelX >= X && pixelY >= Y && pixelX < X + Width && pixelY < Y + Height;
    }
    /** The box shrunk by the edges; never smaller than nothing. */
    UIBox Deflate(const UIEdges& edges) const;
};

/** Resolves an anchored rectangle inside a box. Exposed so that layout can be tested on its own. */
UIBox ResolveUIRect(const UIRect& rect, const UIBox& parent);

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

    void AddQuad(float x, float y, float width, float height,
                 float u0, float v0, float u1, float v1, const glm::vec4& color);
    /** Flat colour: uses the white block of the atlas, so panels and text stay one draw call. */
    void AddRect(float x, float y, float width, float height, const glm::vec4& color);
};

/** The mouse as the interface sees it: pixels from the top left of the window. */
struct UIInputState
{
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    bool bMouseDown = false;
};

/**
 * Anything the interface can draw.
 *
 * An element owns its children, its anchored rectangle and its style; it knows nothing about the
 * device, the window or the input source. Everything an element needs per frame arrives through
 * Layout (where the box is resolved against the parent) and Build (where vertices are written),
 * which keeps drawing a pure function of the tree and makes layout and hit-testing testable
 * without a device.
 *
 * Drawing order is the order of the tree, so a child is always drawn over its parent. Hit-testing
 * walks it backwards, so the element drawn last is the one that receives the click.
 */
class UIElement
{
public:
    virtual ~UIElement() = default;
    UIElement() = default;
    UIElement(const UIElement&) = delete;
    UIElement& operator=(const UIElement&) = delete;

    /** Looked up from markup and from game code; empty for decoration that nobody addresses. */
    std::string Name;
    UIRect Rect;
    UIEdges Padding;
    bool bVisible = true;
    /**
     * Takes part in hit-testing. Off by default: a panel that only draws a background must not
     * swallow the clicks meant for what is drawn on top of it.
     */
    bool bInteractive = false;
    /** Called when a press and the release that follows it both land on this element. */
    std::function<void()> OnClick;

    /** Takes the child and returns it; the pointer stays valid until the tree is rebuilt. */
    template <typename T>
    T* Add(std::unique_ptr<T> child)
    {
        T* raw = child.get();
        child->Parent = this;
        Children.push_back(std::move(child));
        return raw;
    }
    const std::vector<std::unique_ptr<UIElement>>& GetChildren() const { return Children; }
    UIElement* GetParent() const { return Parent; }
    void ClearChildren() { Children.clear(); }
    bool RemoveChild(UIElement* child) {
        for (auto it = Children.begin(); it != Children.end(); ++it)
            if (it->get() == child) { Children.erase(it); return true; }
        return false;
    }

    /** Depth-first search by name, this element included. Returns nullptr when there is no match. */
    UIElement* Find(const std::string& name);
    /**
     * The same search, but only matches an element of the requested type. Game code binds to the
     * markup through this, so a renamed or retyped element is a null pointer and not a bad cast.
     */
    template <typename T>
    T* FindAs(const std::string& name)
    {
        return dynamic_cast<T*>(Find(name));
    }

    /** Resolves this element and everything below it inside the parent's content box. */
    void Layout(const UIBox& parentContent);
    const UIBox& GetBox() const { return Box; }
    /** The part of the box the children are laid out in. */
    UIBox GetContentBox() const { return Box.Deflate(Padding); }

    /** Writes this element and then its children. Layout must have run for the current size. */
    void Build(UIDrawList& drawList) const;

    /**
     * Topmost interactive element under the cursor, or nullptr. Invisible subtrees are skipped
     * whole: what cannot be seen cannot be clicked.
     */
    UIElement* HitTest(float pixelX, float pixelY);

    bool IsHovered() const { return bHovered; }
    bool IsPressed() const { return bPressed; }
    /** Set by UIInputRouter once per frame; elements never decide this for themselves. */
    void SetInputFlags(bool hovered, bool pressed) { bHovered = hovered; bPressed = pressed; }

protected:
    /** Draws this element alone. The resolved box is already in Box. */
    virtual void BuildSelf(UIDrawList& drawList) const {}

    UIBox Box;

private:
    std::vector<std::unique_ptr<UIElement>> Children;
    UIElement* Parent = nullptr;
    bool bHovered = false;
    bool bPressed = false;
};

/**
 * Turns the mouse into hover, press and click on a tree.
 *
 * The press is captured: a click counts only when the release lands on the element the press
 * started on, which is what makes dragging off a button cancel it instead of firing it. The
 * router holds that capture between frames, so elements stay free of input state they would have
 * to reset themselves.
 */
class UIInputRouter
{
public:
    /** Updates the flags of the whole tree and fires OnClick. Returns the element under the cursor. */
    UIElement* Update(UIElement& root, const UIInputState& input);
    /** Forgets hover and capture, e.g. when the window loses focus or the tree is rebuilt. */
    void Reset();
    UIElement* GetHovered() const { return Hovered; }
    bool HasCapture() const { return Captured != nullptr; }

private:
    static void ClearFlags(UIElement& element);

    UIElement* Hovered = nullptr;
    UIElement* Captured = nullptr;
    bool bWasMouseDown = false;
};

/** A filled rectangle: the background of a panel, a separator, a frame. */
class UIPanel : public UIElement
{
public:
    glm::vec4 Color{0.0f, 0.0f, 0.0f, 0.55f};
    /** Drawn inside the edges of the rectangle when the width is above zero. */
    float BorderWidth = 0.0f;
    glm::vec4 BorderColor{1.0f, 1.0f, 1.0f, 0.25f};

protected:
    void BuildSelf(UIDrawList& drawList) const override;
};

/** A line of text. The box gives the top left corner; the height comes from the font. */
class UILabel : public UIElement
{
public:
    std::wstring Text;
    glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
    /** Shifts the line so it ends at the right edge of the box instead of starting at the left. */
    bool bAlignRight = false;

    void SetText(const std::string& utf8);
    std::string GetText() const;

protected:
    void BuildSelf(UIDrawList& drawList) const override;
};

/** A value between zero and one, drawn as a filled track. */
class UIBar : public UIElement
{
public:
    float Fill = 0.0f;
    glm::vec4 TrackColor{1.0f, 1.0f, 1.0f, 0.15f};
    glm::vec4 FillColor{0.88f, 0.52f, 0.24f, 0.9f};

protected:
    void BuildSelf(UIDrawList& drawList) const override;
};

/**
 * A clickable rectangle with a caption. Interactive by default - a button that does not react to
 * the mouse is a panel - and its colour follows the state the router put it in.
 */
class UIButton : public UIElement
{
public:
    UIButton();

    std::wstring Text;
    glm::vec4 Color{0.16f, 0.18f, 0.22f, 0.85f};
    glm::vec4 HoverColor{0.24f, 0.28f, 0.34f, 0.9f};
    glm::vec4 PressedColor{0.10f, 0.11f, 0.14f, 0.95f};
    glm::vec4 TextColor{0.92f, 0.94f, 0.96f, 1.0f};
    float BorderWidth = 1.0f;
    glm::vec4 BorderColor{1.0f, 1.0f, 1.0f, 0.22f};

    void SetText(const std::string& utf8);
    std::string GetText() const;

protected:
    void BuildSelf(UIDrawList& drawList) const override;
};
