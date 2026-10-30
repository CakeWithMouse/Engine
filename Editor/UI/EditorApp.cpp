#include "EditorApp.h"
#include "MainWindow.h"
#include "IEditorHost.h"
#include "QtHost.h"
#include "EditorTheme.h"
#include <QApplication>
#include <functional>

void EditorQtHostRegisterDefaultLauncher(std::function<void(IEditorHost*, std::atomic<bool>*)> launcher);

namespace
{
    int g_argc = 1;
    char g_arg0[] = "Editor.dll";
    char* g_argv[] = {g_arg0, nullptr};

    void LaunchEditorWindow(IEditorHost* host, std::atomic<bool>* ready)
    {
        QApplication app(g_argc, g_argv);
        app.setQuitOnLastWindowClosed(false);
        app.setApplicationName(QStringLiteral("Engine Editor"));
        EditorTheme::Apply(app);

        auto* qtHost = dynamic_cast<EditorQtHost*>(host);
        EditorMainWindow window(qtHost);
        host->SetUiListener(&window);
        window.show();
        ready->store(true);

        app.exec();

        host->SetUiListener(nullptr);
    }

    struct Bootstrap
    {
        Bootstrap()
        {
            EditorQtHostRegisterDefaultLauncher(LaunchEditorWindow);
        }
    } s_bootstrap;
}

void RunEditorMainWindow(IEditorHost* host, std::atomic<bool>* ready)
{
    LaunchEditorWindow(host, ready);
}
