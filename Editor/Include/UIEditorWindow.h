#pragma once
#include <QMainWindow>
#include <QImage>
#include <QPointF>
#include <QTreeWidget>
#include <Engine/UI/UIElement.h>
#include <memory>
class QTreeWidgetItem;
class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QAction;
class QFormLayout;
class QDropEvent;
class UICanvas;
class EditorQtHost;

class UIEditorWindow : public QMainWindow {
public:
    explicit UIEditorWindow(EditorQtHost* host, const QString& file, QWidget* parent = nullptr);
    ~UIEditorWindow() override;
    bool Save();
    bool Reload();
    bool ConfirmClose();
    void AddElement(const QString& type);
    void AddElementAt(const QString& type, const QPointF& pos);
    void DeleteSelected();
    void Select(UIElement* element);
    UIElement* Root() const { return root_.get(); }
    UIElement* Selected() const { return selected_; }
    QString FilePath() const { return file_; }
    void Changed();
    void RebuildTree();
    void RetranslateUi();
    void Checkpoint();
    void Undo();
    void Redo();
    void MoveElement(UIElement* element, UIElement* newParent, size_t index);
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void RefreshInspector();
    bool ApplyInspector();
    void BuildPropertyEditors(QFormLayout* form, QWidget* props);
    void InsertNewElement(const QString& type, float x, float y);
    void PushUndo(const std::string& snapshot);
    void RestoreSnapshot(const std::string& text);
    void UpdateHistoryActions();
    QString ProjectRoot() const;
    QString ContentRoot() const;
    EditorQtHost* host_ = nullptr;
    QString file_;
    std::unique_ptr<UIElement> root_;
    UIElement* selected_ = nullptr;
    std::vector<std::string> undo_, redo_;
    bool refreshing_ = false, dirty_ = false;
    UICanvas* canvas_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLineEdit* name_ = nullptr;
    QLineEdit* text_ = nullptr;
    QDoubleSpinBox* rect_[4]{};
    QComboBox* anchor_ = nullptr;
    QComboBox* colorProperty_ = nullptr;
    QCheckBox* visible_ = nullptr;
    QLabel* status_ = nullptr;

    QAction* saveAction_ = nullptr;
    QAction* reloadAction_ = nullptr;
    QAction* deleteAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* gridAction_ = nullptr;
    QLabel* labelName_ = nullptr;
    QLabel* labelText_ = nullptr;
    QLabel* labelRect_[4]{};
    QLabel* labelAnchor_ = nullptr;
    QLabel* labelVisible_ = nullptr;
    QLabel* labelColorProperty_ = nullptr;
    QPushButton* colorBtn_ = nullptr;
    QLabel* hintLabel_ = nullptr;
    QDoubleSpinBox* padding_[4]{};
    QCheckBox* interactive_ = nullptr;
    QDoubleSpinBox* borderWidth_ = nullptr;
    QCheckBox* alignRight_ = nullptr;
    QDoubleSpinBox* fill_ = nullptr;
    QLabel* labelPadding_ = nullptr;
    QLabel* labelInteractive_ = nullptr;
    QLabel* labelBorderWidth_ = nullptr;
    QLabel* labelAlignRight_ = nullptr;
    QLabel* labelFill_ = nullptr;
    QLabel* sectionLayout_ = nullptr;
    QLabel* sectionAppearance_ = nullptr;
    QLabel* sectionBehavior_ = nullptr;
    int langSubId_ = 0;
};

// Hierarchy tree with internal drag: dropping on a container reparents, dropping above or below
// a sibling reorders. The drop mutates the engine tree through MoveElement, then rebuilds.
class UIHierarchyTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    UIEditorWindow* editor = nullptr;
protected:
    void dropEvent(QDropEvent* event) override;
};
