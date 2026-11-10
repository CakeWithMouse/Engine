#pragma once

#include <QDockWidget>
#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QDir>

class QComboBox;
class QLineEdit;
class QTreeView;
class QToolButton;
class QAction;
class EditorQtHost;

class ProjectFilesFilter final : public QSortFilterProxyModel
{
public:
    QString project, content;
    bool full = false;
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void ShowFull(bool value) { full = value; invalidateFilter(); }
    Qt::DropActions supportedDragActions() const override { return Qt::CopyAction; }

    static bool IsHiddenServiceFile(const QString& name)
    {
        return name.endsWith(QLatin1String(".import.json"), Qt::CaseInsensitive) ||
               name.endsWith(QLatin1String(".importsettings.json"), Qt::CaseInsensitive) ||
               name.endsWith(QLatin1String(".source.json"), Qt::CaseInsensitive);
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        const auto* files = static_cast<QFileSystemModel*>(sourceModel());
        if (!files)
        {
            return true;
        }
        const auto index = files->index(row, 0, parent);
        if (!files->isDir(index))
        {
            if (IsHiddenServiceFile(files->fileName(index)))
            {
                return false;
            }
        }
        if (!full && QDir::cleanPath(files->filePath(parent)) == project)
        {
            return files->isDir(index) && (files->fileName(index) == QStringLiteral("Source") || QDir::cleanPath(files->filePath(index)) == content);
        }
        return true;
    }
};

class FileBrowserDock : public QDockWidget
{
public:
    FileBrowserDock(EditorQtHost* host, QWidget* parent = nullptr);
    ~FileBrowserDock() override;

    void SetRootPath(const QString& path);
    void RetranslateUi();

    QSize minimumSizeHint() const override { return QSize(60, 40); }
    QSize sizeHint() const override { return QSize(250, 180); }

private:
    void BuildUi();
    void OnDoubleClicked(const QModelIndex& index);
    void OnContextMenu(const QPoint& pos);
    void ApplyFilter(const QString& text);
    void DeleteCurrentAsset();
    bool IsProtectedPath(const QString& path) const;

    EditorQtHost* host_ = nullptr;
    QFileSystemModel* model_ = nullptr;
    ProjectFilesFilter* proxy_ = nullptr;
    QTreeView* view_ = nullptr;
    QLineEdit* filterEdit_ = nullptr;
    QComboBox* rootCombo_ = nullptr;
    QToolButton* imports_ = nullptr;
    QAction* actImportModel_ = nullptr;
    QAction* actImportTexture_ = nullptr;
    QAction* actImportAudio_ = nullptr;
    int langSubId_ = 0;
    int themeSubId_ = 0;
    QString projectDir_;
    QString contentDir_;
};
