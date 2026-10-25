#define NOMINMAX
#include <Windows.h>
#include "QtHost.h"
#include "MainWindow.h"
#include <Engine/Runtime/LegacyEditApi.h>
#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QColorDialog>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QImage>
#include <QPixmap>
#include <QDockWidget>
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <stdexcept>

namespace
{
int checks = 0;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); ++checks; }
void Ui(const std::function<void(EditorMainWindow*)>& action)
{
    std::exception_ptr failure;
    QMetaObject::invokeMethod(QCoreApplication::instance(), [&] {
        try
        {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* window = qobject_cast<EditorMainWindow*>(widget)) { action(window); return; }
            throw std::runtime_error("Editor window not found");
        }
        catch (...) { failure = std::current_exception(); }
    }, Qt::BlockingQueuedConnection);
    if (failure) std::rethrow_exception(failure);
}
QAction* Action(EditorMainWindow* window, const QString& label)
{
    for (auto* action : window->findChildren<QAction*>()) if (action->text() == label) return action;
    throw std::runtime_error("Action not found: " + label.toStdString());
}
void Add(EditorMainWindow* window, int index)
{
    auto* combo = window->findChild<QComboBox*>(QStringLiteral("PrimitiveType"));
    Check(combo && combo->count() == 7, "primitive palette"); combo->setCurrentIndex(index);
    auto* button = window->findChild<QPushButton*>(QStringLiteral("AddPrimitive"));
    Check(button && button->isEnabled(), "add button enabled"); button->click();
}
}
int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 3 || argc == 4, "usage: EditorUiTests <project directory> <EngineContent> [model]");
        const auto root = std::filesystem::absolute(argv[1]);
        const auto manifest = (root / "Project.json").u8string(), content = (root / "Content").u8string();
        const auto engineContent = std::filesystem::absolute(argv[2]).u8string();
        HMODULE library = LoadLibraryExW((root / "Game.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        Check(library != nullptr, "load project module");
        struct LibraryGuard { HMODULE module; ~LibraryGuard() { FreeLibrary(module); } } libraryGuard{library};
        auto get = reinterpret_cast<EngineGetEditModuleApiFn>(GetProcAddress(library, "EngineGetEditModuleApi"));
        EngineEditModuleApi factory{}; EngineEditSessionApi api{};
        const bool legacy = argc == 4 && std::wstring(argv[3]) == L"--legacy";
        EngineLegacyV4::Factory oldFactory{}; EngineLegacyV4::EditApi oldSession{};
        if (legacy)
        {
            auto oldGet = reinterpret_cast<EngineLegacyV4::GetFactory>(get);
            Check(oldGet && oldGet(4,sizeof(oldFactory),&oldFactory) && oldFactory.create(engineContent.c_str(),&oldSession) &&
                EngineLegacyV4::Complete(oldSession),"create frozen v4 UI session");
            api = EngineLegacyV4::Adapt(oldSession); factory.destroy = oldFactory.destroy;
        }
        else Check(get && get(ENGINE_EDIT_API_VERSION, sizeof(factory), &factory) && factory.create(engineContent.c_str(), &api), "create edit session");
        struct SessionGuard { EngineEditModuleApi factory; void* context; ~SessionGuard() { if (context) factory.destroy(context); } } sessionGuard{factory, legacy ? oldSession.context : api.context};
        char error[1024]{}; uint64_t scene = 0;
        Check(api.setContentRoot(api.context, content.c_str(), error, sizeof(error)) == EngineSceneOk, "configure UI project content");
        const auto sceneFile = root / "Content/Scenes/Main.scene.json";
        Check(api.loadScene(api.context, sceneFile.u8string().c_str(), &scene, error, sizeof(error)) == EngineSceneOk, "load UI fixture");
        bool exit = false;
        EngineModuleHost callbacks{ENGINE_MODULE_VERSION, &exit,
            [](void*, const char* text) { std::puts(text); }, [](void* p) { *static_cast<bool*>(p) = true; },
            "UI test", manifest.c_str(), content.c_str()};
        EditorQtHost host(callbacks); host.SetModuleApi(api);
        struct HostCleanup
        {
            EditorQtHost& host; EngineEditModuleApi factory; void*& context;
            ~HostCleanup() { host.SetModuleApi({}); if (context) { factory.destroy(context); context = nullptr; } host.Stop(); }
        } hostCleanup{host, factory, sessionGuard.context};
        Check(host.Start(), "start Qt editor");
        auto pump = [&] { host.Update(0.016); Check(api.frame(api.context, 0.016) == EngineSceneOk, "editor render frame"); Sleep(10); };
        int waitIndex = 0;
        auto until = [&](const std::function<bool()>& predicate) {
            const int step = ++waitIndex;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            while (!predicate())
            {
                if (std::chrono::steady_clock::now() > deadline)
                {
                    QString modal;
                    Ui([&](auto*) { if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) modal = box->text(); });
                    throw std::runtime_error("UI condition timed out at step " + std::to_string(step) + ": " + modal.toStdString());
                }
                pump();
            }
            // Allow the queued completion and the UI refresh timer to catch up.
            for (int i = 0; i < 30; ++i) pump();
        };
        until([&] { return host.ViewportActive(); });
        if (legacy)
        {
            const auto originalCount = host.ReadSnapshot().objects.size();
            Ui([](auto* w) {
                auto* combo = w->findChild<QComboBox*>(QStringLiteral("PrimitiveType"));
                Check(combo && combo->count() == 6,"legacy palette excludes unavailable lights");
                combo->setCurrentIndex(0); w->findChild<QPushButton*>(QStringLiteral("AddPrimitive"))->click();
            });
            until([&] { return host.ReadSnapshot().objects.size() == originalCount + 1; });
            auto selectedX = [&] { const auto view = host.ReadSnapshot(); for (const auto& o : view.objects) if (o.id == view.selected) return o.transform.position[0]; return -999.0f; };
            Ui([](auto* w) {
                Check(!w->findChild<QPushButton*>(QStringLiteral("LightColor"))->isVisible(),"legacy light inspector hidden");
                auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0"));
                x->setValue(3.5); QMetaObject::invokeMethod(x,"editingFinished",Qt::DirectConnection);
            });
            until([&] { return selectedX() == 3.5f; });
            Ui([](auto* w) { Action(w,QStringLiteral("Отменить"))->trigger(); });
            until([&] { return !host.ReadSnapshot().selected; });
            Ui([](auto* w) { Action(w,QStringLiteral("Повторить"))->trigger(); });
            until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.transform.position[0] == 3.5f) return true; return false; });
            Ui([](auto* w) { Action(w,QStringLiteral("Сохранить и сделать стартовой"))->trigger(); });
            until([&] { return !host.ReadSnapshot().dirty; });
            Ui([](auto* w) { Action(w,QStringLiteral("▶ Play"))->trigger(); });
            until([&] { return host.PlayState() == EditorQtHost::Play; });
            Ui([](auto* w) { Action(w,QStringLiteral("⏹ Stop"))->trigger(); });
            until([&] { return host.PlayState() == EditorQtHost::Edit; });
            Ui([&](auto* w) { w->OpenScene(QString::fromStdWString(sceneFile.wstring())); });
            for (int i = 0; i < 60; ++i) pump();
            Check(host.ReadSnapshot().objects.size() == originalCount + 1,"legacy reopen preserves objects");
            Ui([](auto* w) { w->close(); }); until([&] { return exit; });
            host.SetModuleApi({}); factory.destroy(sessionGuard.context); sessionGuard.context = nullptr; host.Stop();
            std::printf("Legacy EditorUiTests: %d checks passed\n",checks);
            return 0;
        }
        Ui([](auto* w) { Check(w->findChild<QTreeWidget*>()->topLevelItemCount() == 4, "starter hierarchy"); Add(w, 0); });
        until([&] { return host.ReadSnapshot().objects.size() == 5; });
        Ui([](auto* w) { Add(w, 1); });
        until([&] { return host.ReadSnapshot().objects.size() == 6; });
        const auto imagePath = QString::fromStdWString((root / L"outside.bmp").wstring());
        Ui([&](auto* w) {
            Check(w->findChild<QPushButton*>(QStringLiteral("ImportModel")) && w->findChild<QPushButton*>(QStringLiteral("ImportTexture")), "asset import buttons");
            Check(w->findChild<QPushButton*>(QStringLiteral("AddModel")) && w->findChild<QPushButton*>(QStringLiteral("SceneImportModel")), "model actions visible in scene panel");
            QImage image(4,4,QImage::Format_RGB32); image.fill(Qt::green); Check(image.save(imagePath), "write UI texture fixture");
            w->ImportAsset(true, imagePath);
        });
        const auto importedImage = root / "Content/Textures/outside.bmp";
        until([&] { return std::filesystem::exists(importedImage); });
        Ui([&](auto* w) { w->UseAsset(QString::fromStdWString(importedImage.wstring())); });
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (!o.textureAsset.empty()) return true; return false; });
        Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (!o.textureAsset.empty()) return false; return true; });
        Ui([](auto* w) { Action(w, QStringLiteral("Повторить"))->trigger(); });
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (!o.textureAsset.empty()) return true; return false; });
        if (argc == 4)
        {
            const auto modelSource = std::filesystem::absolute(argv[3]);
            Ui([&](auto* w) { w->ImportAsset(false, QString::fromStdWString(modelSource.wstring())); });
            const auto importedModel = root / "Content/Models" / modelSource.stem() / modelSource.filename();
            until([&] { return std::filesystem::exists(importedModel); });
            Ui([&](auto* w) { w->AddModel(QString::fromStdWString(importedModel.wstring())); });
            until([&] { return host.ReadSnapshot().objects.size() == 7; });
            Ui([&](auto* w) { w->UseAsset(QString::fromStdWString(importedImage.wstring())); });
            until([&] { for (const auto& o : host.ReadSnapshot().objects) if (!o.modelAsset.empty() && !o.textureAsset.empty()) return true; return false; });
            Ui([&](auto* w) {
                for (auto* dock : w->template findChildren<QDockWidget*>()) if (dock->windowTitle() == QStringLiteral("Files")) dock->raise();
                Check(w->grab().save(QString::fromStdWString((root / "editor-assets.png").wstring())), "save editor asset UI preview");
            });
            Ui([](auto* w) { Action(w, QStringLiteral("Дублировать объект"))->trigger(); });
            until([&] { return host.ReadSnapshot().objects.size() == 8; });
            for (const auto& o : host.ReadSnapshot().objects) if (!o.modelAsset.empty()) Check(!o.textureAsset.empty(), "duplicate preserves model texture");
            Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
            until([&] { return host.ReadSnapshot().objects.size() == 7; });
            Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
            until([&] { for (const auto& o : host.ReadSnapshot().objects) if (!o.modelAsset.empty() && !o.textureAsset.empty()) return false; return true; });
            Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
            until([&] { return host.ReadSnapshot().objects.size() == 6; });
        }
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(1) == QStringLiteral("Primitive.Sphere")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return host.ReadSnapshot().selected != 0; });
        auto selectedX = [&] { const auto view = host.ReadSnapshot(); for (const auto& o : view.objects) if (o.id == view.selected) return o.transform.position[0]; return -999.0f; };
        const float originalX = selectedX();
        Ui([](auto* w) { auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0")); Check(x && x->isEnabled(), "inspector enabled"); x->setValue(3.25); QMetaObject::invokeMethod(x, "editingFinished", Qt::DirectConnection); });
        until([&] { return selectedX() == 3.25f; });
        Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
        until([&] { return host.ReadSnapshot().selected == 0; });
        bool restored = false; for (const auto& o : host.ReadSnapshot().objects) if (o.type == "Primitive.Sphere" && o.transform.position[0] == originalX) restored = true;
        Check(restored, "UI undo restores transform");
        Ui([](auto* w) { Action(w, QStringLiteral("Повторить"))->trigger(); });
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.type == "Primitive.Sphere" && o.transform.position[0] == 3.25f) return true; return false; });
        // Select the sphere through the actual tree before testing a focused-field save.
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(1) == QStringLiteral("Primitive.Sphere")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return selectedX() == 3.25f; });
        Ui([](auto* w) { auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0")); x->setFocus(); x->setValue(4.5); Action(w, QStringLiteral("Сохранить и сделать стартовой"))->trigger(); });
        until([&] { return selectedX() == 4.5f && !host.ReadSnapshot().dirty; });
        QFile file(QString::fromStdWString(sceneFile.wstring())); Check(file.open(QIODevice::ReadOnly), "saved scene readable");
        Check(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("objects")).toArray().size() == 6, "UI save persists all objects"); file.close();
        Ui([](auto* w) { Add(w, 6); });
        until([&] { return host.ReadSnapshot().objects.size() == 7; });
        auto selectedLight = [&] {
            const auto view = host.ReadSnapshot();
            for (const auto& o : view.objects) if (o.id == view.selected && o.hasPointLight) return o;
            return EditorQtHost::SceneObject{};
        };
        Check(selectedLight().hasPointLight, "new light selected");
        Ui([](auto* w) {
            auto* button = w->findChild<QPushButton*>(QStringLiteral("LightColor"));
            Check(button && button->isVisible(), "light color button visible"); button->click();
            auto* dialog = w->findChild<QColorDialog*>(); Check(dialog != nullptr, "color picker opens");
            dialog->setCurrentColor(QColor::fromRgbF(0.2f,0.8f,0.4f)); dialog->accept();
        });
        until([&] { return std::abs(selectedLight().pointLight.color[1] - 0.8f) < 0.001f; });
        Ui([](auto* w) { Action(w, QStringLiteral("Отменить"))->trigger(); });
        until([&] { return !host.ReadSnapshot().selected; });
        bool whiteRestored = false;
        for (const auto& o : host.ReadSnapshot().objects) if (o.name == "Точечный свет") whiteRestored = o.pointLight.color[0] == 1;
        Check(whiteRestored, "undo restores previous light color");
        Ui([](auto* w) { Action(w, QStringLiteral("Повторить"))->trigger(); });
        until([&] {
            for (const auto& o : host.ReadSnapshot().objects) if (o.name == "Точечный свет") return std::abs(o.pointLight.color[1] - 0.8f) < 0.001f;
            return false;
        });
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(0) == QStringLiteral("Точечный свет")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return selectedLight().id != 0; });
        Ui([](auto* w) {
            auto* intensity = w->findChild<QDoubleSpinBox*>(QStringLiteral("LightValue3"));
            intensity->setValue(4); QMetaObject::invokeMethod(intensity, "editingFinished", Qt::DirectConnection);
        });
        until([&] { return selectedLight().pointLight.intensity == 4; });
        Ui([](auto* w) { w->findChild<QCheckBox*>(QStringLiteral("LightEnabled"))->click(); });
        until([&] { return !selectedLight().pointLight.enabled; });
        Ui([](auto* w) { w->findChild<QCheckBox*>(QStringLiteral("LightEnabled"))->click(); });
        until([&] { return selectedLight().pointLight.enabled; });
        Ui([](auto* w) {
            auto* x = w->findChild<QDoubleSpinBox*>(QStringLiteral("Transform0"));
            x->setValue(-3); QMetaObject::invokeMethod(x, "editingFinished", Qt::DirectConnection);
        });
        until([&] { return selectedLight().transform.position[0] == -3; });
        Ui([&](auto* w) { Check(w->grab().save(QString::fromStdWString((root / "editor-light.png").wstring())), "save light inspector preview"); });
        Ui([](auto* w) { Action(w, QStringLiteral("Дублировать объект"))->trigger(); });
        until([&] { return host.ReadSnapshot().objects.size() == 8; });
        Check(selectedLight().pointLight.intensity == 4 && std::abs(selectedLight().pointLight.color[1] - 0.8f) < 0.001f &&
            selectedLight().transform.position[0] == -2, "duplicate preserves light properties and offsets position");
        Ui([](auto* w) { Action(w, QStringLiteral("Удалить объект"))->trigger(); });
        until([&] { return host.ReadSnapshot().objects.size() == 7; });
        Ui([](auto* w) { Action(w, QStringLiteral("Сохранить и сделать стартовой"))->trigger(); });
        until([&] { return !host.ReadSnapshot().dirty; });
        Ui([](auto* w) { Action(w, QStringLiteral("▶ Play"))->trigger(); });
        until([&] { return host.PlayState() == EditorQtHost::Play; });
        Ui([](auto* w) { Check(!w->findChild<QPushButton*>(QStringLiteral("AddPrimitive"))->isEnabled(), "editing disabled during Play"); Action(w, QStringLiteral("⏹ Stop"))->trigger(); });
        until([&] { return host.PlayState() == EditorQtHost::Edit; });
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(0) == QStringLiteral("Точечный свет")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return selectedLight().id != 0; });
        Ui([](auto* w) { Action(w, QStringLiteral("Удалить объект"))->trigger(); });
        until([&] { return host.ReadSnapshot().objects.size() == 6; });
        Ui([](auto* w) { Add(w, 4); Action(w, QStringLiteral("Новая сцена"))->trigger(); });
        until([&] { bool modal = false; Ui([&](auto*) { modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) != nullptr; }); return modal; });
        Check(host.ReadSnapshot().objects.size() == 7, "queued addition preserved before New scene prompt");
        Ui([](auto*) { qobject_cast<QMessageBox*>(QApplication::activeModalWidget())->done(QMessageBox::Cancel); });
        for (int i = 0; i < 30; ++i) pump();
        Check(host.ReadSnapshot().objects.size() == 7 && host.ReadSnapshot().dirty, "cancel New keeps scene");
        Ui([](auto* w) { w->close(); });
        until([&] { bool modal = false; Ui([&](auto*) { modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) != nullptr; }); return modal; });
        Check(!exit, "unsaved close waits for a decision");
        Ui([](auto*) { auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); Check(box, "unsaved changes prompt"); box->done(QMessageBox::Cancel); });
        for (int i = 0; i < 30; ++i) pump();
        Check(!exit && host.ReadSnapshot().dirty, "cancel preserves editing session");
        Ui([](auto* w) { Action(w, QStringLiteral("Сохранить и сделать стартовой"))->trigger(); });
        until([&] { return !host.ReadSnapshot().dirty; });
        Ui([](auto* w) { w->close(); });
        until([&] { return exit; });
        host.SetModuleApi({}); factory.destroy(api.context); sessionGuard.context = nullptr; host.Stop();
        std::printf("EditorUiTests: %d checks passed\n", checks);
        return 0;
    }
    catch (const std::exception& e) { std::fprintf(stderr, "EditorUiTests: %s\n", e.what()); return 1; }
}
