#pragma once

#include "AssetExtensions.h"

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QPixmap>
#include <QString>
#include <functional>

class QObject;

struct ThumbnailKey
{
    QString path;
    qint64 mtime = 0;
    int size = 96;
    qreal dpr = 1.0;

    bool operator==(const ThumbnailKey& o) const
    {
        return size == o.size && qFuzzyCompare(dpr, o.dpr) && mtime == o.mtime && path == o.path;
    }
};

inline size_t qHash(const ThumbnailKey& k, size_t seed = 0)
{
    return qHash(k.path, seed) ^ qHash(k.mtime) ^ qHash(k.size) ^ qHash(static_cast<int>(k.dpr * 100));
}

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
    AssetThumbnailCache();
    ~AssetThumbnailCache() = default;
    AssetThumbnailCache(const AssetThumbnailCache&) = delete;
    AssetThumbnailCache& operator=(const AssetThumbnailCache&) = delete;

    mutable QMutex mutex_;
    QCache<ThumbnailKey, QPixmap> cache_;
};
