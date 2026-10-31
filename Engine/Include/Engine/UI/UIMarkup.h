#pragma once

#include <Engine/UI/UIElement.h>

#include <memory>
#include <string>
#include <vector>

/**
 * Contents of a .ui file (XML, UTF-8):
 *
 *     <UI>
 *       <Panel name="frame" anchor="TopLeft" rect="16 16 296 166" color="#101418A0"
 *              border="1" borderColor="#FFFFFF38" padding="16">
 *         <Label name="title" text="FRAME" color="#E0B070FF"/>
 *         <Label name="value" rect="0 26" text="0.0 ms"/>
 *         <Bar   name="load"  rect="0 48 264 10" fill="0.4"/>
 *         <Button name="toggle" rect="0 64 120 26" text="Hide"/>
 *       </Panel>
 *     </UI>
 *
 * Tags: UI and Group (a transparent container), Panel, Label, Bar, Button. Nesting is layout:
 * a child is positioned inside the content box of its parent (the parent's box minus its
 * padding) and is drawn over it.
 *
 * Common attributes: name, anchor, rect, x, y, width, height, padding, visible, interactive.
 *   anchor:  TopLeft TopCenter TopRight CenterLeft Center CenterRight
 *            BottomLeft BottomCenter BottomRight (default TopLeft)
 *   rect:    "x y width height", or "x y" to leave the size open
 *   padding: "all", or "left top right bottom"
 * A width or height left out, or zero, stretches the element to the opposite edge of its parent.
 * Sizes and offsets are in pixels and are measured from the anchor inwards.
 *
 * Per tag: Panel takes color, border, borderColor; Label takes text, color, align (left|right);
 * Bar takes fill, trackColor, fillColor; Button takes text, color, hoverColor, pressedColor,
 * textColor, border, borderColor.
 *
 * Colours are #RRGGBB or #RRGGBBAA. Booleans are true/false. Game code reaches an element by its
 * name, which is also how a button gets its OnClick: the markup says what the interface looks
 * like, never what it does.
 *
 * A file that cannot be parsed is reported with its line and changes nothing, so a typo during
 * hot reload leaves the interface that was already on screen.
 */
class UIMarkup
{
public:
    /** Writes all supported properties, including those not exposed by the visual inspector. */
    static std::string Serialize(const UIElement& root);
    /** Parses markup text. sourceName only appears in messages. */
    static bool Parse(const std::string& text, const std::string& sourceName,
                      std::unique_ptr<UIElement>& outRoot, std::string& error,
                      std::vector<std::string>* warnings = nullptr);

    /** Reads and parses a file. On failure returns false and a message naming the file and line. */
    static bool Load(const std::string& path, std::unique_ptr<UIElement>& outRoot,
                     std::string& error, std::vector<std::string>* warnings = nullptr);

    /** Parses #RRGGBB or #RRGGBBAA. Exposed because the same notation is useful from game code. */
    static bool ParseColor(const std::string& text, glm::vec4& outColor);
    /** Parses one of the nine anchor names, case-insensitively. */
    static bool ParseAnchor(const std::string& text, UIAnchor& outAnchor);
};
