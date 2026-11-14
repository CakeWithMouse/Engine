#include "GraphHistory.h"

namespace EditorCore
{

void GraphHistory::Clear()
{
    undo_.clear();
    redo_.clear();
    lastKey_.clear();
}

void GraphHistory::Push(const QString& snapshot, const QString& mergeKey)
{
    lastKey_ = mergeKey;
    redo_.clear();
    if (undo_.isEmpty() || undo_.last() != snapshot) undo_ << snapshot;
    while (undo_.size() > Limit) undo_.removeFirst();
}

bool GraphHistory::Undo(const QString& current, QString& restored)
{
    if (undo_.isEmpty()) return false;
    redo_ << current;
    restored = undo_.takeLast();
    lastKey_.clear();
    return true;
}

bool GraphHistory::Redo(const QString& current, QString& restored)
{
    if (redo_.isEmpty()) return false;
    undo_ << current;
    restored = redo_.takeLast();
    lastKey_.clear();
    return true;
}

bool GraphHistory::DropEmptyStep(const QString& mergeKey, const QString& current)
{
    if (mergeKey.isEmpty() || lastKey_ != mergeKey || undo_.isEmpty() || undo_.last() != current) return false;
    undo_.removeLast();
    lastKey_.clear();
    return true;
}

} // namespace EditorCore
