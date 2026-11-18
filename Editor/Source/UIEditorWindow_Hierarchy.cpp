#include "UIEditorWindow.h"
#include "UiWidgetDrag.h"
#include "DropRules.h"
#include "UiReparent.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QTreeWidgetItem>

namespace {
UIElement* ElementOf(QTreeWidgetItem* item) {
    return item ? reinterpret_cast<UIElement*>(item->data(0, Qt::UserRole).value<quintptr>()) : nullptr;
}
bool IsContainer(const UIElement* e) {
    if (!e) return false;
    return !dynamic_cast<const UILabel*>(e) &&
           !dynamic_cast<const UIButton*>(e) &&
           !dynamic_cast<const UIBar*>(e);
}
}

void UIHierarchyTree::keyPressEvent(QKeyEvent* event) {
    if (editor && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
        editor->DeleteSelected();
        event->accept();
        return;
    }
    QTreeWidget::keyPressEvent(event);
}

// InternalMove отбрасывает внешние источники до dropEvent; палитру принимаем поверх базового
// обработчика, чтобы дерево продолжало показывать индикатор и принимать свои же перемещения.
void UIHierarchyTree::dragEnterEvent(QDragEnterEvent* event) {
    QTreeWidget::dragEnterEvent(event);
    if (EditorCore::UiWidgetDrag::HasMime(*event->mimeData())) event->acceptProposedAction();
}

void UIHierarchyTree::dragMoveEvent(QDragMoveEvent* event) {
    QTreeWidget::dragMoveEvent(event);
    if (EditorCore::UiWidgetDrag::HasMime(*event->mimeData())) event->acceptProposedAction();
}

void UIHierarchyTree::dropEvent(QDropEvent* event) {
    if (!editor) { event->ignore(); return; }
    if (EditorCore::UiWidgetDrag::HasMime(*event->mimeData())) {
        const auto payload = EditorCore::UiWidgetDrag::FromMime(*event->mimeData());
        const auto decision = EditorCore::DecideDrop(EditorCore::DragSource::UiPalette, EditorCore::DropTarget::Hierarchy, EditorCore::AssetKind::Unknown);
        if (payload.type.isEmpty() || !decision.allowed) { event->ignore(); return; }
        auto* parent = ElementOf(itemAt(event->position().toPoint()));
        while (parent && !IsContainer(parent)) parent = parent->GetParent();
        if (!parent) parent = editor->Root();
        if (!parent) { event->ignore(); return; }
        editor->AddElementTo(payload.type, parent);
        event->acceptProposedAction();
        return;
    }
    auto* element = ElementOf(currentItem());
    if (!element) { event->setDropAction(Qt::IgnoreAction); event->accept(); return; }
    auto* target = ElementOf(itemAt(event->position().toPoint()));
    const auto pos = dropIndicatorPosition();
    EditorCore::UiReparent::DropPosition dropPos = EditorCore::UiReparent::OnItem;
    if (pos == AboveItem) dropPos = EditorCore::UiReparent::AboveItem;
    else if (pos == BelowItem) dropPos = EditorCore::UiReparent::BelowItem;
    else if (pos == OnViewport || !target) dropPos = EditorCore::UiReparent::OnViewport;

    UIElement* newParent = nullptr;
    size_t index = 0;
    EditorCore::UiReparent::ResolveDropTarget(target, dropPos, editor->Root(), newParent, index, IsContainer);
    if (newParent) {
        editor->MoveElement(element, newParent, index);
    }
    event->setDropAction(Qt::IgnoreAction);
    event->accept();
}

void UIEditorWindow::MoveElement(UIElement* element, UIElement* newParent, size_t index) {
    if (!EditorCore::UiReparent::CanReparent(element, newParent, root_.get(), IsContainer)) return;
    Checkpoint();
    if (!EditorCore::UiReparent::MoveElement(element, newParent, index, root_.get(), IsContainer)) return;
    selected_ = element;
    Changed();
    RebuildTree();
    RefreshInspector();
}
