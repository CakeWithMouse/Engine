#include "DockLayoutManager.h"
#include <QSettings>

void DockLayoutManager::UnlockDockResizing(QDockWidget* dock)
{
    if (!dock) return;
    dock->setMinimumSize(80, 60);
    if (dock->widget()) {
        UnlockWidgetResizing(dock->widget());
    }
}

void DockLayoutManager::UnlockWidgetResizing(QWidget* widget)
{
    if (!widget) return;
    widget->setMinimumSize(80, 60);
    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

void DockLayoutManager::SaveLayout(QMainWindow* window, bool isTestMode)
{
    if (!window || isTestMode) return;
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    settings.setValue(QStringLiteral("window/geometry"), window->saveGeometry());
    settings.setValue(QStringLiteral("window/state"), window->saveState());
}

bool DockLayoutManager::RestoreLayout(QMainWindow* window)
{
    if (!window) return false;
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    const QByteArray savedState = settings.value(QStringLiteral("window/state")).toByteArray();
    const QByteArray savedGeom = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!savedState.isEmpty() && window->restoreState(savedState)) {
        if (!savedGeom.isEmpty()) {
            window->restoreGeometry(savedGeom);
        }
        return true;
    }
    return false;
}

void DockLayoutManager::ResetLayout(QMainWindow* window)
{
    if (!window) return;
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    settings.remove(QStringLiteral("window/state"));
    settings.remove(QStringLiteral("window/geometry"));
}

void DockLayoutManager::ApplyDefaultDocks(QMainWindow* window,
                                         QDockWidget* viewportDock,
                                         QDockWidget* sceneObjectsDock,
                                         QDockWidget* inspectorDock,
                                         QDockWidget* fileDock,
                                         QDockWidget* consoleDock)
{
    if (!window) return;
    if (viewportDock) window->addDockWidget(Qt::TopDockWidgetArea, viewportDock);
    if (sceneObjectsDock) window->addDockWidget(Qt::LeftDockWidgetArea, sceneObjectsDock);
    if (inspectorDock) window->addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    if (fileDock) window->addDockWidget(Qt::BottomDockWidgetArea, fileDock);
    if (consoleDock) window->addDockWidget(Qt::BottomDockWidgetArea, consoleDock);

    if (fileDock && consoleDock) {
        window->tabifyDockWidget(fileDock, consoleDock);
        fileDock->raise();
    }
}
