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

// Файл-менеджер проекта: дерево на QFileSystemModel с корнем в каталоге
// проекта (или Content). Живёт целиком в UI-потоке; в движок отдаёт только
// команды через EditorQtHost::PostCommand.
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
    QToolButton* importsBtn_ = nullptr;
    QAction* importTextureAct_ = nullptr;
    QAction* importModelAct_ = nullptr;
    int langSubId_ = 0;
    QString projectDir_;
    QString contentDir_;
};
