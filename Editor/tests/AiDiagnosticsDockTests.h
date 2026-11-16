#pragma once

#include "AiDiagnosticsPanel.h"
#include "ColorSwatch.h"
#include "NavOverlayColors.h"
#include "EditorWindowSetup.h"
#include "DockLayoutManager.h"
#include "CommandRegistry.h"
#include "EditorTheme.h"
#include "InspectorCardStyle.h"
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
        Check(infoBtnWidget == nullptr, "AiLegendInfoButton removed from cardLayers");
        auto* cardLayersWidget = panel.findChild<QWidget*>(QStringLiteral("AiLayersCard"));
        Check(cardLayersWidget != nullptr, "AiLayersCard found");
        auto* cardLayers = dynamic_cast<CollapsibleCard*>(cardLayersWidget);
        if (cardLayers)
        {
            Check(cardLayers->InfoBtn() == nullptr || !cardLayers->InfoBtn()->isVisible(), "AiLayersCard has no visible InfoButton");
        }

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

    // 6. Task X: Details card height, no internal scroll, 5-row legend with ColorSwatch rendering
    {
        QMainWindow window;
        window.resize(320, 600);
        auto* panel = new AiDiagnosticsPanel(nullptr, nullptr, &window);
        window.setCentralWidget(panel);
        window.show();
        QApplication::processEvents();

        auto* detailsCard = panel->findChild<QWidget*>(QStringLiteral("AiDetailsCard"));
        Check(detailsCard != nullptr, "AiDetailsCard found");
        Check(detailsCard->height() >= AiDiagnosticsPanel::kMinDetailsCardHeight, "detailsCard height >= kMinDetailsCardHeight (120)");
        Check(detailsCard->height() >= detailsCard->sizeHint().height(), "detailsCard height >= sizeHint().height()");

        auto* detailsEdit = panel->findChild<QPlainTextEdit*>(QStringLiteral("AiDetails"));
        Check(detailsEdit != nullptr, "AiDetails found");
        Check(detailsEdit->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff, "AiDetails has no internal vertical scrollbar");

        QList<ColorSwatch*> swatches;
        QList<QLabel*> names;
        QList<QLabel*> descs;
        for (int i = 0; i < 5; ++i)
        {
            auto* s = static_cast<ColorSwatch*>(panel->findChild<QWidget*>(QStringLiteral("AiSwatch%1").arg(i)));
            Check(s != nullptr, ("AiSwatch" + std::to_string(i) + " found").c_str());
            if (s) swatches.append(s);

            auto* name = panel->findChild<QLabel*>(QStringLiteral("AiLegendName%1").arg(i));
            Check(name != nullptr && !name->text().isEmpty(), ("AiLegendName" + std::to_string(i) + " not empty").c_str());
            if (name) names.append(name);

            auto* desc = panel->findChild<QLabel*>(QStringLiteral("AiLegendDesc%1").arg(i));
            Check(desc != nullptr && !desc->text().isEmpty(), ("AiLegendDesc" + std::to_string(i) + " not empty").c_str());
            if (desc) descs.append(desc);
        }
        Check(swatches.size() == 5, "Legend has 5 rows with ColorSwatch");
        Check(names.size() == 5, "Legend has 5 rows with name labels");
        Check(descs.size() == 5, "Legend has 5 rows with desc labels");

        // Verify row height, label width, and column alignments
        const int expectedHeight = InspectorCardStyle::kFieldHeight;
        const int expectedLabelWidth = InspectorCardStyle::kLabelWidth;
        const int firstRowXName = names[0]->x();
        const int firstRowXDesc = descs[0]->x();

        for (int i = 0; i < 5; ++i)
        {
            Check(names[i]->height() == expectedHeight, ("AiLegendName" + std::to_string(i) + " has height kFieldHeight").c_str());
            Check(descs[i]->height() == expectedHeight, ("AiLegendDesc" + std::to_string(i) + " has height kFieldHeight").c_str());
            Check(names[i]->width() == expectedLabelWidth, ("AiLegendName" + std::to_string(i) + " has width kLabelWidth").c_str());
            Check(names[i]->x() == firstRowXName, ("AiLegendName" + std::to_string(i) + " has aligned x coord").c_str());
            Check(descs[i]->x() == firstRowXDesc, ("AiLegendDesc" + std::to_string(i) + " has aligned x coord").c_str());
        }

        for (bool dark : {true, false})
        {
            EditorTheme::Instance().SetMode(dark ? ThemeMode::Dark : ThemeMode::Light);
            panel->update();
            QApplication::processEvents();

            std::vector<QColor> colors;
            for (int i = 0; i < swatches.size(); ++i)
            {
                auto* sw = swatches[i];
                QImage img(sw->size(), QImage::Format_ARGB32);
                img.fill(Qt::transparent);
                sw->render(&img);
                QColor center = img.pixelColor(sw->width() / 2, sw->height() / 2);
                Check(center.alpha() > 200, "swatch center pixel is opaque");
                Check(std::abs(center.red() - sw->Color().red()) <= 5 &&
                      std::abs(center.green() - sw->Color().green()) <= 5 &&
                      std::abs(center.blue() - sw->Color().blue()) <= 5,
                      "swatch center pixel matches expected color");
                colors.push_back(center);
            }
            for (size_t i = 0; i < colors.size(); ++i)
            {
                for (size_t j = i + 1; j < colors.size(); ++j)
                {
                    Check(colors[i] != colors[j], "swatch colors pairwise distinct");
                }
            }
        }
        EditorTheme::Instance().SetMode(ThemeMode::Dark);
        checks += 35;
    }

    // 7. Task Y: Standalone editor windows parent and windowFlags verification
    {
        QMainWindow mainWindow;
        mainWindow.setObjectName(QStringLiteral("EditorMainWindow"));
        auto* statusBar = mainWindow.statusBar();
        Check(statusBar != nullptr, "statusBar exists");

        struct TestWindowEntry {
            QString name;
            QMainWindow* window;
        };

        QMainWindow winMaterial(nullptr, Qt::Window);
        winMaterial.setObjectName(QStringLiteral("MaterialEditor"));
        winMaterial.setWindowTitle(QStringLiteral("Material Editor — Test"));
        EditorWindowSetup::ConfigureSubWindow(&winMaterial, &mainWindow);

        QMainWindow winUI(nullptr, Qt::Window);
        winUI.setObjectName(QStringLiteral("UIEditorWindow"));
        winUI.setWindowTitle(QStringLiteral("UI Editor — Test"));
        EditorWindowSetup::ConfigureSubWindow(&winUI, &mainWindow);

        QMainWindow winScript(nullptr, Qt::Window);
        winScript.setObjectName(QStringLiteral("ScriptEditorWindow"));
        winScript.setWindowTitle(QStringLiteral("Script — Test"));
        EditorWindowSetup::ConfigureSubWindow(&winScript, &mainWindow);

        QMainWindow winAiMachine(nullptr, Qt::Window);
        winAiMachine.setObjectName(QStringLiteral("AiMachineEditorWindow"));
        winAiMachine.setWindowTitle(QStringLiteral("AI State Machine — Test"));
        EditorWindowSetup::ConfigureSubWindow(&winAiMachine, &mainWindow);

        std::vector<TestWindowEntry> testWindows = {
            { QStringLiteral("MaterialEditor"), &winMaterial },
            { QStringLiteral("UIEditorWindow"), &winUI },
            { QStringLiteral("ScriptEditorWindow"), &winScript },
            { QStringLiteral("AiMachineEditorWindow"), &winAiMachine }
        };

        for (const auto& entry : testWindows)
        {
            auto* win = entry.window;
            Check(win->parent() == nullptr, (entry.name.toStdString() + " parent == nullptr").c_str());
            Check(win->windowFlags().testFlag(Qt::Window), (entry.name.toStdString() + " has Qt::Window").c_str());
            Check(!win->windowFlags().testFlag(Qt::Tool), (entry.name.toStdString() + " does not have Qt::Tool").c_str());

            win->show();
            QApplication::processEvents();
            win->showMinimized();
            QApplication::processEvents();
            Check(statusBar->currentMessage().contains(win->windowTitle()),
                  (entry.name.toStdString() + " title in statusBar on minimize").c_str());
            Check(statusBar->currentMessage().contains(QStringLiteral("свёрнуто")) ||
                  statusBar->currentMessage().contains(QStringLiteral("minimized")),
                  (entry.name.toStdString() + " status message on minimize").c_str());
            win->hide();
        }

        // Verify closing mainWindow closes child windows
        mainWindow.show();
        for (const auto& entry : testWindows)
        {
            entry.window->show();
        }
        QApplication::processEvents();

        for (const auto& entry : testWindows)
        {
            Check(entry.window->isVisible(), (entry.name.toStdString() + " is visible before mainWindow close").c_str());
        }

        mainWindow.close();
        QApplication::processEvents();

        for (const auto& entry : testWindows)
        {
            Check(!entry.window->isVisible(), (entry.name.toStdString() + " closed when mainWindow closed").c_str());
        }

        checks += 28;
    }

    std::cout << "AiDiagnosticsDock tests: " << checks << " checks passed" << std::endl;
}

} // namespace AiDiagnosticsDockTests
