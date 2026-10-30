#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include <QString>

class DockLayoutManager
{
public:
    static void UnlockDockResizing(QDockWidget* dock);
    static void UnlockWidgetResizing(QWidget* widget);
    static void SaveLayout(QMainWindow* window, const QString& projectPath, bool isTestMode = false);
    static bool RestoreLayout(QMainWindow* window, const QString& projectPath);
    static void ResetLayout(QMainWindow* window, const QString& projectPath);
    static void ApplyDefaultLayout(QMainWindow* window,
                                  QDockWidget* viewportDock,
                                  QDockWidget* sceneObjectsDock,
                                  QDockWidget* inspectorDock,
                                  QDockWidget* fileDock,
                                  QDockWidget* consoleDock);
};
