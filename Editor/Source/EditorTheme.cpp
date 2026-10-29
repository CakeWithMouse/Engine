#include "EditorTheme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QFont>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QSettings>

EditorTheme& EditorTheme::Instance()
{
    static EditorTheme s_instance;
    return s_instance;
}

EditorTheme::EditorTheme()
{
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    const QString modeStr = settings.value(QStringLiteral("ui/theme_mode"), QStringLiteral("dark")).toString();
    mode_ = (modeStr.toLower() == QStringLiteral("light")) ? ThemeMode::Light : ThemeMode::Dark;

    const QString accentStr = settings.value(QStringLiteral("ui/accent_color"), QStringLiteral("#3B82F6")).toString();
    accentColor_ = QColor(accentStr);
    if (!accentColor_.isValid()) {
        accentColor_ = QColor(QStringLiteral("#3B82F6"));
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
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    settings.setValue(QStringLiteral("ui/theme_mode"), mode_ == ThemeMode::Light ? QStringLiteral("light") : QStringLiteral("dark"));
    ApplyToApp();
    NotifyChanged();
}

void EditorTheme::SetAccentColor(const QColor& color)
{
    if (!color.isValid() || accentColor_ == color) return;
    accentColor_ = color;
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    settings.setValue(QStringLiteral("ui/accent_color"), accentColor_.name(QColor::HexRgb));
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
        const QColor bgDarkest(0x14, 0x16, 0x1A);    // #14161A (поля ввода, списки, консоль)
        const QColor bgDark(0x18, 0x1A, 0x1F);       // #181A1F (окно, доки, тулбар)
        const QColor bgPanel(0x20, 0x23, 0x2A);      // #20232A (контент панелей)
        const QColor bgButton(0x21, 0x25, 0x2B);     // #21252B (кнопки, селекторы)
        const QColor textPrimary(0xEC, 0xEF, 0xF4);  // #ECEFF4 (основной текст)
        const QColor textSecondary(0xAB, 0xB2, 0xBF);// #ABB2BF (второстепенный текст)
        const QColor textDisabled(0x5C, 0x63, 0x70); // #5C6370 (неактивный текст)
        const QColor borderLight(0x2C, 0x31, 0x3A);  // #2C313A (границы)

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
        const QColor bgWindow(0xF0, 0xF2, 0xF5);     // #F0F2F5 (светлое окно)
        const QColor bgBase(0xFF, 0xFF, 0xFF);       // #FFFFFF (списки, поля ввода)
        const QColor bgPanel(0xF6, 0xF8, 0xFA);      // #F6F8FA
        const QColor bgButton(0xEA, 0xEE, 0xF2);     // #EAEEF2
        const QColor textPrimary(0x1F, 0x23, 0x28);  // #1F2328
        const QColor textSecondary(0x48, 0x4F, 0x58);// #484F58
        const QColor textDisabled(0x8C, 0x95, 0x9F); // #8C959F
        const QColor borderLight(0xD0, 0xD7, 0xDE);  // #D0D7DE

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
    const bool isDark = (mode_ == ThemeMode::Dark);
    const QString accent = accentColor_.name(QColor::HexRgb).toUpper();
    const QString accentHover = accentColor_.lighter(120).name(QColor::HexRgb).toUpper();

    const QString bgWindow = isDark ? QStringLiteral("#181A1F") : QStringLiteral("#F0F2F5");
    const QString bgDarkest = isDark ? QStringLiteral("#14161A") : QStringLiteral("#FFFFFF");
    const QString bgPanel = isDark ? QStringLiteral("#20232A") : QStringLiteral("#F6F8FA");
    const QString bgButton = isDark ? QStringLiteral("#21252B") : QStringLiteral("#EAEEF2");
    const QString bgButtonHover = isDark ? QStringLiteral("#282C34") : QStringLiteral("#E1E4E8");
    const QString bgButtonPressed = isDark ? QStringLiteral("#1C2026") : QStringLiteral("#D8DEE4");
    const QString textPrimary = isDark ? QStringLiteral("#ECEFF4") : QStringLiteral("#1F2328");
    const QString textSecondary = isDark ? QStringLiteral("#ABB2BF") : QStringLiteral("#484F58");
    const QString textDisabled = isDark ? QStringLiteral("#5C6370") : QStringLiteral("#8C959F");
    const QString borderLight = isDark ? QStringLiteral("#2C313A") : QStringLiteral("#D0D7DE");
    const QString borderMid = isDark ? QStringLiteral("#3E4451") : QStringLiteral("#AFB8C1");

    return QString(R"QSS(
/* === Общие базовые стили === */
QWidget {
    color: %1;
    selection-background-color: %2;
    selection-color: #FFFFFF;
    outline: none;
}

QMainWindow, QDialog, QDockWidget, QFrame, QScrollArea, QTabWidget::pane {
    background-color: %3;
    color: %1;
}

/* === Заголовки и стилизация QDockWidget === */
QDockWidget {
    color: %4;
    font-weight: 600;
    font-size: 8.5pt;
    titlebar-close-icon: none;
    titlebar-normal-icon: none;
}

QDockWidget::title {
    background-color: %5;
    text-align: left;
    padding: 3px 8px;
    border-bottom: 1px solid %6;
}

QDockWidget::title:hover {
    background-color: %7;
    color: %1;
}

QDockWidget::close-button, QDockWidget::float-button {
    background: transparent;
    border: none;
    padding: 2px;
    subcontrol-position: top right;
    subcontrol-origin: margin;
    position: absolute;
    top: 2px; right: 4px;
}

QDockWidget::close-button:hover, QDockWidget::float-button:hover {
    background-color: %8;
    border-radius: 2px;
}

/* === Сплиттеры и разделители === */
QMainWindow::separator {
    background-color: %5;
    width: 3px;
    height: 3px;
    border: none;
}

QMainWindow::separator:hover {
    background-color: %2;
}

/* === Менюбар и выпадающие меню === */
QMenuBar {
    background-color: %5;
    color: %4;
    border-bottom: 1px solid %6;
    padding: 1px 4px;
}

QMenuBar::item {
    background: transparent;
    padding: 4px 8px;
    border-radius: 3px;
}

QMenuBar::item:selected {
    background-color: %8;
    color: %1;
}

QMenu {
    background-color: %7;
    color: %1;
    border: 1px solid %6;
    border-radius: 4px;
    padding: 3px;
}

QMenu::item {
    padding: 4px 22px 4px 10px;
    border-radius: 3px;
}

QMenu::item:selected {
    background-color: %2;
    color: #FFFFFF;
}

QMenu::separator {
    height: 1px;
    background-color: %6;
    margin: 3px 6px;
}

/* === Главный Тулбар === */
QToolBar {
    background-color: %3;
    border-bottom: 1px solid %6;
    spacing: 3px;
    padding: 2px 4px;
}

QToolBar::separator {
    width: 1px;
    background-color: %6;
    margin: 3px 6px;
}

/* === Кнопки (QPushButton, QToolButton) === */
QPushButton, QToolButton {
    background-color: %8;
    color: %1;
    border: 1px solid %6;
    border-radius: 3px;
    padding: 4px 10px;
    font-weight: 500;
}

QPushButton:hover, QToolButton:hover {
    background-color: %9;
    border-color: %10;
}

QPushButton:pressed, QToolButton:pressed {
    background-color: %11;
    border-color: %2;
}

QPushButton:disabled, QToolButton:disabled {
    background-color: %5;
    color: %12;
    border-color: %6;
}

QPushButton#AddPrimitive {
    background-color: %2;
    color: #FFFFFF;
    font-weight: bold;
    border: none;
}

QPushButton#AddPrimitive:hover {
    background-color: %13;
}

/* === Поля ввода (QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox) === */
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox {
    background-color: %5;
    color: %1;
    border: 1px solid %6;
    border-radius: 3px;
    padding: 3px 6px;
}

QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border: 1px solid %2;
}

QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
    background-color: %5;
    color: %12;
    border-color: %6;
}

/* === Выпадающие списки (QComboBox) === */
QComboBox {
    background-color: %8;
    color: %1;
    border: 1px solid %6;
    border-radius: 3px;
    padding: 3px 8px;
    min-height: 18px;
}

QComboBox:hover {
    background-color: %9;
    border-color: %10;
}

QComboBox:focus {
    border-color: %2;
}

QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 16px;
    border-left: none;
}

QComboBox QAbstractItemView {
    background-color: %7;
    color: %1;
    border: 1px solid %6;
    selection-background-color: %2;
    selection-color: #FFFFFF;
}

/* === Деревья и списки (QTreeView, QTreeWidget, QListView) === */
QTreeView, QTreeWidget, QListView, QTableView {
    background-color: %5;
    color: %1;
    border: 1px solid %6;
    outline: none;
}

QTreeView::item, QTreeWidget::item, QListView::item {
    padding: 2px 4px;
    border-radius: 2px;
}

QTreeView::item:hover, QTreeWidget::item:hover, QListView::item:hover {
    background-color: %9;
}

QTreeView::item:selected, QTreeWidget::item:selected, QListView::item:selected {
    background-color: %2;
    color: #FFFFFF;
}

QHeaderView::section {
    background-color: %5;
    color: %4;
    border: none;
    border-right: 1px solid %6;
    border-bottom: 1px solid %6;
    padding: 3px 6px;
    font-weight: 600;
}

/* === Вкладки (QTabBar, QTabWidget) === */
QTabWidget::pane {
    border: 1px solid %6;
    top: -1px;
}

QTabBar::tab {
    background-color: %5;
    color: %4;
    padding: 4px 10px;
    border: 1px solid %6;
    border-bottom: none;
    margin-right: 2px;
    border-top-left-radius: 3px;
    border-top-right-radius: 3px;
}

QTabBar::tab:hover {
    background-color: %8;
    color: %1;
}

QTabBar::tab:selected {
    background-color: %3;
    color: %1;
    border-bottom: 2px solid %2;
    font-weight: 600;
}

/* === Скроллбары (QScrollBar) === */
QScrollBar:vertical {
    background-color: %5;
    width: 8px;
    margin: 0;
    border: none;
}

QScrollBar::handle:vertical {
    background-color: %10;
    min-height: 20px;
    border-radius: 4px;
    margin: 1px;
}

QScrollBar::handle:vertical:hover {
    background-color: %2;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
    height: 0;
}

QScrollBar:horizontal {
    background-color: %5;
    height: 8px;
    margin: 0;
    border: none;
}

QScrollBar::handle:horizontal {
    background-color: %10;
    min-width: 20px;
    border-radius: 4px;
    margin: 1px;
}

QScrollBar::handle:horizontal:hover {
    background-color: %2;
}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: transparent;
    width: 0;
}

/* === Группы (QGroupBox) === */
QGroupBox {
    font-weight: 600;
    color: %4;
    border: 1px solid %6;
    border-radius: 4px;
    margin-top: 10px;
    padding-top: 8px;
    padding-left: 4px;
    padding-right: 4px;
    padding-bottom: 4px;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 8px;
    padding: 0 4px;
    background-color: %3;
}

/* === Чекбоксы (QCheckBox) === */
QCheckBox {
    spacing: 6px;
    color: %1;
}

QCheckBox::indicator {
    width: 14px;
    height: 14px;
    border: 1px solid %10;
    border-radius: 3px;
    background-color: %5;
}

QCheckBox::indicator:hover {
    border-color: %2;
}

QCheckBox::indicator:checked {
    background-color: %2;
    border-color: %2;
}

/* === Статусбар === */
QStatusBar {
    background-color: %5;
    border-top: 1px solid %6;
    color: %4;
}

QStatusBar QLabel {
    color: %4;
    padding: 0 4px;
}

/* === Нативные Direct3D виджеты (не заливать фоном Qt) === */
#MaterialPreview, ViewportWidget, MaterialPreviewWidget {
    background: transparent;
    border: none;
}
)QSS")
    .arg(textPrimary)     // %1
    .arg(accent)          // %2
    .arg(bgWindow)        // %3
    .arg(textSecondary)   // %4
    .arg(bgDarkest)       // %5
    .arg(borderLight)     // %6
    .arg(bgPanel)         // %7
    .arg(bgButton)        // %8
    .arg(bgButtonHover)   // %9
    .arg(borderMid)       // %10
    .arg(bgButtonPressed) // %11
    .arg(textDisabled)    // %12
    .arg(accentHover);    // %13
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
        p.drawRoundedRect(QRectF(3.5, 3.0, 3.0, 10.0), 0.8, 0.8);
        p.drawRoundedRect(QRectF(9.5, 3.0, 3.0, 10.0), 0.8, 0.8);
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
        p.drawRoundedRect(QRectF(3.5, 3.5, 9.0, 9.0), 1.0, 1.0);
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
        QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(2.5, 5.0, 8.5, 8.5), 1.0, 1.0);
        p.drawLine(QPointF(7.5, 8.5), QPointF(13.5, 2.5));
        p.drawLine(QPointF(10.0, 2.5), QPointF(13.5, 2.5));
        p.drawLine(QPointF(13.5, 2.5), QPointF(13.5, 6.0));
    }
    return QIcon(pix);
}
