#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace EditorCore
{
namespace UiReparent
{

struct DefaultIsContainer
{
    template <typename T>
    constexpr bool operator()(const T*) const noexcept { return true; }
};

template <typename TElement>
inline size_t IndexOfChild(const TElement* child)
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

template <typename TElement, typename IsContainerFn = DefaultIsContainer>
inline bool CanReparent(const TElement* element, const TElement* newParent, const TElement* root, IsContainerFn isContainer = {})
{
    if (!element || !newParent || !root) return false;
    if (element == root || element == newParent) return false;
    if (!element->GetParent()) return false;
    if (!isContainer(newParent)) return false;
    for (const auto* p = newParent; p; p = p->GetParent())
    {
        if (p == element) return false;
    }
    return true;
}

template <typename TElement, typename IsContainerFn = DefaultIsContainer>
inline bool MoveElement(TElement* element, TElement* newParent, size_t index, TElement* root, IsContainerFn isContainer = {})
{
    if (!CanReparent(element, newParent, root, isContainer)) return false;
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

template <typename TElement>
inline size_t CountElements(const TElement* root)
{
    if (!root) return 0;
    size_t count = 1;
    for (const auto& child : root->GetChildren())
    {
        count += CountElements(child.get());
    }
    return count;
}

template <typename TElement>
inline bool ValidateTree(const TElement* root)
{
    if (!root) return false;
    std::vector<const TElement*> visited;
    auto dfs = [&](auto self, const TElement* node) -> bool {
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

template <typename TElement, typename IsContainerFn = DefaultIsContainer>
inline void ResolveDropTarget(const TElement* target, DropPosition pos, TElement* root,
                              TElement*& outParent, size_t& outIndex, IsContainerFn isContainer = {})
{
    if (!target || pos == OnViewport || target == root)
    {
        outParent = root;
        outIndex = (pos == AboveItem) ? 0 : (root ? root->GetChildren().size() : 0);
    }
    else if (pos == OnItem && isContainer(target))
    {
        outParent = const_cast<TElement*>(target);
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
