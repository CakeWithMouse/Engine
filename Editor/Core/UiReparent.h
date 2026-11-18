#pragma once

#include <Engine/UI/UIElement.h>
#include <cstddef>
#include <string>
#include <vector>

namespace EditorCore
{
namespace UiReparent
{

inline bool IsContainer(const UIElement* e)
{
    if (!e) return false;
    return !dynamic_cast<const UILabel*>(e) &&
           !dynamic_cast<const UIButton*>(e) &&
           !dynamic_cast<const UIBar*>(e);
}

inline size_t IndexOfChild(const UIElement* child)
{
    if (!child) return 0;
    const auto* parent = child->GetParent();
    if (!parent) return 0;
    const auto& children = parent->GetChildren();
    for (size_t i = 0; i < children.size(); ++i)
    {
        if (children[i].get() == child) return i;
    }
    return children.size();
}

inline bool CanReparent(const UIElement* element, const UIElement* newParent, const UIElement* root)
{
    if (!element || !newParent || !root) return false;
    if (element == root || element == newParent) return false;
    if (!element->GetParent()) return false;
    if (!IsContainer(newParent)) return false;
    for (const auto* p = newParent; p; p = p->GetParent())
    {
        if (p == element) return false;
    }
    return true;
}

inline bool MoveElement(UIElement* element, UIElement* newParent, size_t index, UIElement* root)
{
    if (!CanReparent(element, newParent, root)) return false;
    auto* oldParent = element->GetParent();
    const size_t oldIndex = IndexOfChild(element);
    auto taken = oldParent->TakeChild(element);
    if (!taken) return false;
    if (newParent == oldParent && index > oldIndex)
    {
        --index;
    }
    newParent->InsertChild(index, std::move(taken));
    return true;
}

inline size_t CountElements(const UIElement* root)
{
    if (!root) return 0;
    size_t count = 1;
    for (const auto& child : root->GetChildren())
    {
        count += CountElements(child.get());
    }
    return count;
}

inline bool ValidateTree(const UIElement* root)
{
    if (!root) return false;
    std::vector<const UIElement*> visited;
    auto dfs = [&](auto self, const UIElement* node) -> bool {
        for (const auto* v : visited)
        {
            if (v == node) return false;
        }
        visited.push_back(node);
        for (const auto& child : node->GetChildren())
        {
            if (!child) return false;
            if (child->GetParent() != node) return false;
            if (!self(self, child.get())) return false;
        }
        return true;
    };
    return dfs(dfs, root);
}

enum DropPosition { OnItem, AboveItem, BelowItem, OnViewport };

inline void ResolveDropTarget(const UIElement* target, DropPosition pos, UIElement* root,
                              UIElement*& outParent, size_t& outIndex)
{
    if (!target || pos == OnViewport || target == root)
    {
        outParent = root;
        outIndex = (pos == AboveItem) ? 0 : (root ? root->GetChildren().size() : 0);
    }
    else if (pos == OnItem && IsContainer(target))
    {
        outParent = const_cast<UIElement*>(target);
        outIndex = target->GetChildren().size();
    }
    else
    {
        outParent = target->GetParent() ? target->GetParent() : root;
        const size_t targetIdx = IndexOfChild(target);
        outIndex = targetIdx + (pos == AboveItem ? 0 : 1);
    }
}

} // namespace UiReparent
} // namespace EditorCore
