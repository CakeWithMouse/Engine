#pragma once

#include <QDockWidget>
#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QDir>
#include <QPersistentModelIndex>
#include <QPointer>
#include <functional>
#include "IconProvider.h"
#include "EditorTheme.h"
#include "ProjectFileTypeStrings.h"

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
    explicit ProjectFilesFilter(QObject* parent = nullptr) : QSortFilterProxyModel(parent)
    {
        RefreshIcons();
        themeSubscription_ = EditorTheme::Instance().SubscribeThemeChanged([this] { RefreshIcons(); });
        languageSubscription_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
            emit headerDataChanged(Qt::Horizontal, 2, 2);
            if (sourceModel() && rowCount() > 0) emit dataChanged(index(0, 2), index(rowCount() - 1, 2), {Qt::DisplayRole});
        });
    }
    ~ProjectFilesFilter() override
    {
        EditorTheme::Instance().UnsubscribeThemeChanged(themeSubscription_);
        EditorLocale::Instance().UnsubscribeLanguageChanged(languageSubscription_);
    }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (index.isValid() && index.column() == 0 && role == Qt::DecorationRole) {
            const auto* files = static_cast<QFileSystemModel*>(sourceModel());
            return files->isDir(mapToSource(index)) ? folderIcon_ : fileIcon_;
        }
        if (index.isValid() && index.column() == 2 && role == Qt::DisplayRole) {
            const auto* files = static_cast<QFileSystemModel*>(sourceModel());
            const auto source = mapToSource(index).siblingAtColumn(0);
            return ProjectFileTypeStrings::Type(files->fileName(source), files->isDir(source));
        }
        return QSortFilterProxyModel::data(index, role);
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (section == 2 && orientation == Qt::Horizontal && role == Qt::DisplayRole) return ProjectFileTypeStrings::Header();
        return QSortFilterProxyModel::headerData(section, orientation, role);
    }
    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        auto result = QSortFilterProxyModel::flags(index) & ~Qt::ItemIsEditable;
        if (index.isValid() && index == folderRenameIndex_) result |= Qt::ItemIsEditable;
        return result;
    }
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override
    {
        if (role != Qt::EditRole) return QSortFilterProxyModel::setData(index, value, role);
        if (index != folderRenameIndex_ || !renameFolder_) return false;
        const auto rename = renameFolder_;
        return rename(value.toString());
    }
    void BeginFolderRename(const QModelIndex& index, std::function<bool(const QString&)> rename)
    {
        folderRenameIndex_ = index;
        renameFolder_ = std::move(rename);
    }
    void EndFolderRename() { folderRenameIndex_ = QPersistentModelIndex(); renameFolder_ = {}; }
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

private:
    void RefreshIcons()
    {
        folderIcon_ = IconProvider::FolderIcon();
        fileIcon_ = IconProvider::FileIcon();
        if (sourceModel() && rowCount() > 0) emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::DecorationRole});
    }
    QPersistentModelIndex folderRenameIndex_;
    std::function<bool(const QString&)> renameFolder_;
    QIcon folderIcon_, fileIcon_;
    int themeSubscription_ = 0;
    int languageSubscription_ = 0;
};

class FileBrowserDock : public QDockWidget
{
public:
    FileBrowserDock(EditorQtHost* host, QWidget* parent = nullptr);
    ~FileBrowserDock() override;

    void SetRootPath(const QString& path);
    void RetranslateUi();
    void BeginInlineRename(const QString& absolutePath);

    QSize minimumSizeHint() const override { return QSize(60, 40); }
    QSize sizeHint() const override { return QSize(250, 180); }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void BuildUi();
    void OnDoubleClicked(const QModelIndex& index);
    void OnContextMenu(const QPoint& pos);
    void ApplyFilter(const QString& text);
    void DeleteCurrentAsset();
    bool IsProtectedPath(const QString& path) const;
    void TryBeginInlineRename();
    void EndInlineRename();

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
    QString pendingRenamePath_;
    QPersistentModelIndex renameIndex_;
    QPointer<QLineEdit> renameEditor_;
    int langSubId_ = 0;
    int themeSubId_ = 0;
    QString projectDir_;
    QString contentDir_;
};
