#include "IconProvider.h"
#include "EditorIconData.h"
#include "EditorTheme.h"
#include "IEditorHost.h"
#ifdef ENGINE_HAS_QTSVG
#include <QSvgRenderer>
#endif
#include <QPainter>
#include <QImage>
#include <QByteArray>
#include <vector>

namespace
{
    const char* GetTablerSvg(const QString& name)
    {
        if (name == QLatin1String("player-play") || name == QLatin1String("play")) return EditorIcons::TABLER_PLAYER_PLAY;
        if (name == QLatin1String("player-pause") || name == QLatin1String("pause")) return EditorIcons::TABLER_PLAYER_PAUSE;
        if (name == QLatin1String("player-stop") || name == QLatin1String("stop")) return EditorIcons::TABLER_PLAYER_STOP;
        if (name == QLatin1String("external-link") || name == QLatin1String("external")) return EditorIcons::TABLER_EXTERNAL_LINK;
        if (name == QLatin1String("adjustments")) return EditorIcons::TABLER_ADJUSTMENTS;
        if (name == QLatin1String("settings")) return EditorIcons::TABLER_SETTINGS;
        if (name == QLatin1String("arrows-move") || name == QLatin1String("translate")) return EditorIcons::TABLER_ARROWS_MOVE;
        if (name == QLatin1String("rotate")) return EditorIcons::TABLER_ROTATE;
        if (name == QLatin1String("maximize") || name == QLatin1String("scale")) return EditorIcons::TABLER_MAXIMIZE;
        if (name == QLatin1String("world")) return EditorIcons::TABLER_WORLD;
        if (name == QLatin1String("cube") || name == QLatin1String("local")) return EditorIcons::TABLER_CUBE;
        if (name == QLatin1String("magnet") || name == QLatin1String("snap")) return EditorIcons::TABLER_MAGNET;
        if (name == QLatin1String("refresh") || name == QLatin1String("reset")) return EditorIcons::TABLER_REFRESH;
        if (name == QLatin1String("dots") || name == QLatin1String("more")) return EditorIcons::TABLER_DOTS;
        if (name == QLatin1String("eye")) return EditorIcons::TABLER_EYE;
        if (name == QLatin1String("eye-off")) return EditorIcons::TABLER_EYE_OFF;
        if (name == QLatin1String("lock")) return EditorIcons::TABLER_LOCK;
        if (name == QLatin1String("lock-open")) return EditorIcons::TABLER_LOCK_OPEN;
        if (name == QLatin1String("download") || name == QLatin1String("import")) return EditorIcons::TABLER_DOWNLOAD;
        if (name == QLatin1String("plus") || name == QLatin1String("add")) return EditorIcons::TABLER_PLUS;
        if (name == QLatin1String("trash") || name == QLatin1String("remove")) return EditorIcons::TABLER_TRASH;
        if (name == QLatin1String("folder")) return EditorIcons::TABLER_FOLDER;
        if (name == QLatin1String("file")) return EditorIcons::TABLER_FILE;
        if (name == QLatin1String("search")) return EditorIcons::TABLER_SEARCH;
        if (name == QLatin1String("bulb") || name == QLatin1String("light")) return EditorIcons::TABLER_BULB;
        if (name == QLatin1String("camera")) return EditorIcons::SVG_CAMERA;
        if (name == QLatin1String("particles")) return EditorIcons::SVG_PARTICLES;
        return nullptr;
    }

    QPixmap RenderSvgPixmap(const char* svgData, const QColor& color, int size)
    {
        if (!svgData) return QPixmap();
        QString svgStr = QString::fromUtf8(svgData);
        svgStr.replace(QStringLiteral("currentColor"), color.name(QColor::HexRgb));
        svgStr.replace(QStringLiteral("#FFFFFF"), color.name(QColor::HexRgb));
        svgStr.replace(QStringLiteral("#ffffff"), color.name(QColor::HexRgb));
        const QByteArray bytes = svgStr.toUtf8();

        QPixmap pix(size, size);
        pix.fill(Qt::transparent);
#ifdef ENGINE_HAS_QTSVG
        QSvgRenderer renderer(bytes);
        if (renderer.isValid())
        {
            QPainter painter(&pix);
            renderer.render(&painter);
            return pix;
        }
#endif
        QImage img;
        img.loadFromData(bytes, "SVG");
        if (!img.isNull())
        {
            return QPixmap::fromImage(img.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        return pix;
    }
}

QPixmap IconProvider::GetPixmap(const QString& name, const QColor& color, int size)
{
    const char* svg = GetTablerSvg(name);
    const auto& theme = EditorTheme::Instance();
    const QColor baseColor = color.isValid() ? color : theme.TextPrimary();
    return RenderSvgPixmap(svg, baseColor, size);
}

QIcon IconProvider::GetIcon(const QString& name, const QColor& color, int size)
{
    const char* svg = GetTablerSvg(name);
    if (!svg) return QIcon();

    const auto& theme = EditorTheme::Instance();
    const QColor baseColor = color.isValid() ? color : theme.TextPrimary();
    const QColor disabledColor = theme.Palette().textDisabled;
    const QColor activeColor = theme.Palette().accent;

    QIcon icon;
    icon.addPixmap(RenderSvgPixmap(svg, baseColor, size), QIcon::Normal, QIcon::Off);
    icon.addPixmap(RenderSvgPixmap(svg, disabledColor, size), QIcon::Disabled, QIcon::Off);
    icon.addPixmap(RenderSvgPixmap(svg, activeColor, size), QIcon::Active, QIcon::Off);
    icon.addPixmap(RenderSvgPixmap(svg, QColor(0xFF, 0xFF, 0xFF), size), QIcon::Selected, QIcon::Off);
    return icon;
}

QIcon IconProvider::PlayIcon(const QColor& color) { return GetIcon(QStringLiteral("player-play"), color.isValid() ? color : QColor(0x4A, 0xDE, 0x80)); }
QIcon IconProvider::PauseIcon(const QColor& color) { return GetIcon(QStringLiteral("player-pause"), color.isValid() ? color : QColor(0xFB, 0xBF, 0x24)); }
QIcon IconProvider::StopIcon(const QColor& color) { return GetIcon(QStringLiteral("player-stop"), color.isValid() ? color : QColor(0xEF, 0x44, 0x44)); }
QIcon IconProvider::ExternalIcon(const QColor& color) { return GetIcon(QStringLiteral("external-link"), color.isValid() ? color : QColor(0x60, 0xA5, 0xFA)); }
QIcon IconProvider::SettingsIcon(const QColor& color) { return GetIcon(QStringLiteral("settings"), color); }
QIcon IconProvider::TranslateIcon(const QColor& color) { return GetIcon(QStringLiteral("arrows-move"), color); }
QIcon IconProvider::RotateIcon(const QColor& color) { return GetIcon(QStringLiteral("rotate"), color); }
QIcon IconProvider::ScaleIcon(const QColor& color) { return GetIcon(QStringLiteral("maximize"), color); }
QIcon IconProvider::WorldIcon(const QColor& color) { return GetIcon(QStringLiteral("world"), color); }
QIcon IconProvider::LocalIcon() { return GetIcon(QStringLiteral("cube")); }
QIcon IconProvider::SnapIcon(const QColor& color) { return GetIcon(QStringLiteral("magnet"), color); }
QIcon IconProvider::ResetIcon(const QColor& color) { return GetIcon(QStringLiteral("refresh"), color); }
QIcon IconProvider::MoreIcon(const QColor& color) { return GetIcon(QStringLiteral("dots"), color); }
QIcon IconProvider::EyeIcon(bool visible, const QColor& color) { return GetIcon(visible ? QStringLiteral("eye") : QStringLiteral("eye-off"), color); }
QIcon IconProvider::LockIcon(bool locked, const QColor& color) { return GetIcon(locked ? QStringLiteral("lock") : QStringLiteral("lock-open"), color); }
QIcon IconProvider::ImportIcon(const QColor& color) { return GetIcon(QStringLiteral("import"), color); }
QIcon IconProvider::PlusIcon(const QColor& color) { return GetIcon(QStringLiteral("plus"), color); }
QIcon IconProvider::TrashIcon(const QColor& color) { return GetIcon(QStringLiteral("trash"), color); }
QIcon IconProvider::FolderIcon(const QColor& color) { return GetIcon(QStringLiteral("folder"), color); }
QIcon IconProvider::FileIcon(const QColor& color) { return GetIcon(QStringLiteral("file"), color); }
QIcon IconProvider::SearchIcon(const QColor& color) { return GetIcon(QStringLiteral("search"), color); }
QIcon IconProvider::BulbIcon(const QColor& color) { return GetIcon(QStringLiteral("bulb"), color); }
QIcon IconProvider::CubeIcon(const QColor& color) { return GetIcon(QStringLiteral("cube"), color); }
QIcon IconProvider::AdjustmentsIcon(const QColor& color) { return GetIcon(QStringLiteral("adjustments"), color); }

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
