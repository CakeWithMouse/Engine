#pragma once

#include "EditorLocale.h"

#include <QApplication>
#include <QEvent>
#include <QLabel>
#include <QMainWindow>
#include <QObject>
#include <QPointer>
#include <QStatusBar>
#include <QWidget>

namespace EditorWindowSetup
{

// Стандартные размеры кнопок тулбара окна и панельных шапок (единый размер 34x26 для верхнего тулбара, 30x24 для панелей)
constexpr int kToolbarBtnWidth = 34;
constexpr int kToolbarBtnHeight = 26;
constexpr int kToolbarIconSize = 16;

constexpr int kPanelBtnWidth = 30;
constexpr int kPanelBtnHeight = 24;
constexpr int kPanelIconSize = 16;

class MinimizeWatcher : public QObject
{
public:
    MinimizeWatcher(QWidget* window, QWidget* mainWindow)
        : QObject(window), window_(window), mainWindow_(mainWindow)
    {
        wasMinimized_ = window_ ? window_->isMinimized() : false;
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (obj == window_ && event->type() == QEvent::WindowStateChange)
        {
            const bool nowMin = window_->isMinimized();
            if (nowMin && !wasMinimized_)
            {
                if (auto* main = qobject_cast<QMainWindow*>(mainWindow_.data()))
                {
                    if (auto* sb = main->statusBar())
                    {
                        const QString name = !window_->windowTitle().isEmpty()
                            ? window_->windowTitle()
                            : window_->objectName();
                        const QString msg = Tr(
                            "Окно «%1» свёрнуто, его можно вернуть из панели задач",
                            "Window '%1' minimized, restore it from taskbar"
                        ).arg(name);
                        sb->showMessage(msg, 5000);
                    }
                }
            }
            wasMinimized_ = nowMin;
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QWidget* window_{nullptr};
    QPointer<QWidget> mainWindow_{nullptr};
    bool wasMinimized_{false};
};

class MainWindowCloseWatcher : public QObject
{
public:
    MainWindowCloseWatcher(QWidget* child, QObject* parent)
        : QObject(parent), child_(child) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() == QEvent::Close)
        {
            if (child_)
                child_->close();
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QPointer<QWidget> child_;
};

// Подписанный заголовок боковой/нижней панели: единый стиль во всех редакторах (QLabel#PanelHeader).
inline QLabel* MakePanelHeader(const QString& text, QWidget* parent)
{
    auto* header = new QLabel(text, parent);
    header->setObjectName(QStringLiteral("PanelHeader"));
    return header;
}

inline void ConfigureSubWindow(QWidget* window, QWidget* mainWindow, const QString& title = QString())
{
    if (!window)
        return;

    // Window has parent = nullptr so Windows creates a normal taskbar button when minimized
    window->setParent(nullptr);

    Qt::WindowFlags flags = window->windowFlags();
    flags &= ~Qt::WindowType_Mask;
    flags |= Qt::Window;
    flags &= ~Qt::Tool;
    window->setWindowFlags(flags);

    if (!title.isEmpty())
        window->setWindowTitle(title);

    if (mainWindow && !mainWindow->windowIcon().isNull())
        window->setWindowIcon(mainWindow->windowIcon());
    else if (!QApplication::windowIcon().isNull())
        window->setWindowIcon(QApplication::windowIcon());

    QPointer<QWidget> winPtr(window);
    if (mainWindow)
    {
        QObject::connect(mainWindow, &QObject::destroyed, window, [winPtr]() {
            if (winPtr)
                winPtr->close();
        });
        mainWindow->installEventFilter(new MainWindowCloseWatcher(window, mainWindow));
    }
    if (qApp)
    {
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, window, [winPtr]() {
            if (winPtr)
                winPtr->close();
        });
    }

    window->installEventFilter(new MinimizeWatcher(window, mainWindow));
}

} // namespace EditorWindowSetup
