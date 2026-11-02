#include "EditorTheme.h"
#include "EditorStyles.h"
#include "SettingsService.h"
#include "IconProvider.h"

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

    IconProvider::GetPixmap(QStringLiteral("chevron-up"), arrowColor, 12).save(outUp, "PNG");
    IconProvider::GetPixmap(QStringLiteral("chevron-down"), arrowColor, 12).save(outDown, "PNG");
    IconProvider::GetPixmap(QStringLiteral("chevron-right"), arrowColor, 12).save(outRight, "PNG");
}

QString EditorTheme::CurrentStyleSheet() const
{
    QString arrowUp, arrowDown, arrowRight;
    EnsureArrowIcons(mode_, arrowUp, arrowDown, arrowRight);
    return EditorStyles::BuildStyleSheet(mode_, accentColor_, arrowUp, arrowDown, arrowRight);
}

QIcon EditorTheme::CreatePlayIcon(const QColor& color) { return IconProvider::PlayIcon(color); }
QIcon EditorTheme::CreatePauseIcon(const QColor& color) { return IconProvider::PauseIcon(color); }
QIcon EditorTheme::CreateStopIcon(const QColor& color) { return IconProvider::StopIcon(color); }
QIcon EditorTheme::CreateExternalIcon(const QColor& color) { return IconProvider::ExternalIcon(color); }
QIcon EditorTheme::CreateSettingsIcon(const QColor& color) { return IconProvider::SettingsIcon(color); }
QIcon EditorTheme::CreateTranslateIcon(const QColor& color) { return IconProvider::TranslateIcon(color); }
QIcon EditorTheme::CreateRotateIcon(const QColor& color) { return IconProvider::RotateIcon(color); }
QIcon EditorTheme::CreateScaleIcon(const QColor& color) { return IconProvider::ScaleIcon(color); }
QIcon EditorTheme::CreateWorldIcon(const QColor& color) { return IconProvider::WorldIcon(color); }
QIcon EditorTheme::CreateLocalIcon() { return IconProvider::LocalIcon(); }
QIcon EditorTheme::CreateSnapIcon(const QColor& color) { return IconProvider::SnapIcon(color); }
QIcon EditorTheme::CreateResetIcon(const QColor& color) { return IconProvider::ResetIcon(color); }
QIcon EditorTheme::CreateMoreIcon(const QColor& color) { return IconProvider::MoreIcon(color); }
