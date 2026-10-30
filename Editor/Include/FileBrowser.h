#pragma once

#include <QDockWidget>
#include <QStringList>
#include <QModelIndex>

class QComboBox;
class QFileSystemModel;
class QLineEdit;
class QTreeView;
class QListView;
class QToolButton;
class QPushButton;
class QAction;
class QSplitter;
class QStackedWidget;
class QLabel;
class QSlider;
class ProjectFilesFilter;
class EditorQtHost;
class AssetIconProvider;

// Современный Content Browser в стиле Unreal Engine 5 / Unity:
// Дерево папок слева, режим карточек Grid View (IconMode) + детальный список справа,
// навигация, фильтры, создание ассетов и мини-превью с цветными полосками типов.
class FileBrowserDock : public QDockWidget
{
public:
    explicit FileBrowserDock(EditorQtHost* host, QWidget* parent = nullptr);
    ~FileBrowserDock() override;

    void SetRootPath(const QString& path);
    void SetCurrentFolder(const QString& path, bool recordHistory = true);
    void RetranslateUi();

    QSize minimumSizeHint() const override { return QSize(80, 60); }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void BuildUi();
    void OnDoubleClicked(const QModelIndex& index);
    void OnContextMenu(const QPoint& pos, bool isGrid);
    void ApplyFilter(const QString& text);
    void NavigateBack();
    void NavigateUp();
    void UpdateTileSize(int size);

    void CreateNewFolder();
    void CreateNewMaterial();
    void CreateNewMaterialInstance();
    void CreateNewScene();
    void CreateNewCppClass();
    void RenameItem(const QString& path);
    void DuplicateItem(const QString& path);
    void DeleteItem(const QString& path);

    EditorQtHost* host_ = nullptr;
    QFileSystemModel* model_ = nullptr;
    ProjectFilesFilter* proxy_ = nullptr;

    QFileSystemModel* folderModel_ = nullptr;
    ProjectFilesFilter* folderProxy_ = nullptr;

    AssetIconProvider* iconProvider_ = nullptr;

    // Splitter & Views
    QSplitter* splitter_ = nullptr;
    QTreeView* folderTree_ = nullptr;
    QStackedWidget* viewStack_ = nullptr;
    QListView* gridView_ = nullptr;
    QTreeView* view_ = nullptr; // List/Tree view

    // Top Navigation & Controls
    QPushButton* btnNavBack_ = nullptr;
    QPushButton* btnNavUp_ = nullptr;
    QLabel* lblCurrentPath_ = nullptr;
    QToolButton* btnCreate_ = nullptr;
    QMenu* menuCreate_ = nullptr;
    QToolButton* importsBtn_ = nullptr;
    QAction* importTextureAct_ = nullptr;
    QAction* importModelAct_ = nullptr;
    QLineEdit* filterEdit_ = nullptr;
    QToolButton* btnViewGrid_ = nullptr;
    QToolButton* btnViewList_ = nullptr;
    QSlider* sliderTileSize_ = nullptr;
    QComboBox* rootCombo_ = nullptr;

    int langSubId_ = 0;
    QString projectDir_;
    QString contentDir_;
    QString currentFolder_;
    QStringList history_;
    int historyIndex_ = -1;
    int tileSize_ = 80;
};
