#include "DockLayoutManager.h"
#include "SettingsService.h"
#include <QAbstractScrollArea>
#include <QList>

void DockLayoutManager::UnlockDockResizing(QDockWidget* dock)
{
    if (!dock) return;
    dock->setMinimumSize(80, 60);
    dock->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    if (dock->widget()) {
        UnlockWidgetResizing(dock->widget());
    }
}

void DockLayoutManager::UnlockWidgetResizing(QWidget* widget)
{
    if (!widget) return;
    widget->setMinimumSize(80, 60);
    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    for (auto* child : widget->findChildren<QAbstractScrollArea*>()) {
        child->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
        child->setMinimumSize(60, 50);
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

    if (sceneObjectsDock) window->addDockWidget(Qt::LeftDockWidgetArea, sceneObjectsDock);
    if (viewportDock) {
        if (sceneObjectsDock) {
            window->splitDockWidget(sceneObjectsDock, viewportDock, Qt::Horizontal);
        } else {
            window->addDockWidget(Qt::LeftDockWidgetArea, viewportDock);
        }
    }
    if (inspectorDock) {
        if (viewportDock) {
            window->splitDockWidget(viewportDock, inspectorDock, Qt::Horizontal);
        } else {
            window->addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
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
