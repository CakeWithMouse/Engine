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
        if (EditorCore::AssetDrag::HasAssetMime(*event->mimeData()) || event->mimeData()->hasUrls())
        {
            event->acceptProposedAction();
        }
        else
        {
            // Base implementation only updates the drop indicator; rows are never
            // moved by Qt itself (see dropEvent).
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

        // Internal hierarchy drag: the scene is the single source of truth.
        // Never call QTreeWidget::dropEvent here — its internal move copies/removes
        // rows in the widget model and duplicates objects once the tree is rebuilt
        // from the scene. Only emit the reparent request; the tree refreshes from
        // the scene snapshot after the command applies. IgnoreAction also stops
        // QAbstractItemView::startDrag from deleting the source rows.
        QTreeWidgetItem* draggedItem = currentItem();
        if (!draggedItem)
        {
            event->ignore();
            return;
        }
        const uint64_t childId = draggedItem->data(0, Qt::UserRole).toULongLong();

        uint64_t parentId = 0;
        QTreeWidgetItem* target = itemAt(event->position().toPoint());
        if (target)
        {
            if (dropIndicatorPosition() == QAbstractItemView::OnItem)
            {
                parentId = target->data(0, Qt::UserRole).toULongLong();
            }
            else if (target->parent())
            {
                parentId = target->parent()->data(0, Qt::UserRole).toULongLong();
            }
        }

        if (childId && childId != parentId && onParentChanged)
        {
            onParentChanged(childId, parentId);
        }
        event->setDropAction(Qt::IgnoreAction);
        event->accept();
    }

private:
    QString contentRoot_;
};

