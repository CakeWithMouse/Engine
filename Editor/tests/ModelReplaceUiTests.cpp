#define NOMINMAX
#include <Windows.h>
#include "QtHost.h"
#include "MainWindow.h"
#include "ObjectInspectorPanel.h"
#include "CommandRegistry.h"
#include <Engine/Runtime/GameEditModule.h>
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <filesystem>
#include <cstdio>
#include <stdexcept>

namespace {
int checks = 0;
void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
    ++checks; std::printf("PASS %s\n", message); std::fflush(stdout);
}
QByteArray Read(const std::filesystem::path& path) {
    QFile file(QString::fromStdWString(path.wstring()));
    Check(file.open(QIODevice::ReadOnly), "read scene"); return file.readAll();
}
void Ui(const std::function<void(EditorMainWindow*)>& action) {
    std::exception_ptr failure;
    QMetaObject::invokeMethod(QCoreApplication::instance(), [&] {
        try {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* window = qobject_cast<EditorMainWindow*>(widget)) { action(window); return; }
            throw std::runtime_error("Editor window not found");
        } catch (...) { failure = std::current_exception(); }
    }, Qt::BlockingQueuedConnection);
    if (failure) std::rethrow_exception(failure);
}
}

int wmain(int argc, wchar_t** argv) {
    try {
        Check(argc == 3, "usage: ModelReplaceUiTests <isolated project> <EngineContent>");
        const auto root = std::filesystem::absolute(argv[1]);
        const auto scenePath = root / "Content/Scenes/Main.scene.json";
        const auto originalBytes = Read(scenePath);
        const auto source = QJsonDocument::fromJson(originalBytes).object();
        Check(source["version"].toInt() == 1, "real old scene is version 1");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                          QString::fromStdWString((root / "TestSettings").wstring()));
        const auto library = LoadLibraryExW((root / "Game.dll").c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        Check(library != nullptr, "load real project DLL");
        struct LibraryGuard { HMODULE value; ~LibraryGuard() { FreeLibrary(value); } } libraryGuard{library};
        auto get = reinterpret_cast<EngineGetEditModuleApiFn>(GetProcAddress(library, "EngineGetEditModuleApi"));
        EngineEditModuleApi factory{}; EngineEditSessionApi api{};
        const auto engineContent = std::filesystem::absolute(argv[2]).u8string();
        Check(get && get(ENGINE_EDIT_API_VERSION, sizeof(factory), &factory) &&
              factory.create(engineContent.c_str(), &api), "create real engine session");
        const auto content = (root / "Content").u8string(), manifest = (root / "Project.json").u8string();
        char error[1024]{}; uint64_t sceneId = 0;
        Check(api.setContentRoot(api.context, content.c_str(), error, sizeof(error)) == EngineSceneOk, "set content root");
        Check(api.loadScene(api.context, scenePath.u8string().c_str(), &sceneId, error, sizeof(error)) == EngineSceneOk,
              "load unchanged repository scene through engine");
        EngineModuleHost callbacks{ENGINE_MODULE_VERSION, nullptr,
            [](void*, const char* text) { std::puts(text); }, [](void*) {}, "Legacy replacement test", manifest.c_str(), content.c_str()};
        EditorQtHost host(callbacks); host.SetModuleApi(api);
        struct Cleanup {
            EditorQtHost& host; EngineEditModuleApi factory; void* context;
            ~Cleanup() { host.SetModuleApi({}); factory.destroy(context); host.Stop(); }
        } cleanup{host, factory, api.context};
        Check(host.Start(), "start actual editor command registry");
        auto pump = [&] { for (int i = 0; i < 40; ++i) { host.Update(0.016); Sleep(10); } };
        pump();
        auto initial = host.ReadSnapshot();
        uint64_t model = 0, primitive = 0, light = 0; std::string originalModel;
        for (const auto& o : initial.objects) {
            std::printf("SNAPSHOT id=%llu type=%s model=%s\n", static_cast<unsigned long long>(o.id), o.type.c_str(), o.modelAsset.c_str());
            if (o.type == "Engine.Model") { model = o.id; originalModel = o.modelAsset; }
            if (o.type == "Primitive.Plane") primitive = o.id;
            if (o.type == "Light.Point") light = o.id;
        }
        Check(model && primitive && light, "real fixture contains model, primitive and light");
        Check(!initial.canUndo, "clean history before replacement");
        auto select = [&](uint64_t id) {
            EditorCommand c; c.type = EditorCommand::SelectObject; c.objectId = id; host.PostCommand(c); pump();
        };
        select(model);
        Ui([&](auto* window) {
            auto* panel = window->template findChild<ObjectInspectorPanel*>();
            Check(panel != nullptr, "actual inspector exists");
            panel->Refresh(host.ReadSnapshot(), model, true);
            Check(panel->findChild<QPushButton*>("ChooseModel")->isEnabled(), "old scene model enables replacement");
        });
        auto save = [&](const char* name) {
            const auto path = root / name;
            Check(api.saveScene(api.context, path.u8string().c_str(), error, sizeof(error)) == EngineSceneOk, "serialize actual scene");
            return QJsonDocument::fromJson(Read(path)).object();
        };
        const auto before = save("before.scene.json");
        Check(before["version"].toInt() == 3, "existing engine checkpoint serializer emits version 3 before replacement");
        bool jsonModel = false;
        for (const auto& row : before["objects"].toArray()) {
            const auto o = row.toObject();
            if (o["id"].toInteger() == static_cast<qint64>(model)) jsonModel = o["type"] == "Engine.Model";
        }
        Check(jsonModel, "snapshot and checkpoint JSON agree on Engine.Model");
        // A queued command must use its explicit target, even after selection changes.
        select(primitive);
        Ui([&](auto*) {
            QVariantMap args{{"objectId", QVariant::fromValue<qulonglong>(model)}, {"model", "/Game/Models/pyramid.obj"}};
            Check(CommandRegistry::Instance().Execute("Scene.ReplaceModel", args), "real Scene.ReplaceModel accepts explicit old model ID");
        });
        pump();
        auto replaced = host.ReadSnapshot();
        Check(replaced.canUndo && replaced.selected == model, "replacement selects its target and records undo");
        bool changed = false;
        for (const auto& o : replaced.objects) if (o.id == model) changed = o.modelAsset == "/Game/Models/pyramid.obj";
        Check(changed, "replacement reached actual engine");
        auto after = save("after.scene.json");
        auto rows = after["objects"].toArray();
        for (int i = 0; i < rows.size(); ++i) {
            auto o = rows[i].toObject();
            if (o["id"].toInteger() != static_cast<qint64>(model)) continue;
            auto assets = o["assets"].toObject(); assets["model"] = QString::fromStdString(originalModel);
            o["assets"] = assets; rows[i] = o;
        }
        after["objects"] = rows;
        Check(after == before, "only model asset changed; scene format and other objects preserved");
        EditorCommand undo; undo.type = EditorCommand::Undo; host.PostCommand(undo); pump();
        Check(!host.ReadSnapshot().canUndo && host.ReadSnapshot().canRedo, "exactly one undo point");
        Check(save("undo.scene.json") == before, "undo restores complete real scene");
        select(model);
        Ui([&](auto*) {
            Check(!CommandRegistry::Instance().Execute("Scene.ReplaceModel", {{"model", "/Game/Models/pyramid.obj"}}),
                  "missing ID cannot fall back to current selection");
        });
        auto reject = [&](uint64_t id, bool primitiveExpected) {
            select(id);
            Ui([&](auto* window) {
                QString message;
                QTimer closer;
                QObject::connect(&closer, &QTimer::timeout, [&] {
                    if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                        message = box->text(); box->accept();
                    }
                });
                closer.start(10);
                Check(!CommandRegistry::Instance().Execute("Scene.ReplaceModel",
                    {{"objectId", QVariant::fromValue<qulonglong>(id)}, {"model", "/Game/Models/pyramid.obj"}}), "unsupported target rejected");
                Check(!message.isEmpty() && message.contains("C-ABI") == primitiveExpected, "C-ABI diagnostic only for primitive");
                if (primitiveExpected) {
                    auto* panel = window->template findChild<ObjectInspectorPanel*>();
                    panel->Refresh(host.ReadSnapshot(), id, true);
                    Check(!panel->findChild<QPushButton*>("ChooseModel")->isEnabled(), "primitive replacement button disabled");
                }
            });
        };
        reject(primitive, true); reject(light, false); pump();
        Check(!host.ReadSnapshot().canUndo, "rejected commands create no undo points");
        Check(Read(scenePath) == originalBytes, "original old scene file unchanged byte for byte");
        std::printf("ModelReplaceUiTests: %d checks passed.\n", checks);
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
