#pragma once

#include "../../Core/Preview/PreviewImage.h"
#include "../../Core/Preview/TexturePreviewSource.h"

#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

namespace
{

int gPreviewChecks = 0;

void CheckPreview(bool cond, const char* msg)
{
    ++gPreviewChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (Preview): %s\n", msg);
        std::exit(1);
    }
}

bool RectFuzzy(const QRectF& r, double x, double y, double w, double h)
{
    return qAbs(r.x() - x) < 0.01 && qAbs(r.y() - y) < 0.01 && qAbs(r.width() - w) < 0.01 && qAbs(r.height() - h) < 0.01;
}

void TestPreviewImageMath()
{
    using namespace EditorCore::PreviewImage;

    CheckPreview(PhysicalSize(QSize(96, 96), 1.0) == QSize(96, 96), "PhysicalSize dpr 1");
    CheckPreview(PhysicalSize(QSize(96, 96), 2.0) == QSize(192, 192), "PhysicalSize dpr 2");
    CheckPreview(PhysicalSize(QSize(10, 10), 1.25) == QSize(13, 13), "PhysicalSize fractional dpr rounds");
    CheckPreview(PhysicalSize(QSize(0, 0), 1.0) == QSize(1, 1), "PhysicalSize clamps to >= 1");
    CheckPreview(PhysicalSize(QSize(96, 96), 0.0) == QSize(96, 96), "PhysicalSize invalid dpr falls back to 1");

    CheckPreview(RectFuzzy(AspectFitRect(QSizeF(100, 100), QSizeF(200, 200)), 0, 0, 200, 200), "AspectFit square upscales");
    CheckPreview(RectFuzzy(AspectFitRect(QSizeF(200, 100), QSizeF(200, 200)), 0, 50, 200, 100), "AspectFit wide letterboxes vertically");
    CheckPreview(RectFuzzy(AspectFitRect(QSizeF(100, 200), QSizeF(200, 200)), 50, 0, 100, 200), "AspectFit tall pillarboxes horizontally");
    CheckPreview(RectFuzzy(AspectFitRect(QSizeF(100, 100), QSizeF(200, 100)), 50, 0, 100, 100), "AspectFit non-square target");
    CheckPreview(RectFuzzy(AspectFitRect(QSizeF(0, 0), QSizeF(200, 200)), 100, 100, 0, 0), "AspectFit empty source centered");
    CheckPreview(RectFuzzy(AspectFitRect(QSizeF(100, 100), QSizeF(0, 0)), 0, 0, 0, 0), "AspectFit empty target");
}

void TestPreviewImageFit()
{
    using namespace EditorCore::PreviewImage;

    const QImage nullTile = FitCentered(QImage(), QSize(64, 64));
    CheckPreview(nullTile.size() == QSize(64, 64), "FitCentered null source yields target tile");
    CheckPreview(qAlpha(nullTile.pixel(32, 32)) == 0, "FitCentered null tile transparent");

    QImage wide(200, 100, QImage::Format_ARGB32);
    wide.fill(QColor(255, 0, 0));
    const QImage wideTile = FitCentered(wide, QSize(100, 100));
    CheckPreview(wideTile.size() == QSize(100, 100), "FitCentered wide source becomes square tile");
    CheckPreview(qAlpha(wideTile.pixel(50, 5)) == 0, "FitCentered wide tile has transparent top margin");
    CheckPreview(qAlpha(wideTile.pixel(50, 95)) == 0, "FitCentered wide tile has transparent bottom margin");
    CheckPreview(qRed(wideTile.pixel(50, 50)) > 200 && qAlpha(wideTile.pixel(50, 50)) > 200, "FitCentered wide tile center is source color");

    QImage square(50, 50, QImage::Format_ARGB32);
    square.fill(QColor(0, 0, 255));
    const QImage squareTile = FitCentered(square, QSize(100, 100));
    CheckPreview(squareTile.size() == QSize(100, 100), "FitCentered square tile size");
    CheckPreview(qBlue(squareTile.pixel(10, 10)) > 200 && qAlpha(squareTile.pixel(10, 10)) > 200, "FitCentered square fills tile");

    const QImage hidpi = FitCentered(square, PhysicalSize(QSize(96, 96), 2.0));
    CheckPreview(hidpi.size() == QSize(192, 192), "FitCentered honors HighDPI physical size");
}

void TestTexturePreviewSourceCache()
{
    EditorCore::TexturePreviewSource source;
    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(QColor(0, 255, 0));

    CheckPreview(source.Cached(QStringLiteral("/Game/a.png"), 100, QSize(96, 96), 1.0).isNull(), "Cached miss is null");
    source.Insert(QStringLiteral("/Game/a.png"), 100, QSize(96, 96), 1.0, image);
    CheckPreview(source.Count() == 1, "Insert increments count");
    const QImage hit = source.Cached(QStringLiteral("/Game/a.png"), 100, QSize(96, 96), 1.0);
    CheckPreview(!hit.isNull() && hit.size() == QSize(32, 32), "Cached hit returns stored image");
    CheckPreview(source.Cached(QStringLiteral("/Game/a.png"), 200, QSize(96, 96), 1.0).isNull(), "mtime change invalidates cache");
    CheckPreview(source.Cached(QStringLiteral("/Game/a.png"), 100, QSize(64, 64), 1.0).isNull(), "size change invalidates cache");
    source.Clear();
    CheckPreview(source.Count() == 0, "Clear empties cache");
}

void TestTexturePreviewSourceDecode()
{
    QTemporaryDir temp;
    CheckPreview(temp.isValid(), "Preview decode temp dir");
    const QString path = temp.path() + QStringLiteral("/wide.png");
    QImage wide(64, 32, QImage::Format_ARGB32);
    wide.fill(QColor(255, 0, 0));
    CheckPreview(wide.save(path, "PNG"), "Preview decode fixture saved");

    EditorCore::TexturePreviewSource source;
    QObject receiver;
    QImage delivered;
    bool ready = false;
    const QImage immediate = source.Request(path, 123, QSize(96, 96), 1.0, &receiver, [&](const QImage& img) {
        delivered = img;
        ready = true;
    });
    CheckPreview(immediate.isNull(), "Request returns null before decode completes");

    QElapsedTimer timer;
    timer.start();
    while (!ready && timer.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    CheckPreview(ready, "Request delivers decoded image on receiver thread");
    CheckPreview(delivered.size() == QSize(96, 96), "Decoded preview is a square HighDPI tile");
    CheckPreview(delivered.width() == delivered.height(), "Non-square texture fitted to square without distortion");
    CheckPreview(qRed(delivered.pixel(48, 48)) > 200, "Decoded preview center keeps source color");
    CheckPreview(qAlpha(delivered.pixel(48, 4)) == 0, "Decoded wide preview has transparent margin");
    CheckPreview(!source.Cached(path, 123, QSize(96, 96), 1.0).isNull(), "Decoded preview is cached");
}

inline int RunPreviewTests()
{
    TestPreviewImageMath();
    TestPreviewImageFit();
    TestTexturePreviewSourceCache();
    TestTexturePreviewSourceDecode();
    return gPreviewChecks;
}

} // namespace