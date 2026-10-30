#pragma once

#include <QTreeWidget>
#include <QDropEvent>
#include <functional>
#include <cstdint>

class SceneOutlinerTree : public QTreeWidget
{
public:
    explicit SceneOutlinerTree(QWidget* parent = nullptr) : QTreeWidget(parent)
    {
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDragDropMode(QAbstractItemView::InternalMove);
    }

    std::function<void(uint64_t childId, uint64_t parentId)> onParentChanged;

protected:
    void dropEvent(QDropEvent* event) override
    {
        QTreeWidgetItem* draggedItem = currentItem();
        if (!draggedItem) {
            QTreeWidget::dropEvent(event);
            return;
        }

        uint64_t childId = draggedItem->data(0, Qt::UserRole).toULongLong();

        QTreeWidget::dropEvent(event);

        uint64_t parentId = 0;
        if (draggedItem->parent()) {
            parentId = draggedItem->parent()->data(0, Qt::UserRole).toULongLong();
        }

        if (childId && onParentChanged) {
            onParentChanged(childId, parentId);
        }
    }
};
