#include "PreviewImage.h"

#include <QPainter>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace EditorCore::PreviewImage
{

QSize PhysicalSize(QSize logical, qreal dpr)
{
    const qreal ratio = std::isfinite(static_cast<double>(dpr)) && dpr > 0.0 ? dpr : 1.0;
    const int w = std::max(1, static_cast<int>(std::lround(logical.width() * ratio)));
    const int h = std::max(1, static_cast<int>(std::lround(logical.height() * ratio)));
    return QSize(w, h);
}

QRectF AspectFitRect(QSizeF source, QSizeF target)
{
    if (target.width() <= 0.0 || target.height() <= 0.0)
        return QRectF(0.0, 0.0, 0.0, 0.0);
    if (source.width() <= 0.0 || source.height() <= 0.0)
        return QRectF(target.width() / 2.0, target.height() / 2.0, 0.0, 0.0);

    const qreal scale = std::min(target.width() / source.width(), target.height() / source.height());
    const QSizeF fitted(source.width() * scale, source.height() * scale);
    const QPointF offset((target.width() - fitted.width()) / 2.0, (target.height() - fitted.height()) / 2.0);
    return QRectF(offset, fitted);
}

QImage FitCentered(const QImage& src, QSize targetPhysical)
{
    const QSize target = PhysicalSize(targetPhysical, 1.0);
    QImage tile(target, QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::transparent);
    if (src.isNull())
        return tile;

    const QRectF fit = AspectFitRect(QSizeF(src.size()), QSizeF(target));
    const QSize fitted(std::max(1, static_cast<int>(std::lround(fit.width()))),
                       std::max(1, static_cast<int>(std::lround(fit.height()))));
    const QImage scaled = src.scaled(fitted, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPainter painter(&tile);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QPointF(fit.x(), fit.y()), scaled);
    return tile;
}

} // namespace EditorCore::PreviewImage