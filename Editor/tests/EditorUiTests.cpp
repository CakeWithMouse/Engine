#define NOMINMAX
#include <Windows.h>
#include "QtHost.h"
#include "MainWindow.h"
#include "MaterialEditorWindow.h"
#include <QApplication>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QScreen>
#include <QTextCursor>
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
        Check(get && get(ENGINE_EDIT_API_VERSION, sizeof(factory), &factory) && factory.create(engineContent.c_str(), &api), "create edit session");
        struct SessionGuard { EngineEditModuleApi factory; void* context; ~SessionGuard() { if (context) factory.destroy(context); } } sessionGuard{factory, api.context};
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

        // Material editor: template from Files, live parameter, HLSL errors, recompiled parameters, save and discard.
        const auto materialFile = root / "Content/Materials/Tint.mat", surfaceFile = root / "Content/Materials/Tint.hlsl";
        auto editorWidget = [](auto* w, const QString& name) -> QWidget* {
            auto* editor = w->template findChild<MaterialEditorWindow*>();
            return editor && editor->isVisible() ? editor->template findChild<QWidget*>(name) : nullptr;
        };
        auto inEditor = [&](const QString& name) { bool found = false; Ui([&](auto* w) { found = editorWidget(w, name) != nullptr; }); return found; };
        auto saveMaterial = [](auto* w) {
            for (auto* action : w->template findChild<MaterialEditorWindow*>()->template findChildren<QAction*>())
                if (action->text() == QStringLiteral("Сохранить")) { action->trigger(); return; }
            throw std::runtime_error("material Save action");
        };
        auto typeSource = [](QPlainTextEdit* source, const QString& text) {
            QTextCursor cursor(source->document()); // as typed: setPlainText would clear the modified flag
            cursor.select(QTextCursor::Document);
            cursor.insertText(text);
        };
        auto materialText = [&] { QFile f(QString::fromStdWString(materialFile.wstring())); f.open(QIODevice::ReadOnly); return QJsonDocument::fromJson(f.readAll()).object(); };
        Ui([&](auto* w) { Check(w->CreateMaterial({}, QStringLiteral("Tint")).endsWith(QStringLiteral("Tint.mat")), "create material"); });
        Check(std::filesystem::exists(materialFile) && std::filesystem::exists(surfaceFile), "material template files");
        until([&] { return inEditor(QStringLiteral("MaterialParam_Gloss_0")) && inEditor(QStringLiteral("MaterialParam_Tint_3")); });
        Ui([&](auto* w) {
            // The engine presents into the native window: grab the screen, not the widget.
            auto* preview = editorWidget(w, QStringLiteral("MaterialPreview"));
            const QImage frame = preview->screen()->grabWindow(preview->winId()).toImage();
            Check(frame.save(QString::fromStdWString((root / "editor-material-preview.png").wstring())), "save material preview frame");
            const QColor center = frame.pixelColor(frame.width() / 2, frame.height() / 2), corner = frame.pixelColor(4, 4);
            Check(corner.red() > 20 && corner.blue() > corner.red(), "preview clears to its background");
            Check(center != corner, "preview draws the shape");
        });
        Ui([&](auto* w) {
            Check(w->template findChildren<MaterialEditorWindow*>().size() == 1, "one window per material");
            w->OpenMaterialEditor(QStringLiteral("/Game/Materials/Tint.mat"));
            Check(w->template findChildren<MaterialEditorWindow*>().size() == 1, "reopen raises the same window");
            Check(editorWidget(w, QStringLiteral("MaterialColor_Tint")) != nullptr, "color picker for float4");
            static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Gloss_0")))->setValue(0.9);
        });
        until([&] { bool modified = false; Ui([&](auto* w) { modified = w->template findChild<MaterialEditorWindow*>()->HasUnsavedChanges(); }); return modified; });
        Ui([&](auto* w) {
            auto* source = static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")));
            typeSource(source, source->toPlainText().replace(QStringLiteral("o.SpecularWeight = Gloss;"), QStringLiteral("o.SpecularWeight = Missing;")));
            saveMaterial(w);
        });
        int errorLine = 0;
        until([&] {
            Ui([&](auto* w) {
                auto* errors = static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")));
                for (int i = 0; errors && i < errors->count() && !errorLine; ++i) errorLine = errors->item(i)->data(Qt::UserRole).toInt();
            });
            return errorLine != 0;
        });
        Check(errorLine == 11, "compile error points at the surface line");
        Check(std::abs(materialText().value(QStringLiteral("params")).toObject().value(QStringLiteral("Gloss")).toDouble() - 0.9) < 1e-4, "save writes edited parameter");
        Ui([&](auto* w) {
            auto* source = static_cast<QPlainTextEdit*>(editorWidget(w, QStringLiteral("MaterialSource")));
            typeSource(source, source->toPlainText().replace(QStringLiteral("Missing;"), QStringLiteral("Gloss * Rim;"))
                                     .replace(QStringLiteral("float Gloss = 0.5f;"), QStringLiteral("float Gloss = 0.5f;\n    float Rim = 0.25f;")));
            saveMaterial(w);
        });
        until([&] { return inEditor(QStringLiteral("MaterialParam_Rim_0")); });
        Ui([&](auto* w) {
            Check(static_cast<QListWidget*>(editorWidget(w, QStringLiteral("MaterialErrors")))->count() == 0, "fixed HLSL compiles");
            Check(std::abs(static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Gloss_0")))->value() - 0.9) < 1e-4, "recompile keeps edited value");
            static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Gloss_0")))->setValue(0.1);
            static_cast<QComboBox*>(editorWidget(w, QStringLiteral("MaterialPreviewShape")))->setCurrentIndex(1);
        });
        for (int i = 0; i < 30; ++i) pump();
        Ui([](auto* w) { QMetaObject::invokeMethod(w->template findChild<MaterialEditorWindow*>(), "close", Qt::QueuedConnection); });
        until([&] { bool modal = false; Ui([&](auto*) { modal = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) != nullptr; }); return modal; });
        // question() reports the clicked button; done() alone would read as NoButton
        Ui([](auto*) { qobject_cast<QMessageBox*>(QApplication::activeModalWidget())->button(QMessageBox::Discard)->click(); });
        until([&] { bool gone = false; Ui([&](auto* w) { gone = w->template findChild<MaterialEditorWindow*>() == nullptr; }); return gone; });
        Check(std::abs(materialText().value(QStringLiteral("params")).toObject().value(QStringLiteral("Gloss")).toDouble() - 0.9) < 1e-4, "discard keeps the saved file");
        Ui([&](auto* w) { w->OpenMaterialEditor(QString::fromStdWString(materialFile.wstring())); });
        until([&] { return inEditor(QStringLiteral("MaterialParam_Rim_0")); });
        Ui([&](auto* w) {
            Check(std::abs(static_cast<QDoubleSpinBox*>(editorWidget(w, QStringLiteral("MaterialParam_Gloss_0")))->value() - 0.9) < 1e-4, "discard reverts the engine material");
            Check(w->template findChild<MaterialEditorWindow*>()->grab().save(QString::fromStdWString((root / "editor-material.png").wstring())), "save material editor preview");
            QMetaObject::invokeMethod(w->template findChild<MaterialEditorWindow*>(), "close", Qt::QueuedConnection);
        });
        until([&] { bool gone = false; Ui([&](auto* w) { gone = w->template findChild<MaterialEditorWindow*>() == nullptr; }); return gone; });

        // "Редактировать" on the default material: a new material is created, assigned and opened.
        Ui([](auto* w) {
            auto* tree = w->findChild<QTreeWidget*>();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) if (tree->topLevelItem(i)->text(1) == QStringLiteral("Primitive.Sphere")) tree->setCurrentItem(tree->topLevelItem(i));
        });
        until([&] { return host.ReadSnapshot().selected != 0; });
        const uint64_t selected = host.ReadSnapshot().selected;
        Ui([](auto* w) {
            auto* edit = w->template findChild<QPushButton*>(QStringLiteral("EditMaterial"));
            Check(edit && edit->isEnabled(), "edit enabled with the default material");
            edit->click();
        });
        std::string assigned;
        until([&] { for (const auto& o : host.ReadSnapshot().objects) if (o.id == selected) assigned = o.materialAsset; return !assigned.empty(); });
        Check(assigned.rfind("/Game/Materials/", 0) == 0 && std::filesystem::exists(root / "Content" / std::filesystem::u8path(assigned.substr(6))),
              "default material replaced by a new asset");
        until([&] { return inEditor(QStringLiteral("MaterialParam_Gloss_0")); });
        Ui([](auto* w) { QMetaObject::invokeMethod(w->template findChild<MaterialEditorWindow*>(), "close", Qt::QueuedConnection); });
        until([&] { bool gone = false; Ui([&](auto* w) { gone = w->template findChild<MaterialEditorWindow*>() == nullptr; }); return gone; });
        // F6 is the editor's shader reload (games no longer have it): every scene and preview recompiles.
        Ui([](auto* w) {
            QAction* reload = nullptr;
            for (auto* action : w->template findChildren<QAction*>()) if (action->shortcut() == QKeySequence(Qt::Key_F6)) reload = action;
            Check(reload && reload->shortcutContext() == Qt::ApplicationShortcut, "F6 reloads shaders in every editor window");
            reload->trigger();
        });
        until([&] { bool done = false; Ui([&](auto* w) {
            done = w->template findChild<QPlainTextEdit*>(QStringLiteral("Console"))->toPlainText().contains(QStringLiteral("перекомпилированы")); }); return done; });
        Ui([](auto* w) { w->activateWindow(); }); // focused-field checks below need the main window active

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
