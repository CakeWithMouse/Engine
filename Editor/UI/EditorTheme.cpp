#include "EditorTheme.h"
#include "EditorStyles.h"
#include "SettingsService.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QDir>
#include <QFileInfo>

EditorTheme& EditorTheme::Instance()
{
    static EditorTheme s_instance;
    return s_instance;
}

EditorTheme::EditorTheme()
{
    const QString modeStr = SettingsService::Instance().GetValue(QStringLiteral("ui/theme_mode"), QStringLiteral("dark")).toString();
    mode_ = (modeStr.toLower() == QStringLiteral("light")) ? ThemeMode::Light : ThemeMode::Dark;

    const QString accentStr = SettingsService::Instance().GetValue(QStringLiteral("ui/accent_color"), QStringLiteral("#DC2626")).toString();
    accentColor_ = QColor(accentStr);
    if (!accentColor_.isValid()) {
        accentColor_ = QColor(QStringLiteral("#DC2626"));
    }
}

void EditorTheme::Apply(QApplication& app)
{
    Instance().Initialize(app);
}

void EditorTheme::Initialize(QApplication& app)
{
    app_ = &app;
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QFont defaultFont(QStringLiteral("Segoe UI"), 9);
    defaultFont.setStyleHint(QFont::SansSerif);
    app.setFont(defaultFont);

    ApplyToApp();
}

void EditorTheme::SetMode(ThemeMode mode)
{
    if (mode_ == mode) return;
    mode_ = mode;
    SettingsService::Instance().SetValue(QStringLiteral("ui/theme_mode"), mode_ == ThemeMode::Light ? QStringLiteral("light") : QStringLiteral("dark"));
    ApplyToApp();
    NotifyChanged();
}

void EditorTheme::SetAccentColor(const QColor& color)
{
    if (!color.isValid() || accentColor_ == color) return;
    accentColor_ = color;
    SettingsService::Instance().SetValue(QStringLiteral("ui/accent_color"), accentColor_.name(QColor::HexRgb));
    ApplyToApp();
    NotifyChanged();
}

ThemePalette EditorTheme::Palette() const
{
    ThemePalette pal;
    const bool isDark = (mode_ == ThemeMode::Dark);
    pal.textPrimary = isDark ? QColor(0xEC, 0xEF, 0xF4) : QColor(0x1F, 0x23, 0x28);
    pal.textSecondary = isDark ? QColor(0xAB, 0xB2, 0xBF) : QColor(0x48, 0x4F, 0x58);
    pal.textDisabled = isDark ? QColor(0x5C, 0x63, 0x70) : QColor(0x8C, 0x95, 0x9F);
    pal.bgWindow = isDark ? QColor(0x18, 0x1A, 0x1F) : QColor(0xF0, 0xF2, 0xF5);
    pal.bgPanel = isDark ? QColor(0x20, 0x23, 0x2A) : QColor(0xF6, 0xF8, 0xFA);
    pal.bgDarkest = isDark ? QColor(0x14, 0x16, 0x1A) : QColor(0xFF, 0xFF, 0xFF);
    pal.bgButton = isDark ? QColor(0x21, 0x25, 0x2B) : QColor(0xEA, 0xEE, 0xF2);
    pal.bgButtonHover = isDark ? QColor(0x28, 0x2C, 0x34) : QColor(0xE1, 0xE4, 0xE8);
    pal.bgButtonPressed = isDark ? QColor(0x1C, 0x20, 0x26) : QColor(0xD8, 0xDE, 0xE4);
    pal.borderLight = isDark ? QColor(0x2C, 0x31, 0x3A) : QColor(0xD0, 0xD7, 0xDE);
    pal.borderMid = isDark ? QColor(0x3E, 0x44, 0x51) : QColor(0xAF, 0xB8, 0xC1);
    pal.accent = accentColor_;
    pal.accentHover = accentColor_.lighter(120);
    pal.axisX = QColor(QStringLiteral("#EF4444"));
    pal.axisY = QColor(QStringLiteral("#22C55E"));
    pal.axisZ = QColor(QStringLiteral("#3B82F6"));
    return pal;
}

QColor EditorTheme::TextPrimary() const { return Palette().textPrimary; }
QColor EditorTheme::TextSecondary() const { return Palette().textSecondary; }
QColor EditorTheme::BgWindow() const { return Palette().bgWindow; }
QColor EditorTheme::BgPanel() const { return Palette().bgPanel; }
QColor EditorTheme::BgDarkest() const { return Palette().bgDarkest; }
QColor EditorTheme::BorderLight() const { return Palette().borderLight; }

int EditorTheme::SubscribeThemeChanged(std::function<void()> callback)
{
    const int id = nextId_++;
    subscribers_.push_back({id, std::move(callback)});
    return id;
}

void EditorTheme::UnsubscribeThemeChanged(int id)
{
    for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it) {
        if (it->first == id) {
            subscribers_.erase(it);
            break;
        }
    }
}

void EditorTheme::NotifyChanged()
{
    for (const auto& sub : subscribers_) {
        if (sub.second) sub.second();
    }
}

void EditorTheme::ApplyToApp()
{
    if (!app_) return;

    QPalette palette;
    if (mode_ == ThemeMode::Dark)
    {
        const QColor bgDarkest(0x14, 0x16, 0x1A);
        const QColor bgDark(0x18, 0x1A, 0x1F);
        const QColor bgPanel(0x20, 0x23, 0x2A);
        const QColor bgButton(0x21, 0x25, 0x2B);
        const QColor textPrimary(0xEC, 0xEF, 0xF4);
        const QColor textSecondary(0xAB, 0xB2, 0xBF);
        const QColor textDisabled(0x5C, 0x63, 0x70);
        const QColor borderLight(0x2C, 0x31, 0x3A);

        palette.setColor(QPalette::Window, bgDark);
        palette.setColor(QPalette::WindowText, textSecondary);
        palette.setColor(QPalette::Base, bgDarkest);
        palette.setColor(QPalette::AlternateBase, QColor(0x18, 0x1B, 0x20));
        palette.setColor(QPalette::ToolTipBase, bgPanel);
        palette.setColor(QPalette::ToolTipText, textPrimary);
        palette.setColor(QPalette::Text, textPrimary);
        palette.setColor(QPalette::Button, bgButton);
        palette.setColor(QPalette::ButtonText, textPrimary);
        palette.setColor(QPalette::BrightText, Qt::white);
        palette.setColor(QPalette::Highlight, accentColor_);
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Link, accentColor_);
        palette.setColor(QPalette::Dark, QColor(0x10, 0x12, 0x15));
        palette.setColor(QPalette::Mid, QColor(0x1C, 0x1F, 0x26));
        palette.setColor(QPalette::Midlight, borderLight);
        palette.setColor(QPalette::Shadow, QColor(0x0D, 0x0E, 0x11));

        palette.setColor(QPalette::Disabled, QPalette::Text, textDisabled);
        palette.setColor(QPalette::Disabled, QPalette::WindowText, textDisabled);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled);
        palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x24, 0x2A, 0x36));
        palette.setColor(QPalette::Disabled, QPalette::HighlightedText, textDisabled);
    }
    else
    {
        const QColor bgWindow(0xF0, 0xF2, 0xF5);
        const QColor bgBase(0xFF, 0xFF, 0xFF);
        const QColor bgPanel(0xF6, 0xF8, 0xFA);
        const QColor bgButton(0xEA, 0xEE, 0xF2);
        const QColor textPrimary(0x1F, 0x23, 0x28);
        const QColor textSecondary(0x48, 0x4F, 0x58);
        const QColor textDisabled(0x8C, 0x95, 0x9F);
        const QColor borderLight(0xD0, 0xD7, 0xDE);

        palette.setColor(QPalette::Window, bgWindow);
        palette.setColor(QPalette::WindowText, textPrimary);
        palette.setColor(QPalette::Base, bgBase);
        palette.setColor(QPalette::AlternateBase, QColor(0xF6, 0xF8, 0xFA));
        palette.setColor(QPalette::ToolTipBase, bgPanel);
        palette.setColor(QPalette::ToolTipText, textPrimary);
        palette.setColor(QPalette::Text, textPrimary);
        palette.setColor(QPalette::Button, bgButton);
        palette.setColor(QPalette::ButtonText, textPrimary);
        palette.setColor(QPalette::BrightText, Qt::black);
        palette.setColor(QPalette::Highlight, accentColor_);
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Link, accentColor_);
        palette.setColor(QPalette::Dark, QColor(0xCF, 0xD4, 0xD9));
        palette.setColor(QPalette::Mid, QColor(0xDF, 0xE3, 0xE8));
        palette.setColor(QPalette::Midlight, borderLight);
        palette.setColor(QPalette::Shadow, QColor(0xA0, 0xA6, 0xAE));

        palette.setColor(QPalette::Disabled, QPalette::Text, textDisabled);
        palette.setColor(QPalette::Disabled, QPalette::WindowText, textDisabled);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled);
        palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0xD0, 0xD7, 0xDE));
        palette.setColor(QPalette::Disabled, QPalette::HighlightedText, textDisabled);
    }

    app_->setPalette(palette);
    app_->setStyleSheet(CurrentStyleSheet());
}

static void EnsureArrowIcons(ThemeMode mode, QString& outUp, QString& outDown, QString& outRight)
{
    const QString dirPath = QDir::cleanPath(QDir::tempPath() + QStringLiteral("/PainEngineTheme"));
    QDir().mkpath(dirPath);

    const bool isDark = (mode == ThemeMode::Dark);
    const QString suffix = isDark ? QStringLiteral("_dark.png") : QStringLiteral("_light.png");
    outUp = dirPath + QStringLiteral("/arrow_up") + suffix;
    outDown = dirPath + QStringLiteral("/arrow_down") + suffix;
    outRight = dirPath + QStringLiteral("/arrow_right") + suffix;

    const QColor arrowColor = isDark ? QColor(0xEC, 0xEF, 0xF4) : QColor(0x1F, 0x23, 0x28);

    auto renderTriangle = [&](const QString& path, const QPointF pts[3]) {
        QPixmap pix(12, 12);
        pix.fill(Qt::transparent);
        {
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(arrowColor);
            p.drawPolygon(pts, 3);
        }
        pix.save(path, "PNG");
    };

    const QPointF upPts[3] = { QPointF(2.0, 8.5), QPointF(6.0, 3.5), QPointF(10.0, 8.5) };
    const QPointF downPts[3] = { QPointF(2.0, 3.5), QPointF(10.0, 3.5), QPointF(6.0, 8.5) };
    const QPointF rightPts[3] = { QPointF(3.5, 2.0), QPointF(8.5, 6.0), QPointF(3.5, 10.0) };

    renderTriangle(outUp, upPts);
    renderTriangle(outDown, downPts);
    renderTriangle(outRight, rightPts);
}

QString EditorTheme::CurrentStyleSheet() const
{
    QString arrowUp, arrowDown, arrowRight;
    EnsureArrowIcons(mode_, arrowUp, arrowDown, arrowRight);
    return EditorStyles::BuildStyleSheet(mode_, accentColor_, arrowUp, arrowDown, arrowRight);
}

QIcon EditorTheme::CreatePlayIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        const QPointF points[3] = {
            QPointF(4.0, 3.0),
            QPointF(13.0, 8.0),
            QPointF(4.0, 13.0)
        };
        p.drawPolygon(points, 3);
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreatePauseIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRect(QRectF(3.0, 3.0, 3.5, 10.0));
        p.drawRect(QRectF(9.5, 3.0, 3.5, 10.0));
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateStopIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(3.5, 3.5, 9.0, 9.0), 1.5, 1.5);
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateExternalIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(2.5, 4.5, 8.0, 8.0));
        p.drawLine(QPointF(6.0, 9.5), QPointF(13.0, 2.5));
        p.drawLine(QPointF(9.5, 2.5), QPointF(13.0, 2.5));
        p.drawLine(QPointF(13.0, 2.5), QPointF(13.0, 6.0));
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateSettingsIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(4.0, 4.0, 8.0, 8.0));
        for (int i = 0; i < 8; ++i) {
            p.save();
            p.translate(8.0, 8.0);
            p.rotate(i * 45.0);
            p.drawLine(QPointF(0.0, -5.5), QPointF(0.0, -7.5));
            p.restore();
        }
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateTranslateIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(8.0, 3.5), QPointF(8.0, 12.5));
        p.drawLine(QPointF(3.5, 8.0), QPointF(12.5, 8.0));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        const QPointF up[3] = { QPointF(5.5, 5.0), QPointF(8.0, 1.5), QPointF(10.5, 5.0) };
        const QPointF down[3] = { QPointF(5.5, 11.0), QPointF(8.0, 14.5), QPointF(10.5, 11.0) };
        const QPointF left[3] = { QPointF(5.0, 5.5), QPointF(1.5, 8.0), QPointF(5.0, 10.5) };
        const QPointF right[3] = { QPointF(11.0, 5.5), QPointF(14.5, 8.0), QPointF(11.0, 10.5) };
        p.drawPolygon(up, 3);
        p.drawPolygon(down, 3);
        p.drawPolygon(left, 3);
        p.drawPolygon(right, 3);
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateRotateIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(2.5, 2.5, 11.0, 11.0), 30 * 16, 275 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        const QPointF arrow[3] = { QPointF(10.0, 2.0), QPointF(14.5, 4.5), QPointF(11.5, 7.5) };
        p.drawPolygon(arrow, 3);
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateScaleIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRect(QRectF(2.0, 9.5, 4.5, 4.5));
        p.drawRect(QRectF(8.5, 2.0, 5.5, 5.5));
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(6.0, 10.0), QPointF(9.5, 6.5));
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateWorldIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.4));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(2.5, 2.5, 11.0, 11.0));
        p.drawLine(QPointF(2.5, 8.0), QPointF(13.5, 8.0));
        p.drawEllipse(QRectF(5.0, 2.5, 6.0, 11.0));
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateLocalIcon()
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF o(8.0, 9.0);
        p.setPen(QPen(QColor(0x22, 0xC5, 0x5E), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(o, QPointF(8.0, 2.5));
        p.setPen(QPen(QColor(0xEF, 0x44, 0x44), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(o, QPointF(2.5, 12.5));
        p.setPen(QPen(QColor(0x3B, 0x82, 0xF6), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(o, QPointF(13.5, 12.5));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xFB, 0xBF, 0x24));
        p.drawEllipse(QRectF(6.5, 7.5, 3.0, 3.0));
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateSnapIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::FlatCap));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(4.0, 5.0);
        path.lineTo(4.0, 9.5);
        path.arcTo(QRectF(4.0, 5.5, 8.0, 8.0), 180, -180);
        path.lineTo(12.0, 5.0);
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xEF, 0x44, 0x44));
        p.drawRect(QRectF(2.5, 2.5, 3.0, 2.5));
        p.setBrush(QColor(0x3B, 0x82, 0xF6));
        p.drawRect(QRectF(10.5, 2.5, 3.0, 2.5));
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateResetIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(2.5, 2.5, 11.0, 11.0), 40 * 16, 270 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        const QPointF arrow[3] = { QPointF(10.0, 1.5), QPointF(6.0, 4.0), QPointF(9.5, 6.5) };
        p.drawPolygon(arrow, 3);
    }
    return QIcon(pix);
}

QIcon EditorTheme::CreateMoreIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QRectF(1.5, 6.5, 3.0, 3.0));
        p.drawEllipse(QRectF(6.5, 6.5, 3.0, 3.0));
        p.drawEllipse(QRectF(11.5, 6.5, 3.0, 3.0));
    }
    return QIcon(pix);
}
