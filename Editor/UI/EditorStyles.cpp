#include "EditorStyles.h"
#include "EditorTheme.h"

QString EditorStyles::BuildStyleSheet(ThemeMode mode, const QColor& accentColor,
                                       const QString& arrowUp,
                                       const QString& arrowDown,
                                       const QString& arrowRight)
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

    QString qss = QString(R"QSS(
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
    color: %1;
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
    font-size: 11px;
    font-weight: 400;
}
QDockWidget::title {
    background-color: %7;
    border-bottom: 1px solid %6;
    padding: 2px 4px;
    height: 20px;
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

/* Collapsible cards and inspector tokens */
QFrame#CollapsibleCard {
    background-color: %7;
    border: 1px solid %6;
    border-radius: 4px;
    margin-bottom: 4px;
}
QPushButton#CardHeaderBtn {
    background-color: %8;
    color: %1;
    font-weight: 600;
    font-size: 11px;
    text-align: left;
    height: 22px;
    min-height: 22px;
    max-height: 22px;
    padding: 0px 8px;
    border: none;
    border-top-left-radius: 3px;
    border-top-right-radius: 3px;
}
QPushButton#CardHeaderBtn:hover {
    background-color: %9;
    color: %1;
}
QLabel#CardSectionLabel {
    color: %4;
    font-size: 10px;
    font-weight: 500;
}
QLabel#CardFieldLabel {
    color: %4;
    font-size: 11px;
    font-weight: normal;
}
QFrame#CollapsibleCard QLineEdit,
QFrame#CollapsibleCard QSpinBox,
QFrame#CollapsibleCard QDoubleSpinBox,
QFrame#CollapsibleCard QComboBox {
    min-height: 22px;
    max-height: 22px;
    padding: 1px 4px;
    font-size: 11px;
    color: %1;
}
QFrame#CollapsibleCard QPushButton:not(#CardHeaderBtn) {
    min-height: 22px;
    max-height: 22px;
    padding: 1px 6px;
    font-size: 11px;
}
QLabel#SelectedObjectLabel {
    color: %2;
    font-weight: bold;
    font-size: 11px;
}
QLabel#TypeBadgeLabel {
    background-color: %8;
    color: %4;
    font-weight: bold;
    font-size: 10px;
    padding: 2px 6px;
    border-radius: 4px;
    border: 1px solid %6;
}
QLabel#NoSelectionLabel {
    color: %12;
    font-size: 13px;
    font-weight: 500;
    padding: 40px 10px;
}
QScrollArea#InspectorScrollArea, QWidget#InspectorScrollContent {
    background-color: %3;
    border: none;
}
QPushButton#PrimaryActionButton {
    background-color: %2;
    color: #FFFFFF;
    font-weight: bold;
    border: 1px solid %2;
    border-radius: 4px;
    padding: 3px 10px;
    font-size: 11px;
}
QPushButton#PrimaryActionButton:hover {
    background-color: %13;
}
QPushButton#PrimaryActionButton:disabled {
    background-color: %8;
    color: %12;
    border: 1px solid %6;
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

    if (!arrowUp.isEmpty() && !arrowDown.isEmpty()) {
        qss += QString(R"QSS(
QSpinBox::up-button, QDoubleSpinBox::up-button {
    subcontrol-origin: border;
    subcontrol-position: top right;
    width: 16px;
    border-left: 1px solid %1;
    border-bottom: 1px solid %1;
    background-color: %2;
}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover {
    background-color: %3;
}
QSpinBox::down-button, QDoubleSpinBox::down-button {
    subcontrol-origin: border;
    subcontrol-position: bottom right;
    width: 16px;
    border-left: 1px solid %1;
    background-color: %2;
}
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
    background-color: %3;
}
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
    image: url("%4");
    width: 8px;
    height: 8px;
}
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
    image: url("%5");
    width: 8px;
    height: 8px;
}
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 20px;
    border-left: 1px solid %1;
    background-color: %2;
    border-top-right-radius: 4px;
    border-bottom-right-radius: 4px;
}
QComboBox::drop-down:hover {
    background-color: %3;
}
QComboBox::down-arrow {
    image: url("%5");
    width: 8px;
    height: 8px;
}
)QSS").arg(borderLight, bgButton, bgButtonHover, arrowUp, arrowDown);
    }

    if (!arrowRight.isEmpty() && !arrowDown.isEmpty()) {
        qss += QString(R"QSS(
QTreeView::branch:has-children:!has-siblings:closed,
QTreeView::branch:closed:has-children:has-siblings {
    image: url("%1");
}
QTreeView::branch:open:has-children:!has-siblings,
QTreeView::branch:open:has-children:has-siblings {
    image: url("%2");
}
)QSS").arg(arrowRight, arrowDown);
    }

    return qss;
}
