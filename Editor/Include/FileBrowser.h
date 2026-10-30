#pragma once

#include <QDockWidget>

class QComboBox;
class QFileSystemModel;
class QLineEdit;
class QTreeView;
class QToolButton;
class QAction;
class ProjectFilesFilter;
class EditorQtHost;

class FileBrowserDock : public QDockWidget
{
public:
    FileBrowserDock(EditorQtHost* host, QWidget* parent = nullptr);
    ~FileBrowserDock() override;

    void SetRootPath(const QString& path);
    void RetranslateUi();

private:
    void BuildUi();
    void OnDoubleClicked(const QModelIndex& index);
    void OnContextMenu(const QPoint& pos);
    void ApplyFilter(const QString& text);

    EditorQtHost* host_ = nullptr;
    QFileSystemModel* model_ = nullptr;
    ProjectFilesFilter* proxy_ = nullptr;
    QTreeView* view_ = nullptr;
    QLineEdit* filterEdit_ = nullptr;
    QComboBox* rootCombo_ = nullptr;
    QToolButton* imports_ = nullptr;
    QAction* actImportModel_ = nullptr;
    QAction* actImportTexture_ = nullptr;
    int langSubId_ = 0;
    QString projectDir_;
    QString contentDir_;
};
