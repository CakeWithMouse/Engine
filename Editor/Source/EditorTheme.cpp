#include "EditorTheme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QFont>

void EditorTheme::Apply(QApplication& app)
{
    // 1. Установка базового движка стилей Fusion
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // 2. Читаемый системный шрифт
    QFont defaultFont(QStringLiteral("Segoe UI"), 9);
    defaultFont.setStyleHint(QFont::SansSerif);
    app.setFont(defaultFont);

    // 3. Палитра цветов (Dark Blender 4.x / UE5)
    QPalette palette;
    const QColor bgDarkest(0x14, 0x16, 0x1A);    // #14161A (поля ввода, списки, консоль)
    const QColor bgDark(0x18, 0x1A, 0x1F);       // #181A1F (окно, доки, тулбар)
    const QColor bgPanel(0x20, 0x23, 0x2A);      // #20232A (контент панелей)
    const QColor bgButton(0x21, 0x25, 0x2B);     // #21252B (кнопки, селекторы)
    const QColor textPrimary(0xEC, 0xEF, 0xF4);  // #ECEFF4 (основной текст)
    const QColor textSecondary(0xAB, 0xB2, 0xBF);// #ABB2BF (второстепенный текст)
    const QColor textDisabled(0x5C, 0x63, 0x70); // #5C6370 (неактивный текст)
    const QColor accentBlue(0x3B, 0x82, 0xF6);   // #3B82F6 (акцент выделения)
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
    palette.setColor(QPalette::Highlight, accentBlue);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, QColor(0x4C, 0x8B, 0xF5));
    palette.setColor(QPalette::Dark, QColor(0x10, 0x12, 0x15));
    palette.setColor(QPalette::Mid, QColor(0x1C, 0x1F, 0x26));
    palette.setColor(QPalette::Midlight, borderLight);
    palette.setColor(QPalette::Shadow, QColor(0x0D, 0x0E, 0x11));

    // Неактивное состояние
    palette.setColor(QPalette::Disabled, QPalette::Text, textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x24, 0x2A, 0x36));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, textDisabled);

    app.setPalette(palette);

    // 4. Глобальный QSS (Таблица стилей)
    app.setStyleSheet(QString::fromUtf8(StyleRules()));
}

const char* EditorTheme::StyleRules()
{
    return R"QSS(
/* === Общие базовые стили === */
QWidget {
    color: #ECEFF4;
    background-color: transparent;
    selection-background-color: #3B82F6;
    selection-color: #FFFFFF;
    outline: none;
}

QMainWindow {
    background-color: #181A1F;
}

/* === Заголовки и стилизация QDockWidget === */
QDockWidget {
    color: #ABB2BF;
    font-weight: 600;
    font-size: 8.5pt;
    titlebar-close-icon: none;
    titlebar-normal-icon: none;
}

QDockWidget::title {
    background-color: #14161A;
    text-align: left;
    padding: 3px 8px;
    border-bottom: 1px solid #2C313A;
}

QDockWidget::title:hover {
    background-color: #1C2028;
    color: #ECEFF4;
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
    background-color: #2D333F;
    border-radius: 2px;
}

/* === Сплиттеры и разделители === */
QMainWindow::separator {
    background-color: #14161A;
    width: 3px;
    height: 3px;
    border: none;
}

QMainWindow::separator:hover {
    background-color: #3B82F6;
}

QSplitter::handle {
    background-color: #14161A;
}

QSplitter::handle:hover {
    background-color: #3B82F6;
}

/* === Минималистичные скроллбары (Blender / UE5) === */
QScrollBar:vertical {
    background: transparent;
    width: 8px;
    margin: 0px;
}

QScrollBar::handle:vertical {
    background: #343A46;
    min-height: 20px;
    border-radius: 4px;
}

QScrollBar::handle:vertical:hover {
    background: #4B5364;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: none;
    height: 0px;
}

QScrollBar:horizontal {
    background: transparent;
    height: 8px;
    margin: 0px;
}

QScrollBar::handle:horizontal {
    background: #343A46;
    min-width: 20px;
    border-radius: 4px;
}

QScrollBar::handle:horizontal:hover {
    background: #4B5364;
}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: none;
    width: 0px;
}

/* === Меню и Тулбар === */
QMenuBar {
    background-color: #14161A;
    border-bottom: 1px solid #2C313A;
    padding: 2px 4px;
    color: #ABB2BF;
}

QMenuBar::item {
    background: transparent;
    padding: 4px 8px;
    border-radius: 3px;
}

QMenuBar::item:selected {
    background-color: #2D333F;
    color: #ECEFF4;
}

QMenu {
    background-color: #181A1F;
    border: 1px solid #2C313A;
    padding: 4px;
    border-radius: 4px;
}

QMenu::item {
    padding: 5px 24px 5px 12px;
    border-radius: 3px;
    color: #ECEFF4;
}

QMenu::item:selected {
    background-color: #3B82F6;
    color: #FFFFFF;
}

QMenu::item:disabled {
    color: #5C6370;
}

QMenu::separator {
    height: 1px;
    background-color: #2C313A;
    margin: 4px 2px;
}

QToolBar {
    background-color: #181A1F;
    border-bottom: 1px solid #2C313A;
    spacing: 4px;
    padding: 3px 6px;
}

QToolBar::separator {
    width: 1px;
    background-color: #2C313A;
    margin: 4px 3px;
}

/* === Кнопки === */
QPushButton, QToolButton {
    background-color: #21252B;
    border: 1px solid #2C313A;
    border-radius: 4px;
    padding: 3px 8px;
    color: #ECEFF4;
    font-size: 9pt;
}

QPushButton:hover, QToolButton:hover {
    background-color: #2D333F;
    border-color: #3B82F6;
}

QPushButton:pressed, QToolButton:pressed {
    background-color: #38404F;
    border-color: #3B82F6;
}

QPushButton:disabled, QToolButton:disabled {
    background-color: #181A1F;
    border-color: #242830;
    color: #5C6370;
}

/* === Поля ввода и текст === */
QLineEdit, QTextEdit, QPlainTextEdit {
    background-color: #14161A;
    border: 1px solid #2C313A;
    border-radius: 4px;
    padding: 3px 6px;
    color: #ECEFF4;
    selection-background-color: #3B82F6;
}

QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border: 1px solid #3B82F6;
}

/* === Выпадающие списки (QComboBox) === */
QComboBox {
    background-color: #21252B;
    border: 1px solid #2C313A;
    border-radius: 4px;
    padding: 3px 8px;
    color: #ECEFF4;
    min-height: 18px;
}

QComboBox:hover {
    background-color: #2D333F;
    border-color: #3B82F6;
}

QComboBox:focus {
    border-color: #3B82F6;
}

QComboBox::drop-down {
    border: none;
    width: 18px;
    subcontrol-origin: padding;
    subcontrol-position: top right;
}

QComboBox QAbstractItemView {
    background-color: #181A1F;
    border: 1px solid #2C313A;
    border-radius: 4px;
    padding: 2px;
    selection-background-color: #3B82F6;
}

/* === Спинбоксы (QDoubleSpinBox, QSpinBox) === */
QSpinBox, QDoubleSpinBox {
    background-color: #14161A;
    border: 1px solid #2C313A;
    border-radius: 4px;
    padding: 3px 6px;
    color: #ECEFF4;
}

QSpinBox:focus, QDoubleSpinBox:focus {
    border: 1px solid #3B82F6;
}

QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
    background-color: #21252B;
    border: none;
    width: 14px;
}

QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
    background-color: #2D333F;
}

/* === Деревья и списки (QTreeView, QTreeWidget, QListWidget) === */
QTreeView, QTreeWidget, QListView, QListWidget {
    background-color: #14161A;
    alternate-background-color: #181B20;
    border: 1px solid #2C313A;
    border-radius: 4px;
    color: #ECEFF4;
}

QTreeView::item, QTreeWidget::item, QListView::item, QListWidget::item {
    padding: 2px 4px;
    border-radius: 2px;
}

QTreeView::item:hover, QTreeWidget::item:hover, QListView::item:hover, QListWidget::item:hover {
    background-color: #20242C;
}

QTreeView::item:selected, QTreeWidget::item:selected, QListView::item:selected, QListWidget::item:selected {
    background-color: #2B5B9E;
    color: #FFFFFF;
}

QTreeView::item:selected:active, QTreeWidget::item:selected:active, QListView::item:selected:active, QListWidget::item:selected:active {
    background-color: #3B82F6;
    color: #FFFFFF;
}

QHeaderView::section {
    background-color: #181A1F;
    color: #828997;
    border: none;
    border-bottom: 1px solid #2C313A;
    border-right: 1px solid #20232A;
    padding: 4px 6px;
    font-weight: 600;
    font-size: 8.5pt;
}

/* === Вкладки (QTabBar, QTabWidget) в стиле Blender === */
QTabBar::tab {
    background-color: #181A1F;
    border: 1px solid #2C313A;
    border-bottom: none;
    padding: 5px 12px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    color: #828997;
    margin-right: 2px;
}

QTabBar::tab:hover {
    background-color: #20232A;
    color: #ABB2BF;
}

QTabBar::tab:selected {
    background-color: #20232A;
    color: #ECEFF4;
    border-top: 2px solid #E07A1F; /* Оранжевый акцент вкладки Blender */
}

QTabWidget::pane {
    border: 1px solid #2C313A;
    background-color: #20232A;
    border-radius: 4px;
}

/* === Группы (QGroupBox) === */
QGroupBox {
    font-weight: 600;
    color: #ABB2BF;
    border: 1px solid #2C313A;
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
    background-color: #181A1F;
}

/* === Чекбоксы (QCheckBox) === */
QCheckBox {
    spacing: 6px;
    color: #ECEFF4;
}

QCheckBox::indicator {
    width: 14px;
    height: 14px;
    border: 1px solid #3E4451;
    border-radius: 3px;
    background-color: #14161A;
}

QCheckBox::indicator:hover {
    border-color: #3B82F6;
}

QCheckBox::indicator:checked {
    background-color: #3B82F6;
    border-color: #3B82F6;
}

/* === Статусбар === */
QStatusBar {
    background-color: #14161A;
    border-top: 1px solid #2C313A;
    color: #828997;
}

QStatusBar QLabel {
    color: #828997;
    padding: 0 4px;
}
)QSS";
}
