#include "IconProvider.h"
#include "EditorIconData.h"
#include "IEditorHost.h"
#include <QSvgRenderer>
#include <QPainter>
#include <QImage>
#include <QByteArray>

void IconProvider::UploadIcons(IEditorHost* host, bool force)
{
    if (!host) return;
    static bool sUploaded = false;
    if (sUploaded && !force) return;

    auto renderSvg = [](const char* svgData, int size = 64) -> QImage {
        QByteArray bytes(svgData);
        QSvgRenderer renderer(bytes);
        QImage image(size, size, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter);
        return image;
    };

    struct IconEntry {
        EditorIcons::IconId id;
        const char* svg;
    };

    const IconEntry icons[] = {
        { EditorIcons::IconId::PointLight, EditorIcons::SVG_BULB },
        { EditorIcons::IconId::SunSky,     EditorIcons::SVG_SUN },
        { EditorIcons::IconId::Camera,     EditorIcons::SVG_CAMERA },
        { EditorIcons::IconId::GameCamera, EditorIcons::SVG_VIDEO },
        { EditorIcons::IconId::Particles,  EditorIcons::SVG_PARTICLES },
        { EditorIcons::IconId::Character,  EditorIcons::SVG_USER }
    };

    for (const auto& item : icons)
    {
        QImage img = renderSvg(item.svg, 64);
        host->SetOverlayIcon(static_cast<uint32_t>(item.id), 64, 64, img.constBits());
    }

    sUploaded = true;
}
