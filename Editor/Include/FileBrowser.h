#pragma once

#include <QDockWidget>

class QComboBox;
class QFileSystemModel;
class QLineEdit;
class QTreeView;
class EditorQtHost;

// Файл-менеджер проекта: дерево на QFileSystemModel с корнем в каталоге
// проекта (или Content). Живёт целиком в UI-потоке; в движок отдаёт только
// команды через EditorQtHost::PostCommand.
class FileBrowserDock : public QDockWidget
{
public:
    FileBrowserDock(EditorQtHost* host, QWidget* parent = nullptr);

    void SetRootPath(const QString& path);

private:
    void BuildUi();
    void OnDoubleClicked(const QModelIndex& index);
    void OnContextMenu(const QPoint& pos);
    void ApplyFilter(const QString& text);

    EditorQtHost* host_ = nullptr;
    QFileSystemModel* model_ = nullptr;
    QTreeView* view_ = nullptr;
    QLineEdit* filterEdit_ = nullptr;
    QComboBox* rootCombo_ = nullptr;
    QString projectDir_;
    QString contentDir_;
};