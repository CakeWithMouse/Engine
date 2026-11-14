#pragma once

#include "DockLayoutManager.h"

#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QWidget>
#include <iostream>
#include <stdexcept>

namespace DockRedockTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(std::string("DockRedock check failed: ") + message);
}

inline QDockWidget* MakeDock(QMainWindow* window, const QString& name, Qt::DockWidgetAreas areas)
{
    auto* dock = new QDockWidget(name, window);
    dock->setObjectName(name);
    dock->setAllowedAreas(areas);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    dock->setWidget(new QWidget(dock));
    return dock;
}

inline void Run()
{
    std::cout << "[Test] Running DockRedockTests..." << std::endl;
    int checks = 0;

    {
        QMainWindow window;
        auto* viewport = MakeDock(&window, QStringLiteral("ViewportDock"), Qt::AllDockWidgetAreas);
        auto* scene = MakeDock(&window, QStringLiteral("SceneObjectsDock"), Qt::AllDockWidgetAreas);
        auto* inspector = MakeDock(&window, QStringLiteral("InspectorDock"), Qt::AllDockWidgetAreas);
        auto* content = MakeDock(&window, QStringLiteral("ContentDock"), Qt::AllDockWidgetAreas);
        auto* console = MakeDock(&window, QStringLiteral("ConsoleDock"), Qt::AllDockWidgetAreas);
        window.resize(1280, 720);
        window.show();
        QApplication::processEvents();
        DockLayoutManager::ApplyDefaultLayout(&window, viewport, scene, inspector, content, console);
        QApplication::processEvents();

        for (auto* dock : {viewport, scene, inspector, content, console})
        {
            Check(dock->allowedAreas() & Qt::LeftDockWidgetArea, "allowedAreas contains Left");
            Check(dock->allowedAreas() & Qt::RightDockWidgetArea, "allowedAreas contains Right");
            Check(dock->allowedAreas() & Qt::TopDockWidgetArea, "allowedAreas contains Top");
            Check(dock->allowedAreas() & Qt::BottomDockWidgetArea, "allowedAreas contains Bottom");
            Check(dock->features() & QDockWidget::DockWidgetMovable, "features contain Movable");
            Check(dock->features() & QDockWidget::DockWidgetFloatable, "features contain Floatable");
            ++checks;
        }

        for (auto* dock : {scene, inspector})
        {
            dock->setFloating(true);
            QApplication::processEvents();
            Check(dock->isFloating(), "dock is floating after setFloating(true)");
            dock->setFloating(false);
            QApplication::processEvents();
            Check(!dock->isFloating(), "dock re-docked after setFloating(false)");
            Check(window.dockWidgetArea(dock) != Qt::NoDockWidgetArea, "dock has a valid area after re-dock");
            ++checks;
        }
    }

    std::cout << "DockRedockTests: " << checks << " checks passed." << std::endl;
}

} // namespace DockRedockTests