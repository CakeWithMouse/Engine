#pragma once
#include "MainWindow.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QThread>
#include <cstdio>
#include <stdexcept>

// Gizmo drags complete at mouse rate; the scene panels must follow at a bounded rate.
namespace GizmoCompletionTests {
inline void Run(EditorMainWindow* window) {
    int checks = 0;
    auto check = [&](bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
        ++checks;
    };
    auto settle = [] {
        QElapsedTimer t; t.start();
        while (t.elapsed() < 120) { QCoreApplication::processEvents(); QThread::msleep(1); }
    };
    auto* add = window->findChild<QPushButton*>(QStringLiteral("SceneAddButton"));
    auto* console = window->findChild<QPlainTextEdit*>(QStringLiteral("Console"));
    check(add && console, "gizmo completion controls exist");
    auto* panel = add->parentWidget();
    settle();
    check(panel->isEnabled(), "scene panel enabled in edit mode");

    EditorCommand tick; tick.type = EditorCommand::UpdateObjectTransform; tick.objectId = 1;
    tick.path = QStringLiteral("gizmo");
    panel->setEnabled(false);
    int completions = 0, refreshes = 0;
    QElapsedTimer gesture; gesture.start();
    while (gesture.elapsed() < 500) {
        window->CommandFinished(tick, EngineSceneOk, {});
        ++completions;
        QCoreApplication::processEvents();
        if (panel->isEnabled()) { ++refreshes; panel->setEnabled(false); }
    }
    const int bound = static_cast<int>(gesture.elapsed() / 30) + 2;
    check(completions > 10 * bound, "gesture produced many more completions than the refresh bound");
    check(refreshes >= 2, "scene UI keeps updating during a gizmo gesture");
    check(refreshes <= bound, "N gizmo completions give at most ~30 Hz of scene UI refreshes");
    settle();
    check(panel->isEnabled(), "one trailing refresh follows the last gizmo completion");

    panel->setEnabled(false);
    const auto marker = QStringLiteral("gizmo-completion-error-marker");
    window->CommandFinished(tick, EngineSceneFailure, marker);
    check(console->toPlainText().contains(marker), "gizmo completion failure stays visible in console");

    EditorCommand nudge = tick; nudge.path = QStringLiteral("nudge");
    window->CommandFinished(nudge, EngineSceneOk, {});
    check(panel->isEnabled(), "non-gizmo transform completion still refreshes immediately");
    settle();
    std::printf("Gizmo completion tests: %d checks passed (N=%d completions, K=%d refreshes, bound=%d)\n",
        checks, completions, refreshes, bound);
}
}
