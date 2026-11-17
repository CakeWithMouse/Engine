#include "TexturePreviewSource.h"
#include "PreviewImage.h"

#include <QImageReader>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QThreadPool>

namespace EditorCore
{

TexturePreviewSource::TexturePreviewSource()
{
    cache_.setMaxCost(256);
}

QImage TexturePreviewSource::Cached(const QString& asset, qint64 mtime, QSize size, qreal dpr) const
{
    QMutexLocker locker(&mutex_);
    const PreviewKey key{asset, mtime, size, dpr};
    if (cache_.contains(key))
        return *cache_[key];
    return QImage();
}

void TexturePreviewSource::Insert(const QString& asset, qint64 mtime, QSize size, qreal dpr, const QImage& image)
{
    QMutexLocker locker(&mutex_);
    cache_.insert(PreviewKey{asset, mtime, size, dpr}, new QImage(image));
}

void TexturePreviewSource::Clear()
{
    QMutexLocker locker(&mutex_);
    cache_.clear();
}

int TexturePreviewSource::Count() const
{
    QMutexLocker locker(&mutex_);
    return cache_.count();
}

QImage TexturePreviewSource::Request(const QString& asset, qint64 mtime, QSize size, qreal dpr,
                                     QObject* receiver, std::function<void(const QImage&)> onReady)
{
    {
        QMutexLocker locker(&mutex_);
        const PreviewKey key{asset, mtime, size, dpr};
        if (cache_.contains(key))
            return *cache_[key];
    }

    if (asset.isEmpty() || !receiver)
        return QImage();

    const PreviewKey key{asset, mtime, size, dpr};
    QPointer<QObject> guard(receiver);
    // The source must outlive the receiver (the AssetThumbnailCache adapter is a singleton).
    QThreadPool::globalInstance()->start([this, key, guard, onReady]() {
        if (guard.isNull())
            return;

        QImageReader reader(key.path);
        reader.setAutoTransform(true);
        const QSize physical = PreviewImage::PhysicalSize(key.size, key.dpr);
        const QSize orig = reader.size();
        if (orig.isValid())
            reader.setScaledSize(orig.scaled(physical, Qt::KeepAspectRatio));

        QImage img = reader.read();
        if (guard.isNull() || img.isNull())
            return;
        img = PreviewImage::FitCentered(img, physical);

        QMetaObject::invokeMethod(guard.data(), [this, guard, key, img, onReady]() {
            if (guard.isNull() || img.isNull())
                return;
            Insert(key.path, key.mtime, key.size, key.dpr, img);
            if (onReady)
                onReady(img);
        }, Qt::QueuedConnection);
    });

    return QImage();
}

} // namespace EditorCore