#include "EditorApp.h"
#include "MainWindow.h"
#include "IEditorHost.h"
#include "QtHost.h"
#include "EditorTheme.h"
#include <QApplication>
#include <QTimer>
#include <QPixmap>
#include <functional>
#include <cstdio>
#include <cwchar>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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

        const bool smokeMode = qEnvironmentVariableIsSet("PAINENGINE_SMOKE_EDITOR") ||
            (std::wcsstr(GetCommandLineW(), L"--smoke-editor") != nullptr);

        if (smokeMode)
        {
            QString screenshotPath = QString::fromLocal8Bit(qgetenv("PAINENGINE_SMOKE_SCREENSHOT"));
            if (screenshotPath.isEmpty())
            {
                screenshotPath = QStringLiteral("editor_smoke.png");
            }

            QTimer::singleShot(1500, [&window, host, screenshotPath] {
                std::printf("[Info] SmokeEditor: selecting Cube (id 2)\n");
                std::fflush(stdout);
                EditorCommand c1;
                c1.type = EditorCommand::SelectObject;
                c1.objectId = 2;
                host->PostCommand(c1);

                QTimer::singleShot(600, [&window, host, screenshotPath] {
                    std::printf("[Info] SmokeEditor: selecting Warm Light (id 3)\n");
                    std::fflush(stdout);
                    EditorCommand c2;
                    c2.type = EditorCommand::SelectObject;
                    c2.objectId = 3;
                    host->PostCommand(c2);

                    QTimer::singleShot(800, [&window, screenshotPath] {
                        QPixmap grab = window.grab();
                        grab.save(screenshotPath);
                        std::printf("[Info] SmokeEditor: screenshot saved to %s (%dx%d)\n",
                            screenshotPath.toUtf8().constData(), grab.width(), grab.height());
                        std::fflush(stdout);
                        QApplication::quit();
                    });
                });
            });
        }

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
