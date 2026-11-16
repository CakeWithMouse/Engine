#pragma once

#include "AiDiagnosticsPanel.h"
#include "DockLayoutManager.h"
#include "CommandRegistry.h"
#include "EditorTheme.h"
#include "SettingsService.h"

#include <QApplication>
#include <QDockWidget>
#include <QGroupBox>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <iostream>
#include <stdexcept>

namespace AiDiagnosticsDockTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(std::string("AiDiagnosticsDock check failed: ") + message);
}

inline void Run()
{
    int checks = 0;
    std::cout << "[Test] Running AiDiagnosticsDockTests..." << std::endl;

    // 1. Panel structure & styling verification
    {
        AiDiagnosticsPanel panel(nullptr, nullptr);
        panel.resize(320, 600);

        // Verify minimum size is fluid (no hardcoded 390/460 constraints)
        Check(panel.minimumSize() == QSize(40, 30), "panel has compact fluid minimum size (40, 30)");
        Check(panel.sizePolicy().horizontalPolicy() == QSizePolicy::Ignored, "panel has Ignored horizontal size policy for dock resizing");
        Check(panel.sizePolicy().verticalPolicy() == QSizePolicy::Ignored, "panel has Ignored vertical size policy for dock resizing");

        // Verify no raw QGroupBox used; styled with CollapsibleCards
        auto groupBoxes = panel.findChildren<QGroupBox*>();
        Check(groupBoxes.isEmpty(), "no crude QGroupBox frames; sections use CollapsibleCards");

        int cardCount = 0;
        for (auto* w : panel.findChildren<QWidget*>())
        {
            if (w->property("collapsibleCard").toBool())
                ++cardCount;
        }
        Check(cardCount >= 5, "all 5 collapsible cards present (Layers, Subject, Probe, Display, Details)");

        // Verify key widgets by objectName
        Check(panel.findChild<QCheckBox*>(QStringLiteral("AiEnabled")) != nullptr, "AiEnabled checkbox found");
        Check(panel.findChild<QLabel*>(QStringLiteral("AiSourceStatus")) != nullptr, "AiSourceStatus label found");
        Check(panel.findChild<QComboBox*>(QStringLiteral("AiSubject")) != nullptr, "AiSubject combobox found");
        Check(panel.findChild<QPlainTextEdit*>(QStringLiteral("AiDetails")) != nullptr, "AiDetails text edit found");

        for (int i = 0; i < 8; ++i)
        {
            Check(panel.findChild<QCheckBox*>(QStringLiteral("AiLayer%1").arg(i)) != nullptr,
                  ("AiLayer" + std::to_string(i) + " found").c_str());
        }
        checks += 13;

        // Task H: Structure, legend, details, and command synchronization
        if (!CommandRegistry::Instance().HasCommand(QStringLiteral("View.ToggleAiDiagnostics")))
        {
            CommandDef def;
            def.id = QStringLiteral("View.ToggleAiDiagnostics");
            def.title = QStringLiteral("Toggle AI Diagnostics");
            def.handler = [](const CommandContext& ctx) {
                bool current = SettingsService::Instance().GetValue(QStringLiteral("ai/enabled"), false).toBool();
                bool next = ctx.parameters.contains(QStringLiteral("enabled"))
                    ? ctx.parameters.value(QStringLiteral("enabled")).toBool()
                    : !current;
                SettingsService::Instance().SetValue(QStringLiteral("ai/enabled"), next);
                return true;
            };
            CommandRegistry::Instance().RegisterCommand(def);
        }

        auto* detailsCard = panel.findChild<QWidget*>(QStringLiteral("AiDetailsCard"));
        Check(detailsCard != nullptr, "AiDetailsCard found");
        auto* cardObj = dynamic_cast<CollapsibleCard*>(detailsCard);
        Check(cardObj != nullptr && (cardObj->Title().contains(QStringLiteral("память")) || cardObj->Title().contains(QStringLiteral("Memory"))),
              "AiDetailsCard has updated title");

        auto* detailsEdit = panel.findChild<QPlainTextEdit*>(QStringLiteral("AiDetails"));
        Check(detailsEdit != nullptr, "AiDetails found");
        Check(detailsEdit->minimumHeight() >= 150, "AiDetails has minimumHeight >= 150");
        Check(detailsEdit->sizePolicy().verticalPolicy() == QSizePolicy::MinimumExpanding, "AiDetails expands vertically");

        auto* legend = panel.findChild<QLabel*>(QStringLiteral("AiLegend"));
        Check(legend != nullptr, "AiLegend found");
        Check(legend->text().contains(QStringLiteral("Проход")) || legend->text().contains(QStringLiteral("Walkable")),
              "AiLegend has compact legend text");

        auto* infoBtnWidget = panel.findChild<QToolButton*>(QStringLiteral("AiLegendInfoButton"));
        Check(infoBtnWidget != nullptr, "AiLegendInfoButton found");
        auto* infoBtn = dynamic_cast<InfoButton*>(infoBtnWidget);
        Check(infoBtn != nullptr, "AiLegendInfoButton is InfoButton");
        infoBtn->click();
        QApplication::processEvents();
        Check(infoBtn->Popup() != nullptr && infoBtn->Popup()->isVisible(), "AiLegendInfoButton popup opens on click");
        auto* popupText = infoBtn->Popup()->findChild<QLabel*>(QStringLiteral("InfoPopupText"));
        Check(popupText != nullptr && (popupText->text().contains(QStringLiteral("Зеленый")) || popupText->text().contains(QStringLiteral("Green"))),
              "InfoPopupText contains layer legend text");
        infoBtn->Popup()->hide();

        auto* cellInfo = panel.findChild<QLabel*>(QStringLiteral("AiCellInfo"));
        Check(cellInfo != nullptr, "AiCellInfo found");
        Check(cellInfo->text().contains(QStringLiteral("Клетка")) || cellInfo->text().contains(QStringLiteral("Cell")),
              "AiCellInfo shows initial compact hint");

        auto* cellDetails = panel.findChild<QWidget*>(QStringLiteral("AiCellDetails"));
        Check(cellDetails != nullptr, "AiCellDetails found");
        Check(cellDetails->isHidden(), "AiCellDetails is initially hidden when no cell is hovered");

        // Sync test: View.ToggleAiDiagnostics command <-> AiEnabled checkbox <-> SettingsService
        auto* aiEnabled = panel.findChild<QCheckBox*>(QStringLiteral("AiEnabled"));
        const bool prevEnabled = aiEnabled->isChecked();
        CommandRegistry::Instance().Execute(QStringLiteral("View.ToggleAiDiagnostics"), {{QStringLiteral("enabled"), !prevEnabled}});
        Check(aiEnabled->isChecked() == !prevEnabled, "AiEnabled checkbox synced from View.ToggleAiDiagnostics command");
        Check(SettingsService::Instance().GetValue(QStringLiteral("ai/enabled")).toBool() == !prevEnabled,
              "SettingsService ai/enabled synced from View.ToggleAiDiagnostics command");

        // Restore
        CommandRegistry::Instance().Execute(QStringLiteral("View.ToggleAiDiagnostics"), {{QStringLiteral("enabled"), prevEnabled}});
        Check(aiEnabled->isChecked() == prevEnabled, "AiEnabled checkbox restored");
        checks += 11;
    }

    // 2. Dock lifecycle: Open, Close, Reopen
    {
        QMainWindow window;
        window.resize(1024, 768);
        window.show();
        QApplication::processEvents();

        auto* viewportDock = new QDockWidget(QStringLiteral("Viewport"), &window);
        viewportDock->setObjectName(QStringLiteral("ViewportDock"));
        window.addDockWidget(Qt::TopDockWidgetArea, viewportDock);

        auto* inspectorDock = new QDockWidget(QStringLiteral("Inspector"), &window);
        inspectorDock->setObjectName(QStringLiteral("InspectorDock"));
        window.addDockWidget(Qt::TopDockWidgetArea, inspectorDock);

        auto* aiDock = new QDockWidget(QStringLiteral("AI Diagnostics"), &window);
        aiDock->setObjectName(QStringLiteral("AiDiagnosticsDock"));
        aiDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        aiDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
        aiDock->setWidget(new AiDiagnosticsPanel(nullptr, nullptr, aiDock));
        window.addDockWidget(Qt::RightDockWidgetArea, aiDock);
        aiDock->hide();

        Check(aiDock->isHidden(), "AiDiagnosticsDock starts hidden");

        // Open
        aiDock->show();
        Check(aiDock->isVisible(), "AiDiagnosticsDock becomes visible after open");
        ++checks;

        // Close
        aiDock->hide();
        Check(aiDock->isHidden(), "AiDiagnosticsDock becomes hidden after close");
        ++checks;

        // Reopen
        aiDock->show();
        Check(aiDock->isVisible(), "AiDiagnosticsDock becomes visible after reopen");
        ++checks;

        // 3. Tabify with another dock
        window.tabifyDockWidget(inspectorDock, aiDock);
        aiDock->show();
        aiDock->raise();
        QApplication::processEvents();

        Check(!aiDock->isFloating(), "AiDiagnosticsDock is not floating when tabified");
        Check(aiDock->isVisible(), "AiDiagnosticsDock remains visible in tab group");
        checks += 2;

        // 4. Save / Restore layout does not lose the dock
        const QByteArray savedState = window.saveState();
        Check(!savedState.isEmpty(), "window saveState is not empty");

        // Close dock and verify hidden
        aiDock->hide();
        Check(aiDock->isHidden(), "AiDiagnosticsDock is hidden before restore");

        // Restore state
        const bool restored = window.restoreState(savedState);
        Check(restored, "window restoreState succeeded");
        Check(aiDock->isVisible(), "AiDiagnosticsDock restored to visible state");
        checks += 4;

        // 5. Reset Layout
        auto* sceneDock = new QDockWidget(QStringLiteral("Scene"), &window);
        sceneDock->setObjectName(QStringLiteral("SceneObjectsDock"));
        auto* fileDock = new QDockWidget(QStringLiteral("Content"), &window);
        fileDock->setObjectName(QStringLiteral("FileDock"));
        auto* consoleDock = new QDockWidget(QStringLiteral("Console"), &window);
        consoleDock->setObjectName(QStringLiteral("ConsoleDock"));

        DockLayoutManager::ApplyDefaultLayout(&window, viewportDock, sceneDock, inspectorDock, fileDock, consoleDock, aiDock);
        Check(aiDock->isHidden(), "ApplyDefaultLayout sets AiDiagnosticsDock hidden by default");
        Check(!aiDock->isFloating(), "ApplyDefaultLayout sets AiDiagnosticsDock non-floating");
        checks += 2;
    }

    std::cout << "AiDiagnosticsDock tests: " << checks << " checks passed" << std::endl;
}

} // namespace AiDiagnosticsDockTests
