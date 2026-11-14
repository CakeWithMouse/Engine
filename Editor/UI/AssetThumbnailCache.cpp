#include "AssetThumbnailCache.h"
#include "EditorTheme.h"
#include "IconProvider.h"

#include <QImageReader>
#include <QMetaObject>
#include <QPainter>
#include <QPointer>
#include <QThreadPool>

AssetThumbnailCache& AssetThumbnailCache::Instance()
{
    static AssetThumbnailCache instance;
    return instance;
}

AssetThumbnailCache::AssetThumbnailCache()
{
    cache_.setMaxCost(256);
}

QPixmap AssetThumbnailCache::FindInCache(const QString& filePath, qint64 mtime, int size, qreal dpr) const
{
    QMutexLocker locker(&mutex_);
    const ThumbnailKey key{filePath, mtime, size, dpr};
    if (cache_.contains(key))
        return *cache_[key];
    return QPixmap();
}

void AssetThumbnailCache::Insert(const QString& filePath, qint64 mtime, int size, qreal dpr, const QPixmap& pixmap)
{
    QMutexLocker locker(&mutex_);
    const ThumbnailKey key{filePath, mtime, size, dpr};
    cache_.insert(key, new QPixmap(pixmap));
}

void AssetThumbnailCache::Clear()
{
    QMutexLocker locker(&mutex_);
    cache_.clear();
}

int AssetThumbnailCache::Count() const
{
    QMutexLocker locker(&mutex_);
    return cache_.count();
}

QPixmap AssetThumbnailCache::CategoryIcon(EditorCore::AssetKind kind, int size, qreal dpr)
{
    static std::map<std::tuple<int, int, int>, QPixmap> s_categoryCache;
    const int dprInt = qRound(dpr * 1000.0);
    const auto key = std::make_tuple(static_cast<int>(kind), size, dprInt);
    auto it = s_categoryCache.find(key);
    if (it != s_categoryCache.end())
        return it->second;

    const int physical = qMax(1, qRound(size * dpr));
    QPixmap pixmap(physical, physical);
    pixmap.fill(Qt::transparent);

    const auto& theme = EditorTheme::Instance();
    const auto& pal = theme.Palette();

    {
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Background tile with subtle border
        p.setBrush(pal.bgDarkest);
        p.setPen(QPen(pal.borderMid, qMax(1.0, 1.0 * dpr)));
        const qreal margin = 2.0 * dpr;
        p.drawRoundedRect(QRectF(margin, margin, physical - 2 * margin, physical - 2 * margin), 6.0 * dpr, 6.0 * dpr);

        // Category glyph centered
        const int glyphSize = qMax(16, qRound(physical * 0.45));
        QPixmap glyph;
        if (kind == EditorCore::AssetKind::Model)
            glyph = IconProvider::GetPixmap(QStringLiteral("cube"), pal.accent, glyphSize);
        else if (kind == EditorCore::AssetKind::Material)
            glyph = IconProvider::GetPixmap(QStringLiteral("adjustments"), pal.accent, glyphSize);
        else
            glyph = IconProvider::GetPixmap(QStringLiteral("file"), pal.textSecondary, glyphSize);

        if (!glyph.isNull())
        {
            const int gx = (physical - glyph.width()) / 2;
            const int gy = (physical - glyph.height()) / 2;
            p.drawPixmap(gx, gy, glyph);
        }
    }

    pixmap.setDevicePixelRatio(dpr);
    s_categoryCache[key] = pixmap;
    return pixmap;
}

QPixmap AssetThumbnailCache::GetOrCreate(
    EditorCore::AssetKind kind,
    const QString& filePath,
    qint64 mtime,
    int size,
    qreal dpr,
    QObject* receiver,
    std::function<void(const QPixmap&)> onLoaded
)
{
    const ThumbnailKey key{filePath, mtime, size, dpr};

    {
        QMutexLocker locker(&mutex_);
        if (cache_.contains(key))
            return *cache_[key];
    }

    const QPixmap placeholder = CategoryIcon(kind, size, dpr);

    if (kind == EditorCore::AssetKind::Texture && !filePath.isEmpty() && receiver)
    {
        QPointer<QObject> guard(receiver);
        QThreadPool::globalInstance()->start([key, guard, onLoaded]() {
            if (guard.isNull())
                return;

            QImageReader reader(key.path);
            reader.setAutoTransform(true);
            const int physicalSize = qMax(1, qRound(key.size * key.dpr));
            const QSize orig = reader.size();
            if (orig.isValid())
                reader.setScaledSize(orig.scaled(physicalSize, physicalSize, Qt::KeepAspectRatio));

            QImage img = reader.read();
            if (!img.isNull())
            {
                img = img.scaled(physicalSize, physicalSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }

            if (guard.isNull() || img.isNull())
                return;

            QMetaObject::invokeMethod(guard.data(), [guard, key, img, onLoaded]() {
                if (guard.isNull() || img.isNull())
                    return;

                QPixmap pm = QPixmap::fromImage(img);
                pm.setDevicePixelRatio(key.dpr);
                AssetThumbnailCache::Instance().Insert(key.path, key.mtime, key.size, key.dpr, pm);
                if (onLoaded)
                    onLoaded(pm);
            }, Qt::QueuedConnection);
        });
    }

    return placeholder;
}
