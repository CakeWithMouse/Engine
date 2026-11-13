#pragma once
#include "MainWindow.h"
#include <QPlainTextEdit>
#include <QPushButton>
#include <stdexcept>
#include <cstdio>

namespace ViewportCompletionTests {
inline void Run(EditorMainWindow* window) {
    int checks = 0;
    auto check = [&](bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
        ++checks;
    };
    auto* add = window->findChild<QPushButton*>(QStringLiteral("SceneAddButton"));
    auto* console = window->findChild<QPlainTextEdit*>(QStringLiteral("Console"));
    check(add && console, "viewport completion controls exist");
    auto* panel = add->parentWidget();
    check(panel->isEnabled(), "scene panel initially enabled in edit mode");
    for (auto type : {EditorCommand::InputCursor, EditorCommand::ViewportCamera}) {
        panel->setEnabled(false);
        EditorCommand command; command.type = type;
        for (int i = 0; i < 1000; ++i)
            window->CommandFinished(command, EngineSceneOk, {});
        check(!panel->isEnabled(), "transient input success must not refresh scene UI");
        const auto marker = QStringLiteral("viewport-completion-regression-%1").arg(static_cast<int>(type));
        window->CommandFinished(command, EngineSceneFailure, marker);
        check(console->toPlainText().contains(marker), "transient input failure remains visible in console");
        EditorCommand changed; changed.type = EditorCommand::RenameObject;
        window->CommandFinished(changed, EngineSceneOk, {});
        check(panel->isEnabled(), "scene mutation completion still refreshes UI");
    }
    std::printf("Viewport completion tests: %d checks passed\n", checks);
}
}
