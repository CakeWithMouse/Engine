#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <functional>

class QObject;

namespace EditorCore
{

// A static (QImage) preview provider: decodes an asset once, caches the result and
// delivers it asynchronously. Request-driven — no idle frames, no repaint timer.
// Implementations live in Core (QtCore/QtGui only); the realtime HWND preview is a
// separate UI concern (PreviewWidget) and does not implement this interface.
class IPreviewSource
{
public:
    virtual ~IPreviewSource() = default;

    // Cached image for an asset at a logical size and device pixel ratio, or a null
    // image when it has not been decoded yet. mtime invalidates stale entries.
    virtual QImage Cached(const QString& asset, qint64 mtime, QSize size, qreal dpr) const = 0;

    // Returns the cached image immediately when present; otherwise schedules a decode
    // and invokes onReady on the receiver's thread (queued) once ready. The delivery is
    // discarded safely if the receiver is destroyed first. Never blocks the caller.
    virtual QImage Request(const QString& asset, qint64 mtime, QSize size, qreal dpr,
                           QObject* receiver, std::function<void(const QImage&)> onReady) = 0;
};

} // namespace EditorCore