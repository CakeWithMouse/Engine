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
class QDragEnterEvent;
class QDragMoveEvent;
class UICanvas;
class EditorQtHost;
class CollapsibleCard;
class InfoButton;

class UIEditorWindow : public QMainWindow {
public:
    explicit UIEditorWindow(EditorQtHost* host, const QString& file, QWidget* parent = nullptr);
    ~UIEditorWindow() override;
    bool Save();
    bool Reload();
    bool ConfirmClose();
    void AddElement(const QString& type);
    void AddElementAt(const QString& type, const QPointF& pos);
    // Добавление из палитры в конкретный контейнер дерева (drop в иерархию).
    void AddElementTo(const QString& type, UIElement* parent);
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
    void BuildInspector(QWidget* props, class QScrollArea* scroll);
    void RefreshInspector();
    bool ApplyInspector();
    void InsertNewElement(const QString& type, float x, float y, UIElement* parent);
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
    QAction* snapAction_ = nullptr;
    QLabel* hierarchyHeader_ = nullptr;
    QLabel* paletteHeader_ = nullptr;
    QLabel* propsHeader_ = nullptr;
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
    CollapsibleCard* cardGeneral_ = nullptr;
    CollapsibleCard* cardLayout_ = nullptr;
    CollapsibleCard* cardAppearance_ = nullptr;
    CollapsibleCard* cardBehavior_ = nullptr;
    InfoButton* helpBtn_ = nullptr;
    int langSubId_ = 0;
};

// Hierarchy tree with internal drag: dropping on a container reparents, dropping above or below
// a sibling reorders. The drop mutates the engine tree through MoveElement, then rebuilds.
// Palette drops (UiWidgetDrag mime) add a new widget into the container under the cursor.
class UIHierarchyTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    UIEditorWindow* editor = nullptr;
protected:
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};
