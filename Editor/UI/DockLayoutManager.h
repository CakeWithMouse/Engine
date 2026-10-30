#pragma once

#include <QMainWindow>
#include <QDockWidget>

class DockLayoutManager
{
public:
    static void UnlockDockResizing(QDockWidget* dock);
    static void UnlockWidgetResizing(QWidget* widget);
    static void SaveLayout(QMainWindow* window, bool isTestMode = false);
    static bool RestoreLayout(QMainWindow* window);
    static void ResetLayout(QMainWindow* window);
    static void ApplyDefaultDocks(QMainWindow* window,
                                  QDockWidget* viewportDock,
                                  QDockWidget* sceneObjectsDock,
                                  QDockWidget* inspectorDock,
                                  QDockWidget* fileDock,
                                  QDockWidget* consoleDock);
};
