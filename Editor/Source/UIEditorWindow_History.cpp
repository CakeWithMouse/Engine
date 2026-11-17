#include "UIEditorWindow.h"
#include <Engine/UI/UIMarkup.h>
#include <QAction>

// Snapshot history: each gesture pushes the whole markup before it mutates the tree, so one
// undo step reverses one drag, one inspector commit, one add or one delete. Snapshots are
// produced by UIMarkup::Serialize and restored by Parse, the same round trip Save relies on.
void UIEditorWindow::PushUndo(const std::string& snapshot) {
    undo_.push_back(snapshot);
    if (undo_.size() > 100) undo_.erase(undo_.begin());
    redo_.clear();
    UpdateHistoryActions();
}

void UIEditorWindow::Checkpoint() {
    if (root_) PushUndo(UIMarkup::Serialize(*root_));
}

void UIEditorWindow::RestoreSnapshot(const std::string& text) {
    std::unique_ptr<UIElement> next; std::string error;
    if (!UIMarkup::Parse(text, file_.toStdString(), next, error)) {
        if (status_) status_->setText(QString::fromStdString(error));
        return;
    }
    const std::string selectedName = selected_ ? selected_->Name : std::string();
    root_ = std::move(next);
    selected_ = root_->Find(selectedName);
    if (!selected_) selected_ = root_.get();
    RebuildTree(); RefreshInspector();
    Changed();
    UpdateHistoryActions();
}

void UIEditorWindow::Undo() {
    if (undo_.empty() || !root_) return;
    redo_.push_back(UIMarkup::Serialize(*root_));
    const auto text = undo_.back(); undo_.pop_back();
    RestoreSnapshot(text);
}

void UIEditorWindow::Redo() {
    if (redo_.empty() || !root_) return;
    undo_.push_back(UIMarkup::Serialize(*root_));
    const auto text = redo_.back(); redo_.pop_back();
    RestoreSnapshot(text);
}

void UIEditorWindow::UpdateHistoryActions() {
    if (undoAction_) undoAction_->setEnabled(!undo_.empty());
    if (redoAction_) redoAction_->setEnabled(!redo_.empty());
}