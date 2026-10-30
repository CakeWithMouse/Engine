#pragma once

#include <QString>
#include <QColor>
#include <QIcon>
#include <functional>
#include <vector>

class QApplication;

enum class ThemeMode
{
    Dark,
    Light
};

struct ThemePalette
{
    QColor textPrimary;
    QColor textSecondary;
    QColor textDisabled;
    QColor bgWindow;
    QColor bgPanel;
    QColor bgDarkest;
    QColor bgButton;
    QColor bgButtonHover;
    QColor bgButtonPressed;
    QColor borderLight;
    QColor borderMid;
    QColor accent;
    QColor accentHover;
    QColor axisX;
    QColor axisY;
    QColor axisZ;
};

class EditorTheme
{
public:
    static EditorTheme& Instance();
    static void Apply(QApplication& app);

    void Initialize(QApplication& app);

    ThemeMode Mode() const { return mode_; }
    void SetMode(ThemeMode mode);

    QColor AccentColor() const { return accentColor_; }
    void SetAccentColor(const QColor& color);

    ThemePalette Palette() const;
    QColor TextPrimary() const;
    QColor TextSecondary() const;
    QColor BgWindow() const;
    QColor BgPanel() const;
    QColor BgDarkest() const;
    QColor BorderLight() const;
    QColor AxisX() const { return QColor(QStringLiteral("#EF4444")); }
    QColor AxisY() const { return QColor(QStringLiteral("#22C55E")); }
    QColor AxisZ() const { return QColor(QStringLiteral("#3B82F6")); }

    int SubscribeThemeChanged(std::function<void()> callback);
    void UnsubscribeThemeChanged(int id);

    QString CurrentStyleSheet() const;

    // Vector icons for toolbar (without emoji)
    static QIcon CreatePlayIcon(const QColor& color = QColor(0x4A, 0xDE, 0x80));
    static QIcon CreatePauseIcon(const QColor& color = QColor(0xFB, 0xBF, 0x24));
    static QIcon CreateStopIcon(const QColor& color = QColor(0xF8, 0xF1, 0x71));
    static QIcon CreateExternalIcon(const QColor& color = QColor(0x60, 0xA5, 0xFA));
    static QIcon CreateSettingsIcon(const QColor& color = QColor(0x94, 0xA3, 0xB8));

private:
    EditorTheme();
    void ApplyToApp();
    void NotifyChanged();

    ThemeMode mode_ = ThemeMode::Dark;
    QColor accentColor_ = QColor(QStringLiteral("#DC2626")); // Pain Red accent by default
    QApplication* app_ = nullptr;
    int nextId_ = 1;
    std::vector<std::pair<int, std::function<void()>>> subscribers_;
};
