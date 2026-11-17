#pragma once

#include "AssetExtensions.h"
#include "Preview/TexturePreviewSource.h"

#include <QPixmap>
#include <QString>
#include <functional>

class QObject;

// UI adapter over the Core static preview source: turns cached/decoded QImages into
// QPixmaps for icon views and supplies themed category placeholders. The decode, LRU
// cache, aspect fit and HighDPI sizing live in EditorCore::TexturePreviewSource.
class AssetThumbnailCache
{
public:
    static AssetThumbnailCache& Instance();

    // Returns cached pixmap if present. If not, returns category placeholder icon and schedules
    // asynchronous decoding on a worker thread. When ready, onLoaded is invoked on the UI thread
    // via queued invoke. If receiver is destroyed, the task is discarded safely.
    QPixmap GetOrCreate(
        EditorCore::AssetKind kind,
        const QString& filePath,
        qint64 mtime,
        int size,
        qreal dpr,
        QObject* receiver,
        std::function<void(const QPixmap&)> onLoaded
    );

    QPixmap FindInCache(const QString& filePath, qint64 mtime, int size, qreal dpr) const;
    void Insert(const QString& filePath, qint64 mtime, int size, qreal dpr, const QPixmap& pixmap);
    void Clear();
    int Count() const;

    static QPixmap CategoryIcon(EditorCore::AssetKind kind, int size, qreal dpr);

private:
    AssetThumbnailCache() = default;
    ~AssetThumbnailCache() = default;
    AssetThumbnailCache(const AssetThumbnailCache&) = delete;
    AssetThumbnailCache& operator=(const AssetThumbnailCache&) = delete;

    EditorCore::TexturePreviewSource source_;
};