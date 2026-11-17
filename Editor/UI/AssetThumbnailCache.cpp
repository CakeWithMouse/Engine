#include "AssetThumbnailCache.h"
#include "EditorTheme.h"
#include "IconProvider.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>

AssetThumbnailCache& AssetThumbnailCache::Instance()
{
    static AssetThumbnailCache instance;
    return instance;
}

QPixmap AssetThumbnailCache::FindInCache(const QString& filePath, qint64 mtime, int size, qreal dpr) const
{
    const QImage image = source_.Cached(filePath, mtime, QSize(size, size), dpr);
    if (image.isNull())
        return QPixmap();
    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

void AssetThumbnailCache::Insert(const QString& filePath, qint64 mtime, int size, qreal dpr, const QPixmap& pixmap)
{
    source_.Insert(filePath, mtime, QSize(size, size), dpr, pixmap.toImage());
}

void AssetThumbnailCache::Clear()
{
    source_.Clear();
}

int AssetThumbnailCache::Count() const
{
    return source_.Count();
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
    const QSize logical(size, size);
    const QImage cached = source_.Cached(filePath, mtime, logical, dpr);
    if (!cached.isNull())
    {
        QPixmap pixmap = QPixmap::fromImage(cached);
        pixmap.setDevicePixelRatio(dpr);
        return pixmap;
    }

    const QPixmap placeholder = CategoryIcon(kind, size, dpr);

    if (kind == EditorCore::AssetKind::Texture && !filePath.isEmpty() && receiver)
    {
        source_.Request(filePath, mtime, logical, dpr, receiver, [dpr, onLoaded](const QImage& image) {
            QPixmap pixmap = QPixmap::fromImage(image);
            pixmap.setDevicePixelRatio(dpr);
            if (onLoaded)
                onLoaded(pixmap);
        });
    }

    return placeholder;
}
