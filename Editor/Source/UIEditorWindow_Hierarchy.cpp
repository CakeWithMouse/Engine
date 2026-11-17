#include "UIEditorWindow.h"
#include <QDropEvent>
#include <QTreeWidgetItem>

namespace {
UIElement* ElementOf(QTreeWidgetItem* item) {
    return item ? reinterpret_cast<UIElement*>(item->data(0, Qt::UserRole).value<quintptr>()) : nullptr;
}
size_t IndexOfChild(UIElement* child) {
    auto* parent = child->GetParent();
    if (!parent) return 0;
    const auto& children = parent->GetChildren();
    for (size_t i = 0; i < children.size(); ++i) if (children[i].get() == child) return i;
    return children.size();
}
bool IsContainer(UIElement* e) {
    return !dynamic_cast<UILabel*>(e) && !dynamic_cast<UIButton*>(e) && !dynamic_cast<UIBar*>(e);
}
}

void UIHierarchyTree::dropEvent(QDropEvent* event) {
    if (!editor) { event->ignore(); return; }
    auto* element = ElementOf(currentItem());
    if (!element) { event->ignore(); return; }
    auto* target = ElementOf(itemAt(event->position().toPoint()));
    const auto pos = dropIndicatorPosition();
    UIElement* newParent = nullptr; size_t index = 0;
    if (!target || pos == OnViewport) {
        newParent = editor->Root(); index = newParent ? newParent->GetChildren().size() : 0;
    } else if (pos == OnItem && IsContainer(target)) {
        newParent = target; index = target->GetChildren().size();
    } else {
        newParent = target->GetParent(); index = IndexOfChild(target) + (pos == BelowItem ? 1 : 0);
    }
    if (!newParent) { event->ignore(); return; }
    editor->MoveElement(element, newParent, index);
    event->accept();
}

void UIEditorWindow::MoveElement(UIElement* element, UIElement* newParent, size_t index) {
    if (!element || !newParent || !root_ || element == root_.get() || element == newParent) return;
    if (!IsContainer(newParent)) return;
    for (auto* p = newParent; p; p = p->GetParent()) if (p == element) return;
    auto* oldParent = element->GetParent();
    if (!oldParent) return;
    const size_t oldIndex = IndexOfChild(element);
    Checkpoint();
    auto taken = oldParent->TakeChild(element);
    if (!taken) return;
    if (newParent == oldParent && index > oldIndex) --index;
    newParent->InsertChild(index, std::move(taken));
    selected_ = element; Changed(); RebuildTree(); RefreshInspector();
}