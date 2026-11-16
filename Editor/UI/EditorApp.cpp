#include <QMouseEvent>
#include "ViewportWidget.h"
#include <QToolBar>
#include <QToolButton>
#include "EditorApp.h"
#include "MainWindow.h"
#include "IEditorHost.h"
#include "QtHost.h"
#include "EditorTheme.h"
#include "../Core/CommandRegistry.h"
#include "../Core/EditorLocale.h"
#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QPixmap>
#include <QImage>
#include <vector>
#include <QQuaternion>
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

        const QString appDir = QCoreApplication::applicationDirPath();
        std::vector<QString> iconCandidates = {
            appDir + QStringLiteral("/icon_pain_multisize.ico"),
            appDir + QStringLiteral("/Resources/icon_pain_multisize.ico"),
            appDir + QStringLiteral("/../icon_pain_multisize.ico"),
            appDir + QStringLiteral("/../../icon_pain_multisize.ico"),
            appDir + QStringLiteral("/../../../icon_pain_multisize.ico"),
            appDir + QStringLiteral("/../../../../icon_pain_multisize.ico"),
            appDir + QStringLiteral("/../../../../../icon_pain_multisize.ico"),
            appDir + QStringLiteral("/../../../../../Editor/Resources/icon_pain_multisize.ico"),
            appDir + QStringLiteral("/../../../../Editor/Resources/icon_pain_multisize.ico"),
            QDir::currentPath() + QStringLiteral("/icon_pain_multisize.ico"),
            QDir::currentPath() + QStringLiteral("/Editor/Resources/icon_pain_multisize.ico"),
            QDir::currentPath() + QStringLiteral("/Resources/icon_pain_multisize.ico"),
            QStringLiteral("Editor/Resources/icon_pain_multisize.ico"),
            QStringLiteral("Resources/icon_pain_multisize.ico"),
            QStringLiteral("icon_pain_multisize.ico")
        };
        const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
        if (!localAppData.isEmpty())
        {
            iconCandidates.push_back(localAppData + QStringLiteral("/PainEngine/icon_pain_multisize.ico"));
            iconCandidates.push_back(localAppData + QStringLiteral("/PainEngine/Engines/icon_pain_multisize.ico"));
        }

        QString iconPath;
        for (const auto& candidate : iconCandidates)
        {
            if (QFile::exists(candidate))
            {
                iconPath = candidate;
                break;
            }
        }

        QIcon appIcon;
        if (!iconPath.isEmpty())
        {
            appIcon = QIcon(iconPath);
        }

        HICON hBig = nullptr;
        HICON hSm = nullptr;
        if (!iconPath.isEmpty())
        {
            const std::wstring nativePath = QDir::toNativeSeparators(iconPath).toStdWString();
            hBig = static_cast<HICON>(LoadImageW(nullptr, nativePath.c_str(), IMAGE_ICON,
                GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE));
            hSm = static_cast<HICON>(LoadImageW(nullptr, nativePath.c_str(), IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE));
        }
        if (!hBig)
        {
            hBig = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
            hSm = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
        }

        if (appIcon.isNull() && hBig)
        {
            QImage img = QImage::fromHICON(hBig);
            if (!img.isNull())
            {
                appIcon = QIcon(QPixmap::fromImage(img));
            }
        }

        if (!appIcon.isNull())
        {
            app.setWindowIcon(appIcon);
        }

        HWND consoleWnd = ::GetConsoleWindow();
        if (consoleWnd && (hBig || hSm))
        {
            if (hBig) SendMessageW(consoleWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hBig));
            if (hSm) SendMessageW(consoleWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hSm));
        }

        auto* qtHost = dynamic_cast<EditorQtHost*>(host);
        qtHost->InitializeRecovery();
        EditorMainWindow window(qtHost);
        if (!appIcon.isNull())
        {
            window.setWindowIcon(appIcon);
        }
        host->SetUiListener(&window);
        window.show();
        HWND mainWnd = reinterpret_cast<HWND>(window.winId());
        if (mainWnd && (hBig || hSm))
        {
            if (hBig) SendMessageW(mainWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hBig));
            if (hSm) SendMessageW(mainWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hSm));
        }
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

            
            // Step 1: Select Cube
            QTimer::singleShot(1200, [host] {
                auto snap = host->ReadSnapshot();
                uint64_t cubeId = 0;
                for (const auto& obj : snap.objects) {
                    if (obj.name == "Cube" || obj.type == "Primitive.Cube") { cubeId = obj.id; break; }
                }
                if (cubeId == 0 && snap.objects.size() > 1) cubeId = snap.objects[1].id;
                std::printf("[SmokeTest] Step 1: Selecting Cube (id %llu)\n", cubeId);
                std::fflush(stdout);
                EditorCommand c;
                c.type = EditorCommand::SelectObject;
                c.objectId = cubeId;
                host->PostCommand(c);
            });

            // Step 2: Test View menu toggles
            QTimer::singleShot(2000, [] {
                std::printf("[SmokeTest] Step 2: Testing View menu toggles (Grid=off, Gizmo=off, Icons=off, AA=off)\n");
                std::fflush(stdout);
                CommandRegistry::Instance().Execute(QStringLiteral("View.ToggleGrid"));
                CommandRegistry::Instance().Execute(QStringLiteral("View.ToggleGizmo"));
                CommandRegistry::Instance().Execute(QStringLiteral("View.ToggleIcons"));
                QVariantMap pOff; pOff.insert(QStringLiteral("mode"), QStringLiteral("off"));
                CommandRegistry::Instance().Execute(QStringLiteral("View.SetAntialiasing"), pOff);
            });

            // Step 3: Restore View menu toggles
            QTimer::singleShot(2600, [] {
                std::printf("[SmokeTest] Step 3: Restoring View menu toggles (Grid=on, Gizmo=on, Icons=on, AA=fxaa)\n");
                std::fflush(stdout);
                CommandRegistry::Instance().Execute(QStringLiteral("View.ToggleGrid"));
                CommandRegistry::Instance().Execute(QStringLiteral("View.ToggleGizmo"));
                CommandRegistry::Instance().Execute(QStringLiteral("View.ToggleIcons"));
                QVariantMap pFxaa; pFxaa.insert(QStringLiteral("mode"), QStringLiteral("fxaa"));
                CommandRegistry::Instance().Execute(QStringLiteral("View.SetAntialiasing"), pFxaa);
            });

            // Step 4: Select PointLight and modify parameters
            QTimer::singleShot(3200, [host] {
                auto snap = host->ReadSnapshot();
                uint64_t lightId = 0;
                for (const auto& obj : snap.objects) {
                    if (obj.hasPointLight || obj.type == "Light.Point" || obj.name.find("Light") != std::string::npos) {
                        lightId = obj.id;
                        break;
                    }
                }
                if (lightId != 0)
                {
                    std::printf("[SmokeTest] Step 4: Selecting PointLight (id %llu) and modifying parameters\n", lightId);
                    std::fflush(stdout);
                    EditorCommand cSel;
                    cSel.type = EditorCommand::SelectObject;
                    cSel.objectId = lightId;
                    host->PostCommand(cSel);

                    EditorCommand cLight;
                    cLight.type = EditorCommand::SetPointLight;
                    cLight.objectId = lightId;
                    cLight.pointLight.color[0] = 0.9f;
                    cLight.pointLight.color[1] = 0.7f;
                    cLight.pointLight.color[2] = 0.4f;
                    cLight.pointLight.intensity = 3.5f;
                    cLight.pointLight.radius = 12.0f;
                    cLight.pointLight.enabled = 1;
                    cLight.castShadows = false;
                    host->PostCommand(cLight);
                }
            });

            // Step 5: Show / Create Directional Light and select it
            QTimer::singleShot(3400, [host] {
                std::printf("[SmokeTest] Step 5: Creating and Selecting Directional Light\n");
                std::fflush(stdout);
                auto snap = host->ReadSnapshot();
                uint64_t dirId = 0;
                for (const auto& obj : snap.objects) {
                    if (obj.type == "Light.Directional") { dirId = obj.id; break; }
                }
                if (dirId != 0)
                {
                    EditorCommand cSel; cSel.type = EditorCommand::SelectObject; cSel.objectId = dirId;
                    host->PostCommand(cSel);
                }
                else
                {
                    EditorCommand c; c.type = EditorCommand::CreateObject;
                    c.path = "Light.Directional"; c.name = Tr("Направленный свет", "Directional Light");
                    const auto rotation = QQuaternion::fromEulerAngles(-55, -30, 0);
                    c.transform.rotation[0] = rotation.x(); c.transform.rotation[1] = rotation.y();
                    c.transform.rotation[2] = rotation.z(); c.transform.rotation[3] = rotation.scalar();
                    c.transform.position[0] = 0.0f; c.transform.position[1] = 5.0f; c.transform.position[2] = 0.0f;
                    host->PostCommand(c);
                }
            });

            // Step 6: Save screenshot and exit
            QTimer::singleShot(4800, [&window, screenshotPath] {
                auto* dock = window.findChild<QDockWidget*>("InspectorDock");
                if (dock) {
                    for (auto* c : dock->findChildren<QWidget*>()) {
                        if (c->isVisible() && (c->inherits("QFrame") || c->inherits("QPushButton") || c->inherits("QLabel") || c->inherits("QCheckBox"))) {
                            std::printf("[InspectorChild] %s '%s' geom=%d,%d %dx%d map=%d,%d\n",
                                c->metaObject()->className(), c->objectName().toUtf8().constData(),
                                c->geometry().x(), c->geometry().y(), c->geometry().width(), c->geometry().height(),
                                c->mapTo(&window, QPoint(0, 0)).x(), c->mapTo(&window, QPoint(0, 0)).y());
                        }
                    }
                }
                QPixmap grab = window.grab();
                grab.save(screenshotPath);
                std::printf("[Info] SmokeEditor: screenshot saved to %s (%dx%d)\n",
                    screenshotPath.toUtf8().constData(), grab.width(), grab.height());
                std::fflush(stdout);
                QApplication::quit();
                ExitProcess(0);
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
