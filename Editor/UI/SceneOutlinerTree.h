#pragma once

#include <QTreeWidget>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <functional>
#include <cstdint>

#include "AssetDrag.h"

class SceneOutlinerTree : public QTreeWidget
{
public:
    explicit SceneOutlinerTree(QWidget* parent = nullptr) : QTreeWidget(parent)
    {
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setAlternatingRowColors(true);
        setIndentation(16);
        setRootIsDecorated(true);
    }

    std::function<void(uint64_t childId, uint64_t parentId)> onParentChanged;
    std::function<void(uint64_t targetId, EditorCore::AssetKind kind, const QString& path)> onAssetDropped;

    void SetContentRoot(const QString& root) { contentRoot_ = root; }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event->mimeData()->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist")) ||
            EditorCore::AssetDrag::HasAssetMime(*event->mimeData()) ||
            event->mimeData()->hasUrls())
        {
            event->acceptProposedAction();
        }
        else
        {
            QTreeWidget::dragEnterEvent(event);
        }
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        if (event->mimeData()->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist")) ||
            EditorCore::AssetDrag::HasAssetMime(*event->mimeData()) ||
            event->mimeData()->hasUrls())
        {
            event->acceptProposedAction();
        }
        else
        {
            QTreeWidget::dragMoveEvent(event);
        }
    }

    void dropEvent(QDropEvent* event) override
    {
        if (EditorCore::AssetDrag::HasAssetMime(*event->mimeData()) || event->mimeData()->hasUrls())
        {
            const auto payload = EditorCore::AssetDrag::FromMime(*event->mimeData(), contentRoot_);
            if (!payload.virtualPaths.isEmpty() && onAssetDropped)
            {
                QTreeWidgetItem* target = itemAt(event->position().toPoint());
                const uint64_t targetId = target ? target->data(0, Qt::UserRole).toULongLong() : 0;
                onAssetDropped(targetId, payload.kind, payload.virtualPaths.front());
                event->acceptProposedAction();
                return;
            }
        }

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

private:
    QString contentRoot_;
};

