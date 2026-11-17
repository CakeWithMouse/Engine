#pragma once

#include <QTreeView>
#include <QString>
#include <QStringList>

class QFileSystemModel;
class ProjectFilesFilter;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

// Content Browser tree: emits the shared AssetDrag MIME on drag and routes
// internal move/copy and external Explorer import drops to CommandRegistry.
class AssetTreeView final : public QTreeView
{
    Q_OBJECT
public:
    AssetTreeView(QFileSystemModel* model, ProjectFilesFilter* proxy,
                  const QString& projectDir, const QString& contentDir,
                  QWidget* parent = nullptr);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QStringList SelectedAbsolutePaths() const;
    QString DestinationFolderAt(const QPoint& pos) const;
    bool AcceptsDrag(const QMimeData* mime) const;

    QFileSystemModel* model_ = nullptr;
    ProjectFilesFilter* proxy_ = nullptr;
    QString projectDir_;
    QString contentDir_;
};