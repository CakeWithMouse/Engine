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
class UICanvas;

class UIEditorWindow : public QMainWindow {
public:
    explicit UIEditorWindow(const QString& file, QWidget* parent = nullptr);
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
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void RefreshInspector();
    void ApplyInspector();
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
};
