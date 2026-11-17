#pragma once

#include <QImage>
#include <QRectF>
#include <QSize>
#include <QSizeF>

namespace EditorCore::PreviewImage
{

// Physical pixel size for a logical size at a device pixel ratio (each side >= 1).
QSize PhysicalSize(QSize logical, qreal dpr);

// Centered aspect-fit rectangle of source inside target: no distortion, no crop.
// Degenerate source (empty) yields a zero-size rect centered in target.
QRectF AspectFitRect(QSizeF source, QSizeF target);

// Scale src to fit targetPhysical keeping aspect, centered on a transparent
// targetPhysical-sized canvas. Non-square sources become square tiles with even
// margins instead of a stretched or unevenly letterboxed image.
QImage FitCentered(const QImage& src, QSize targetPhysical);

} // namespace EditorCore::PreviewImage