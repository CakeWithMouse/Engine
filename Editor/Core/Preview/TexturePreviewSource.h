#pragma once

#include "IPreviewSource.h"

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QSize>
#include <QString>

namespace EditorCore
{

struct PreviewKey
{
    QString path;
    qint64 mtime = 0;
    QSize size{96, 96};
    qreal dpr = 1.0;

    bool operator==(const PreviewKey& o) const
    {
        return size == o.size && qFuzzyCompare(dpr, o.dpr) && mtime == o.mtime && path == o.path;
    }
};

inline size_t qHash(const PreviewKey& k, size_t seed = 0)
{
    return ::qHash(k.path, seed) ^ ::qHash(k.mtime) ^ ::qHash(k.size.width()) ^ ::qHash(k.size.height()) ^
           ::qHash(static_cast<int>(k.dpr * 100));
}

// Static texture previews: decodes an image file on a worker thread, fits it aspect-correct
// onto a square HighDPI tile (PreviewImage) and caches the QImage in an LRU. Request-driven:
// nothing is decoded or repainted until asked, so an idle picker costs zero frames.
class TexturePreviewSource final : public IPreviewSource
{
public:
    TexturePreviewSource();
    ~TexturePreviewSource() override = default;
    TexturePreviewSource(const TexturePreviewSource&) = delete;
    TexturePreviewSource& operator=(const TexturePreviewSource&) = delete;

    QImage Cached(const QString& asset, qint64 mtime, QSize size, qreal dpr) const override;
    QImage Request(const QString& asset, qint64 mtime, QSize size, qreal dpr,
                   QObject* receiver, std::function<void(const QImage&)> onReady) override;

    void Insert(const QString& asset, qint64 mtime, QSize size, qreal dpr, const QImage& image);
    void Clear();
    int Count() const;

private:
    mutable QMutex mutex_;
    QCache<PreviewKey, QImage> cache_;
};

} // namespace EditorCore