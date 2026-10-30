#include "EditorStyles.h"
#include "EditorTheme.h"

QString EditorStyles::BuildStyleSheet(ThemeMode mode, const QColor& accentColor)
{
    const bool isDark = (mode == ThemeMode::Dark);
    const QString accent = accentColor.name(QColor::HexRgb).toUpper();
    const QString accentHover = accentColor.lighter(120).name(QColor::HexRgb).toUpper();

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
QWidget {
    color: %1;
    selection-background-color: %2;
    selection-color: #FFFFFF;
    outline: none;
}
QMainWindow, QDialog {
    background-color: %3;
}
QMenuBar {
    background-color: %3;
    border-bottom: 1px solid %6;
    padding: 2px 6px;
    font-size: 12px;
}
QMenuBar::item {
    background: transparent;
    padding: 4px 8px;
    border-radius: 4px;
    color: %4;
}
QMenuBar::item:selected {
    background-color: %9;
    color: %1;
}
QMenu {
    background-color: %7;
    border: 1px solid %6;
    border-radius: 4px;
    padding: 4px;
}
QMenu::item {
    padding: 5px 24px 5px 12px;
    border-radius: 3px;
    color: %1;
}
QMenu::item:selected {
    background-color: %2;
    color: #FFFFFF;
}
QMenu::separator {
    height: 1px;
    background-color: %6;
    margin: 4px 6px;
}
QToolBar {
    background-color: %3;
    border-bottom: 1px solid %6;
    spacing: 4px;
    padding: 3px 6px;
}
QToolButton {
    background-color: %8;
    border: 1px solid %6;
    border-radius: 4px;
    padding: 4px 8px;
    color: %1;
}
QToolButton:hover {
    background-color: %9;
    border-color: %10;
}
QToolButton:pressed, QToolButton:checked {
    background-color: %11;
    border-color: %2;
}
QPushButton {
    background-color: %8;
    border: 1px solid %6;
    border-radius: 4px;
    padding: 5px 12px;
    font-weight: 500;
}
QPushButton:hover {
    background-color: %9;
    border-color: %10;
}
QPushButton:pressed {
    background-color: %11;
    border-color: %2;
}
QPushButton:disabled {
    color: %12;
    border-color: %6;
}
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background-color: %5;
    border: 1px solid %6;
    border-radius: 4px;
    padding: 3px 6px;
    color: %1;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border-color: %2;
}
QTreeView, QTreeWidget, QListView, QListWidget, QPlainTextEdit, QTextEdit {
    background-color: %5;
    border: 1px solid %6;
    border-radius: 4px;
    color: %1;
    alternate-background-color: %7;
}
QTreeView::item:hover, QListView::item:hover {
    background-color: %9;
}
QTreeView::item:selected, QListView::item:selected {
    background-color: %2;
    color: #FFFFFF;
}
QHeaderView::section {
    background-color: %7;
    color: %4;
    border: none;
    border-right: 1px solid %6;
    border-bottom: 1px solid %6;
    padding: 4px 8px;
    font-weight: 600;
}
QDockWidget {
    titlebar-close-icon: none;
    titlebar-normal-icon: none;
    font-weight: 600;
}
QDockWidget::title {
    background-color: %7;
    border-bottom: 1px solid %6;
    padding: 6px 8px;
    text-align: left;
}
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
QStatusBar {
    background-color: %5;
    border-top: 1px solid %6;
    color: %4;
}
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
