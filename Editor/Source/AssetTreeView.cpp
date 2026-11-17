#include "AssetTreeView.h"
#include "FileBrowser.h"
#include "AssetDrag.h"
#include "AssetExtensions.h"
#include "CommandRegistry.h"

#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QMimeData>
#include <QUrl>
#include <QVariantMap>

using namespace EditorCore;

AssetTreeView::AssetTreeView(QFileSystemModel* model, ProjectFilesFilter* proxy,
                             const QString& projectDir, const QString& contentDir,
                             QWidget* parent)
    : QTreeView(parent)
    , model_(model)
    , proxy_(proxy)
    , projectDir_(projectDir)
    , contentDir_(contentDir)
{
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
}

QStringList AssetTreeView::SelectedAbsolutePaths() const
{
    QStringList result;
    if (!model_ || !proxy_)
        return result;
    const QModelIndexList selected = selectedIndexes();
    for (const QModelIndex& index : selected)
    {
        if (index.column() != 0)
            continue;
        const QModelIndex source = proxy_->mapToSource(index);
        const QString path = model_->filePath(source);
        if (!path.isEmpty())
            result << QDir::cleanPath(path);
    }
    result.removeDuplicates();
    return result;
}

QString AssetTreeView::DestinationFolderAt(const QPoint& pos) const
{
    if (!model_ || !proxy_)
        return contentDir_;
    const QModelIndex proxyIndex = indexAt(pos);
    QString absolute;
    if (proxyIndex.isValid())
    {
        const QModelIndex source = proxy_->mapToSource(proxyIndex.siblingAtColumn(0));
        absolute = model_->filePath(source);
        if (!model_->isDir(source))
            absolute = QFileInfo(absolute).absolutePath();
    }
    else
    {
        absolute = contentDir_.isEmpty() ? model_->rootPath() : contentDir_;
    }
    return QDir::cleanPath(absolute);
}

bool AssetTreeView::AcceptsDrag(const QMimeData* mime) const
{
    return mime && (AssetDrag::HasAssetMime(*mime) || mime->hasUrls());
}

void AssetTreeView::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    const QStringList absolutePaths = SelectedAbsolutePaths();
    if (absolutePaths.isEmpty())
        return;

    AssetDrag::Payload payload;
    payload.source = QStringLiteral("ContentBrowser");
    payload.kind = KindForFile(absolutePaths.first());
    for (const QString& path : absolutePaths)
        payload.virtualPaths << NormalizeVirtualPath(path, contentDir_);

    auto* mime = new QMimeData;
    mime->setData(QLatin1String(AssetDrag::kMimeType), AssetDrag::Encode(payload));
    QList<QUrl> urls;
    for (const QString& path : absolutePaths)
        urls << QUrl::fromLocalFile(path);
    mime->setUrls(urls);

    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction);
}

void AssetTreeView::dragEnterEvent(QDragEnterEvent* event)
{
    if (!AcceptsDrag(event->mimeData()))
    {
        event->ignore();
        return;
    }
    if (AssetDrag::HasAssetMime(*event->mimeData()))
        event->setDropAction((event->modifiers() & Qt::ControlModifier) ? Qt::CopyAction : Qt::MoveAction);
    else
        event->setDropAction(Qt::CopyAction);
    event->accept();
}

void AssetTreeView::dragMoveEvent(QDragMoveEvent* event)
{
    if (!AcceptsDrag(event->mimeData()))
    {
        event->ignore();
        return;
    }
    if (AssetDrag::HasAssetMime(*event->mimeData()))
        event->setDropAction((event->modifiers() & Qt::ControlModifier) ? Qt::CopyAction : Qt::MoveAction);
    else
        event->setDropAction(Qt::CopyAction);
    event->accept();
}

void AssetTreeView::dropEvent(QDropEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (!AcceptsDrag(mime))
    {
        event->ignore();
        return;
    }

    if (AssetDrag::HasAssetMime(*mime))
    {
        const AssetDrag::Payload payload = AssetDrag::FromMime(*mime, contentDir_);
        QStringList absolutePaths;
        for (const QString& virtualPath : payload.virtualPaths)
        {
            absolutePaths << (virtualPath.startsWith(QStringLiteral("/Game/"))
                ? QDir::cleanPath(contentDir_ + QStringLiteral("/") + virtualPath.mid(6))
                : virtualPath);
        }
        const bool copy = event->dropAction() == Qt::CopyAction || (event->modifiers() & Qt::ControlModifier);
        QVariantMap params;
        params[QStringLiteral("paths")] = absolutePaths;
        params[QStringLiteral("destination")] = DestinationFolderAt(event->position().toPoint());
        CommandRegistry::Instance().Execute(copy ? QStringLiteral("Assets.Copy") : QStringLiteral("Assets.Move"), params);
        event->accept();
        return;
    }

    const QStringList localPaths = AssetDrag::LocalPathsFromUrls(mime->urls());
    QStringList knownPaths;
    for (const QString& path : localPaths)
    {
        if (KindForFile(path) != AssetKind::Unknown)
            knownPaths << path;
    }
    if (knownPaths.isEmpty())
    {
        event->ignore();
        return;
    }
    QVariantMap params;
    params[QStringLiteral("paths")] = knownPaths;
    params[QStringLiteral("kind")] = static_cast<int>(KindForFile(knownPaths.first()));
    CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), params);
    event->setDropAction(Qt::CopyAction);
    event->accept();
}