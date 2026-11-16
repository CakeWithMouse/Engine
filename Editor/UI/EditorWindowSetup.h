#pragma once

#include "EditorLocale.h"

#include <QApplication>
#include <QEvent>
#include <QMainWindow>
#include <QObject>
#include <QPointer>
#include <QStatusBar>
#include <QWidget>

namespace EditorWindowSetup
{

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
                QWidget* host = mainWindow_ ? mainWindow_.data() : window_->parentWidget();
                if (auto* main = qobject_cast<QMainWindow*>(host))
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

inline void ConfigureSubWindow(QWidget* window, QWidget* mainWindow, const QString& title = QString())
{
    if (!window)
        return;

    Qt::WindowFlags flags = window->windowFlags();
    flags &= ~Qt::WindowType_Mask;
    flags |= Qt::Window;
    window->setParent(mainWindow, flags);

    if (!title.isEmpty())
        window->setWindowTitle(title);

    if (mainWindow && !mainWindow->windowIcon().isNull())
        window->setWindowIcon(mainWindow->windowIcon());
    else if (!QApplication::windowIcon().isNull())
        window->setWindowIcon(QApplication::windowIcon());

    window->installEventFilter(new MinimizeWatcher(window, mainWindow));
}

} // namespace EditorWindowSetup
