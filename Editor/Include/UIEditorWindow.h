#pragma once
#include <QMainWindow>
#include <QImage>
#include <Engine/UI/UIElement.h>
#include <memory>
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QAction;
class UICanvas;

class UIEditorWindow : public QMainWindow {
public:
    explicit UIEditorWindow(const QString& file, QWidget* parent = nullptr);
    ~UIEditorWindow() override;
    bool Save();
    bool Reload();
    bool ConfirmClose();
    void AddElement(const QString& type);
    void DeleteSelected();
    void Select(UIElement* element);
    UIElement* Root() const { return root_.get(); }
    UIElement* Selected() const { return selected_; }
    QString FilePath() const { return file_; }
    void Changed();
    void RebuildTree();
    void RetranslateUi();
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void RefreshInspector();
    bool ApplyInspector();
    QString file_;
    std::unique_ptr<UIElement> root_;
    UIElement* selected_ = nullptr;
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
    QLabel* labelName_ = nullptr;
    QLabel* labelText_ = nullptr;
    QLabel* labelRect_[4]{};
    QLabel* labelAnchor_ = nullptr;
    QLabel* labelVisible_ = nullptr;
    QLabel* labelColorProperty_ = nullptr;
    QPushButton* colorBtn_ = nullptr;
    QLabel* hintLabel_ = nullptr;
    int langSubId_ = 0;
};
