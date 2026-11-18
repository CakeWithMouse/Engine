#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace EditorCore
{
namespace SceneReparent
{

struct Node
{
    uint64_t id = 0;
    uint64_t parent = 0;
};

// Single source of truth for reparent validation: the child must exist, the new
// parent must exist (0 = scene root), and the move must not create a cycle
// (into itself or into its own descendant). Also rejects already-corrupt input
// (duplicate ids or pre-existing cycles) instead of looping forever.
inline bool CanReparent(const std::vector<Node>& nodes, uint64_t childId, uint64_t parentId)
{
    if (childId == 0 || childId == parentId)
        return false;

    std::unordered_map<uint64_t, uint64_t> parentOf;
    parentOf.reserve(nodes.size());
    bool childFound = false;
    for (const Node& n : nodes)
    {
        if (n.id == 0 || !parentOf.emplace(n.id, n.parent).second)
            return false;
        if (n.id == childId)
            childFound = true;
    }
    if (!childFound || (parentId != 0 && !parentOf.count(parentId)))
        return false;

    uint64_t a = parentId;
    size_t steps = 0;
    while (a != 0)
    {
        if (a == childId || ++steps > nodes.size())
            return false;
        const auto it = parentOf.find(a);
        if (it == parentOf.end())
            return false;
        a = it->second;
    }
    return true;
}

inline bool HasCycle(const std::vector<Node>& nodes)
{
    std::unordered_map<uint64_t, uint64_t> parentOf;
    parentOf.reserve(nodes.size());
    for (const Node& n : nodes)
    {
        if (n.id == 0 || !parentOf.emplace(n.id, n.parent).second)
            return true;
    }
    for (const Node& n : nodes)
    {
        uint64_t a = n.id;
        size_t steps = 0;
        while (a != 0)
        {
            if (++steps > nodes.size())
                return true;
            const auto it = parentOf.find(a);
            if (it == parentOf.end())
                return true;
            a = it->second;
        }
    }
    return false;
}

} // namespace SceneReparent
} // namespace EditorCore