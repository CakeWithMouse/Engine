#include "IconProvider.h"
#include "EditorIconData.h"
#include "IEditorHost.h"
#ifdef ENGINE_HAS_QTSVG
#include <QSvgRenderer>
#endif
#include <QPainter>
#include <QImage>
#include <QByteArray>
#include <cstring>

void IconProvider::UploadIcons(IEditorHost* host, bool force)
{
    if (!host) return;
    (void)force;

    auto renderSvg = [](const char* svgData, EditorIcons::IconId id, int size = 64) -> QImage {
        QImage image(size, size, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);

        const char* svgStart = std::strstr(svgData, "<svg");
        QByteArray bytes(svgStart ? svgStart : svgData);
#ifdef ENGINE_HAS_QTSVG
        QSvgRenderer renderer(bytes);
        if (renderer.isValid())
        {
            QPainter painter(&image);
            renderer.render(&painter);
        }

#else
        image.loadFromData(bytes, "SVG");
        if (image.isNull()) { image = QImage(size, size, QImage::Format_RGBA8888); image.fill(Qt::transparent); }
        else image = image.scaled(size, size).convertToFormat(QImage::Format_RGBA8888);
#endif
        bool hasPixels = false;
        for (int y = 0; y < size && !hasPixels; ++y)
        {
            const auto* line = image.constScanLine(y);
            for (int x = 0; x < size * 4; ++x)
            {
                if (line[x] != 0) { hasPixels = true; break; }
            }
        }

        if (!hasPixels)
        {
            QPainter p(&image);
            p.setRenderHint(QPainter::Antialiasing, true);
            if (id == EditorIcons::IconId::PointLight)
            {
                p.setBrush(QColor(255, 230, 80, 240));
                p.setPen(QPen(QColor(255, 255, 255, 255), 2.0));
                p.drawEllipse(size / 4, size / 4, size / 2, size / 2);
            }
            else if (id == EditorIcons::IconId::SunSky)
            {
                p.setBrush(QColor(255, 200, 50, 240));
                p.setPen(QPen(QColor(255, 240, 150, 255), 2.0));
                p.drawEllipse(size / 4, size / 4, size / 2, size / 2);
            }
            else
            {
                p.setBrush(QColor(80, 180, 255, 220));
                p.setPen(QPen(Qt::white, 2.0));
                p.drawRect(size / 4, size / 4, size / 2, size / 2);
            }
        }

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

    static const auto images = [&] {
        std::vector<QImage> result;
        for (const auto& item : icons) result.push_back(renderSvg(item.svg, item.id, 64));
        return result;
    }();
    for (size_t i = 0; i < images.size(); ++i)
        host->SetOverlayIcon(static_cast<uint32_t>(icons[i].id), 64, 64, images[i].constBits());
}
