#include "DockLayoutManager.h"
#include "SettingsService.h"
#include "Logger.h"
#include <QAbstractScrollArea>
#include <QTabBar>
#include <QApplication>
#include <QMouseEvent>
#include <QTimer>
#include <QList>
#include <QLayout>

void DockLayoutManager::UnlockDockResizing(QDockWidget* dock)
{
    if (!dock) return;
    dock->setMinimumSize(40, 40);
    dock->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    if (dock->widget()) {
        UnlockWidgetResizing(dock->widget());
    }
}

void DockLayoutManager::UnlockWidgetResizing(QWidget* widget)
{
    if (!widget) return;
    if (widget->layout()) widget->layout()->setSizeConstraint(QLayout::SetNoConstraint);
    widget->setMinimumSize(40, 30);
    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    for (auto* child : widget->findChildren<QAbstractScrollArea*>()) {
        child->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
        child->setMinimumSize(30, 20);
        child->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }
}

void DockLayoutManager::SaveLayout(QMainWindow* window, const QString& projectPath, bool isTestMode)
{
    if (!window || isTestMode) return;
    SettingsService::Instance().SaveProjectLayout(projectPath, window->saveState(), window->saveGeometry());
}

bool DockLayoutManager::RestoreLayout(QMainWindow* window, const QString& projectPath)
{
    if (!window) return false;
    QByteArray state, geom;
    if (SettingsService::Instance().RestoreProjectLayout(projectPath, state, geom)) {
        if (window->restoreState(state)) {
            if (!geom.isEmpty()) {
                window->restoreGeometry(geom);
            }
            return true;
        }
    }
    return false;
}

void DockLayoutManager::ResetLayout(QMainWindow* window, const QString& projectPath)
{
    if (!window) return;
    SettingsService::Instance().ResetProjectLayout(projectPath);
}

void DockLayoutManager::ApplyDefaultLayout(QMainWindow* window,
                                          QDockWidget* viewportDock,
                                          QDockWidget* sceneObjectsDock,
                                          QDockWidget* inspectorDock,
                                          QDockWidget* fileDock,
                                          QDockWidget* consoleDock)
{
    if (!window) return;

    for (auto* dock : {viewportDock, sceneObjectsDock, inspectorDock, fileDock, consoleDock}) {
        if (dock) {
            dock->setFloating(false);
            dock->show();
            UnlockDockResizing(dock);
        }
    }

    if (sceneObjectsDock) window->addDockWidget(Qt::TopDockWidgetArea, sceneObjectsDock);
    if (viewportDock) {
        if (sceneObjectsDock) {
            window->splitDockWidget(sceneObjectsDock, viewportDock, Qt::Horizontal);
        } else {
            window->addDockWidget(Qt::TopDockWidgetArea, viewportDock);
        }
    }
    if (inspectorDock) {
        if (viewportDock) {
            window->splitDockWidget(viewportDock, inspectorDock, Qt::Horizontal);
        } else {
            window->addDockWidget(Qt::TopDockWidgetArea, inspectorDock);
        }
    }
    if (consoleDock) window->addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
    if (fileDock) window->addDockWidget(Qt::BottomDockWidgetArea, fileDock);

    if (fileDock && consoleDock) {
        window->tabifyDockWidget(consoleDock, fileDock);
        fileDock->raise();
    }

    if (viewportDock && (fileDock || consoleDock)) {
        window->resizeDocks({viewportDock, fileDock ? fileDock : consoleDock}, {500, 180}, Qt::Vertical);
    }
    if (sceneObjectsDock && viewportDock && inspectorDock) {
        window->resizeDocks({sceneObjectsDock, viewportDock, inspectorDock}, {240, 720, 320}, Qt::Horizontal);
    }
}

void DockLayoutManager::ConfigureTabBars(QMainWindow* window)
{
    if (!window) return;
    for (auto* bar : window->findChildren<QTabBar*>()) {
        bar->setElideMode(Qt::ElideNone);
        bar->setUsesScrollButtons(false);
        bar->setExpanding(false);
        bar->setFocusPolicy(Qt::NoFocus);
    }

    QTimer::singleShot(400, window, [window] {
        Logger::Instance().Info(QStringLiteral("=== TAB DIAGNOSTIC START ==="));
        for (auto* bar : window->findChildren<QTabBar*>()) {
            if (!bar->isVisible() || bar->count() < 2) continue;
            Logger::Instance().Info(QStringLiteral("[TabDiag] Bar: parent=%1 name='%2' geom=(%3,%4 %5x%6) vis=%7 count=%8 cur=%9")
                .arg(bar->parent() ? bar->parent()->metaObject()->className() : "null")
                .arg(bar->objectName())
                .arg(bar->x()).arg(bar->y()).arg(bar->width()).arg(bar->height())
                .arg(bar->isVisible() ? 1 : 0).arg(bar->count()).arg(bar->currentIndex()));
            for (int t = 0; t < bar->count(); ++t) {
                QRect r = bar->tabRect(t);
                QPoint center = r.center();
                QPoint gCenter = bar->mapToGlobal(center);
                QWidget* wAtCenter = QApplication::widgetAt(gCenter);
                Logger::Instance().Info(QStringLiteral("[TabDiag] tab %1 '%2': rect=(%3,%4 %5x%6) tabAtCenter=%7 wAtCenter=%8 en=%9")
                    .arg(t).arg(bar->tabText(t))
                    .arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())
                    .arg(bar->tabAt(center))
                    .arg(wAtCenter ? wAtCenter->metaObject()->className() : "null")
                    .arg(bar->isTabEnabled(t) ? 1 : 0));
            }
            // Execute synthetic clicks to test both tabs
            for (int cycle = 0; cycle < 3; ++cycle) {
                QPoint c0 = bar->tabRect(0).center();
                QWidget* target0 = QApplication::widgetAt(bar->mapToGlobal(c0));
                if (target0) {
                    QPoint localPt = target0->mapFromGlobal(bar->mapToGlobal(c0));
                    QMouseEvent press(QEvent::MouseButtonPress, localPt, bar->mapToGlobal(c0), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(target0, &press);
                    QMouseEvent release(QEvent::MouseButtonRelease, localPt, bar->mapToGlobal(c0), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(target0, &release);
                }
                Logger::Instance().Info(QStringLiteral("[TabDiag] Cycle %1: click tab 0 -> tabAt=%2 cur=%3")
                    .arg(cycle).arg(bar->tabAt(c0)).arg(bar->currentIndex()));

                QPoint c1 = bar->tabRect(1).center();
                QWidget* target1 = QApplication::widgetAt(bar->mapToGlobal(c1));
                if (target1) {
                    QPoint localPt = target1->mapFromGlobal(bar->mapToGlobal(c1));
                    QMouseEvent press(QEvent::MouseButtonPress, localPt, bar->mapToGlobal(c1), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(target1, &press);
                    QMouseEvent release(QEvent::MouseButtonRelease, localPt, bar->mapToGlobal(c1), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(target1, &release);
                }
                Logger::Instance().Info(QStringLiteral("[TabDiag] Cycle %1: click tab 1 -> tabAt=%2 cur=%3")
                    .arg(cycle).arg(bar->tabAt(c1)).arg(bar->currentIndex()));
            }
        }
        Logger::Instance().Info(QStringLiteral("=== TAB DIAGNOSTIC END ==="));
    });
}
