#include "EditorTheme.h"
#include "EditorStyles.h"
#include "SettingsService.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QFont>
#include <QPainter>
#include <QPixmap>
#include <QPointF>

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
    const bool isDark = (mode_ == ThemeMode::Dark);

    if (isDark)
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

QString EditorTheme::CurrentStyleSheet() const
{
    return EditorStyles::BuildStyleSheet(mode_, accentColor_);
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

