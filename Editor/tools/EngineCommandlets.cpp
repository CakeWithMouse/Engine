#define NOMINMAX
#include "EditorStyles.h"
#include "EditorTheme.h"
#include "GizmoMath.h"
#include "MainWindow.h"
#include "MaterialEditorWindow.h"
#include "MaterialGraphWidget.h"
#include "ObjectInspectorPanel.h"
#include "QtHost.h"
#include "UIEditorWindow.h"
#include <DbgHelp.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/InputDevice/Keys.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/MainGame/GameSession.h>
#include <Engine/Project/StandardTypes.h>
#include <Engine/Render/SkeletalLoader.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Runtime/EditorReloadApi.h>
#include <Engine/Runtime/GameEditModule.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/SaveGame/SaveGame.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/UI/UIMarkup.h>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QInputMethodEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <Windows.h>
#pragma comment(lib, "dbghelp.lib")
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include "PointShadowDiagnostics.h"
#include "TransparencyDiagnostics.h"

namespace {
std::wstring crashDumpPath;
LONG WINAPI DumpCrash(EXCEPTION_POINTERS *exception) {
  HANDLE file = CreateFileW(crashDumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file != INVALID_HANDLE_VALUE) {
    MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), exception, FALSE};
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                      MiniDumpNormal, &info, nullptr, nullptr);
    CloseHandle(file);
  }
  return EXCEPTION_EXECUTE_HANDLER;
}
QStringList warnings;
void Messages(QtMsgType type, const QMessageLogContext &,
              const QString &message) {
  if (type == QtWarningMsg || type == QtCriticalMsg)
    warnings.push_back(message);
}
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
QJsonArray Array(const glm::vec3 &v) { return {v.x, v.y, v.z}; }
glm::vec3 Vector(const QJsonValue &value, glm::vec3 fallback) {
  if (value.isUndefined())
    return fallback;
  const auto a = value.toArray();
  Require(a.size() == 3, "vector must contain three numbers");
  for (auto n : a)
    Require(n.isDouble() && std::isfinite(float(n.toDouble())),
            "vector element must be finite numeric float");
  return {float(a[0].toDouble()), float(a[1].toDouble()),
          float(a[2].toDouble())};
}
QJsonObject ProjectCommand(const QString &command, const QJsonObject &in) {
  namespace fs = std::filesystem;
  const auto project = QFileInfo(in["project"].toString()).absoluteFilePath();
  QFile file(project);
  Require(file.open(QIODevice::ReadOnly), "cannot read project");
  const auto manifest = QJsonDocument::fromJson(file.readAll()).object();
  file.close();
  const QDir root = QFileInfo(project).dir();
  const auto runtime =
      fs::path(QCoreApplication::applicationDirPath().toStdWString());
  const auto content = root.absoluteFilePath(manifest["content"].toString());
  const auto projectBytes = project.toUtf8(), contentBytes = content.toUtf8();
  HWND hwnd = CreateWindowExW(0, L"STATIC", L"Commandlet viewport",
                              WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr,
                              nullptr, GetModuleHandleW(nullptr), nullptr);
  Require(hwnd != nullptr, "hidden D3D viewport creation failed");
  struct WindowGuard {
    HWND h;
    ~WindowGuard() { DestroyWindow(h); }
  } guard{hwnd};
  GameEditModule module(
      fs::path(
          root.absoluteFilePath(manifest["module"].toString()).toStdWString()),
      runtime / "EngineContent", fs::path(content.toStdWString()));
  uint64_t scene = 0;
  char error[2048]{};
  Require(module.Api().loadScene(
              module.Api().context,
              root.absoluteFilePath(manifest["startupScene"].toString())
                  .toUtf8()
                  .constData(),
              &scene, error, sizeof(error)) == EngineSceneOk,
          error);
  struct Context {
    GameEditModule *module;
    EngineEditorReloadApi reload{};
  } context{&module};
  context.reload = {
      1, &context,
      [](void *p, const char *path, const char *checkpoint, void *h, uint32_t w,
         uint32_t height, EngineEditorReloadBindings *out, char *error,
         uint32_t cap) -> uint32_t {
        auto &m = *static_cast<Context *>(p)->module;
        auto status = m.Reload(fs::u8path(path), fs::u8path(checkpoint), h, w,
                               height, error, cap);
        if (status == EngineSceneOk)
          *out = {m.Api(), m.ProjectDataApi(), m.SelectionApi(),
                  m.ViewportInputApi()};
        return status;
      }};
  EngineModuleHost callbacks{ENGINE_MODULE_VERSION,
                             &context,
                             nullptr,
                             nullptr,
                             "Commandlet",
                             projectBytes.constData(),
                             contentBytes.constData()};
  callbacks.getService = [](void *p, uint32_t id,
                            uint32_t version) -> const void * {
    auto &c = *static_cast<Context *>(p);
    if (version != 1)
      return nullptr;
    if (id == EngineEditorReloadService)
      return &c.reload;
    if (id == EngineViewportOverlayService)
      return &c.module->ViewportOverlayApi();
    if (id == EngineViewportCursorService)
      return &c.module->ViewportCursorApi();
    if (id == EngineEditorBatchService)
      return &c.module->BatchApi();
    if (id == EngineAuthoringService)
      return &c.module->AuthoringApi();
    if (id == EnginePointLightV2Service)
      return &c.module->PointLightV2Api();
    return nullptr;
  };
  struct CommandCapture : IEditorUiListener {
    uint32_t status = EngineSceneOk;
    QString error;
    void OnLogMessage(const QString &) override {}
    void OnCommandFinished(const EditorCommand &, uint32_t value,
                           const QString &detail) override {
      status = value;
      error = detail;
    }
  } capture;
  EditorQtHost host(callbacks, [](IEditorHost *, std::atomic<bool> *ready) {
    ready->store(true);
  });
  host.SetUiListener(&capture);
  host.SetModuleApi(module.Api());
  host.SetProjectDataApi(module.ProjectDataApi());
  host.SetSelectionApi(module.SelectionApi());
  host.SetViewportInputApi(module.ViewportInputApi());
  Require(host.Start(), "host initialization failed");
  EditorCommand attach;
  attach.type = EditorCommand::ViewportAttach;
  attach.hwnd = hwnd;
  attach.w = 640;
  attach.h = 480;
  host.PostCommand(attach);
  host.Update(0);
  Require(host.ViewportActive(), "D3D viewport unavailable");
  auto tick = [&] {
    host.Update(.016);
    Require(module.Api().frame(module.Api().context, .016) == EngineSceneOk,
            "frame failed");
  };
  auto objects = [&] {
    QJsonArray out;
    EngineSceneInfo info{};
    module.Api().readActiveScene(
        module.Api().context, &info,
        [](void *p, const EngineSceneObjectInfo *o) {
          static_cast<QJsonArray *>(p)->append(
              QJsonObject{{"name", o->name},
                          {"id", double(o->id)},
                          {"parent", double(o->parentId)},
                          {"type", o->typeName},
                          {"material",o->materialAsset},
                          {"position", QJsonArray{o->transform.position[0],
                                                  o->transform.position[1],
                                                  o->transform.position[2]}}});
        },
        &out);
    const auto dataApi=module.ProjectDataApi();
    for(int i=0;i<out.size();++i) {
      auto object=out[i].toObject();
      dataApi.read(dataApi.context,uint64_t(object["id"].toDouble()),[](void* p,const char* data){
        (*static_cast<QJsonObject*>(p))["projectData"]=QJsonDocument::fromJson(QByteArray(data)).object();
      },&object);
      out[i]=object;
    }
    return out;
  };
  QJsonObject result;
  if (command == "project.scene") {
    const auto steps = in["steps"].toArray();
    Require(!steps.empty() && steps.size() <= 100,
            "1..100 scene steps required");
    auto resolve = [&](const QString &name) -> uint64_t {
      if (name.isEmpty())
        return 0;
      for (const auto value : objects()) {
        const auto object = value.toObject();
        if (object["name"].toString() == name)
          return uint64_t(object["id"].toDouble());
      }
      throw std::runtime_error(("Object not found: " + name).toStdString());
    };
    QJsonArray results;
    for (const auto value : steps) {
      Require(value.isObject(), "scene step must be an object");
      const auto step = value.toObject();
      const auto action = step["action"].toString();
      EditorCommand cmd;
      cmd.objectId = resolve(step["target"].toString());
      cmd.name = step["name"].toString();
      cmd.path = step["path"].toString();
      if (action == "create") {
        cmd.type = EditorCommand::CreateObject;
        cmd.path = step["type"].toString();
        Require(!cmd.path.isEmpty(), "create requires type");
      } else if (action == "model") {
        cmd.type=EditorCommand::CreateModel;
        Require(cmd.path.startsWith("/Game/"),"model requires a /Game asset path");
      } else if (action == "delete")
        cmd.type = EditorCommand::RemoveObject;
      else if (action == "duplicate")
        cmd.type = EditorCommand::DuplicateObject;
      else if (action == "rename")
        cmd.type = EditorCommand::RenameObject;
      else if (action == "parent") {
        cmd.type = EditorCommand::SetParent;
        cmd.parentId = resolve(step["parent"].toString());
      } else if (action == "select") {
        cmd.type = EditorCommand::SelectObject;
        for (const auto name : step["targets"].toArray())
          cmd.objectIds.push_back(resolve(name.toString()));
        if (!cmd.objectId && !cmd.objectIds.empty())
          cmd.objectId = cmd.objectIds.front();
      } else if (action == "move") {
        cmd.type = EditorCommand::UpdateObjectTransform;
        cmd.path = "nudge";
        const auto delta = Vector(step["delta"], {});
        cmd.right = delta.x;
        cmd.up = delta.y;
        cmd.forward = delta.z;
      } else if (action == "property") {
        cmd.type = EditorCommand::SetProperty;
        const auto bytes = QJsonDocument(QJsonArray{step["value"]})
                               .toJson(QJsonDocument::Compact);
        cmd.name = QString::fromUtf8(bytes.mid(1, bytes.size() - 2));
      } else if (action == "material") {
        cmd.type = EditorCommand::SetMaterial;
      } else if (action == "save")
        cmd.type = EditorCommand::SaveScene;
      else if (action == "load")
        cmd.type = EditorCommand::LoadScene;
      else if (action == "new")
        cmd.type = EditorCommand::NewScene;
      else if (action == "undo")
        cmd.type = EditorCommand::Undo;
      else if (action == "redo")
        cmd.type = EditorCommand::Redo;
      else
        throw std::runtime_error("unknown project.scene action");
      capture.status = EngineSceneFailure;
      capture.error.clear();
      host.PostCommand(cmd);
      host.Update(0);
      const auto snapshot = host.ReadSnapshot();
      QJsonArray selected;
      for (auto id : snapshot.selectedIds)
        selected.append(double(id));
      results.append(QJsonObject{{"action", action},
                                 {"status", int(capture.status)},
                                 {"error", capture.error},
                                 {"objects", objects()},
                                 {"selected", selected},
                                 {"canUndo", snapshot.canUndo},
                                 {"canRedo", snapshot.canRedo}});
    }
    result["steps"] = results;
  } else if (command == "material.graph") {
    EditorTheme::Apply(
        *static_cast<QApplication *>(QCoreApplication::instance()));
    const auto material = in["material"].toString();
    Require(material.startsWith("/Game/"),
            "material must be a /Game path in the scratch project");
    QStringList dialogs;
    QTimer dismiss;
    QObject::connect(&dismiss, &QTimer::timeout, [&] {
      for (auto *w : QApplication::topLevelWidgets())
        if (auto *box = qobject_cast<QMessageBox *>(w)) {
          dialogs << box->text();
          auto *button =
              box->button(box->standardButtons().testFlag(QMessageBox::Yes)
                              ? QMessageBox::Yes
                              : QMessageBox::Ok);
          if (button)
            button->click();
        }
    });
    dismiss.start(10);
    QPointer<MaterialEditorWindow> editor =
        new MaterialEditorWindow(&host, material);
    struct EditorGuard {
      QPointer<MaterialEditorWindow> &ptr;
      ~EditorGuard() {
        if (ptr)
          delete ptr.data();
      }
    } editorGuard{editor};
    editor->setAttribute(Qt::WA_DontShowOnScreen);
    editor->resize(1000, 720);
    editor->show();
    auto pump = [&] {
      QElapsedTimer timer;
      timer.start();
      qint64 nextFrame = 0;
      do {
        host.Update(0);
        QApplication::processEvents();
        if (timer.elapsed() >= nextFrame) {
          Require(module.Api().frame(module.Api().context, .016) ==
                      EngineSceneOk,
                  "material preview frame failed");
          nextFrame = timer.elapsed() + 16;
        }
        QThread::msleep(1);
      } while (timer.elapsed() < 400);
    };
    pump();
    auto *preview = editor->findChild<QWidget *>("MaterialPreview");
    Require(preview && IsWindow(reinterpret_cast<HWND>(preview->winId())),
            "material preview requires a real native HWND");
    result["nativePreviewWindow"] = true;
    QJsonArray results;
    const auto steps = in["steps"].toArray();
    Require(!steps.empty() && steps.size() <= 100,
            "1..100 graph steps required");
    for (const auto value : steps) {
      const auto step = value.toObject();
      const auto action = step["action"].toString();
      HANDLE lock = INVALID_HANDLE_VALUE;
      const auto lockedFile = step["lockedFile"].toString();
      if (!lockedFile.isEmpty()) {
        Require(QFileInfo(lockedFile).isAbsolute(),
                "lockedFile must be absolute");
        lock = CreateFileW(reinterpret_cast<LPCWSTR>(lockedFile.utf16()),
                           GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(lock != INVALID_HANDLE_VALUE,
                "cannot acquire write-failure lock");
      }
      struct LockGuard {
        HANDLE h;
        ~LockGuard() {
          if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
        }
      } lockGuard{lock};
      bool accepted = true;
      if (action == "enable")
        editor->EnableGraph();
      else if (action == "disable")
        editor->DisableGraph();
      else if (action == "apply")
        editor->ApplyGraph();
      else if (action == "revert")
        editor->Revert();
      else if (action == "save") {
        bool done = false, success = false;
        accepted = editor->Save([&](bool ok) {
          done = true;
          success = ok;
        });
        if (accepted) {
          QElapsedTimer timer;
          timer.start();
          while (!done && timer.elapsed() < 15000)
            pump();
          Require(done, "material save timed out");
          accepted = success;
        }
      } else {
        Require(editor->HasGraph() && editor->Graph(), "graph is not enabled");
        auto *graph = editor->Graph();
        if (action == "preset") {
          const int index = step["index"].toInt(-1);
          Require(index >= 0 && index < graph->PresetNames().size(),
                  "invalid preset index");
          graph->LoadPreset(index);
        } else if (action == "ai")
          accepted = graph->PasteFromAi(step["text"].toString());
        else if (action == "undo")
          graph->Undo();
        else if (action == "redo")
          graph->Redo();
        else if (action == "remove") {
          QList<int> ids;
          for (const auto id : step["ids"].toArray())
            ids.append(id.toInt());
          graph->RemoveNodes(ids);
        } else
          throw std::runtime_error("unknown material.graph action");
      }
      pump();
      QJsonObject state{{"action", action},
                        {"accepted", accepted},
                        {"hasGraph", editor->HasGraph()},
                        {"dirty", editor->HasUnsavedChanges()}};
      if (editor->HasGraph())
        state["graph"] = QJsonDocument::fromJson(
                             editor->Graph()->Model().ExportToJson().toUtf8())
                             .object();
      if (auto *source = editor->findChild<QPlainTextEdit *>("MaterialSource"))
        state["hlsl"] = source->toPlainText();
      results.append(state);
    }
    const auto image = in["image"].toString();
    if (!image.isEmpty())
      Require(editor->grab().save(image), "graph image write failed");
    result["steps"] = results;
    result["dialogs"] = QJsonArray::fromStringList(dialogs);
    Require(editor->Shutdown(), "material shutdown rollback failed");
    pump();
  } else if (command == "project.input") {
    const auto steps = in["steps"].toArray();
    Require(!steps.empty() && steps.size() <= 100,
            "steps must contain 1..100 input/control/frame operations");
    int totalFrames = 0;
    QJsonArray results;
    for (const auto value : steps) {
      Require(value.isObject(), "step must be an object");
      const auto step = value.toObject();
      const auto kind = step["kind"].toString();
      if (kind == "control") {
        const auto action = step["action"].toString();
        const int control = action == "play"     ? EnginePlay
                            : action == "stop"   ? EngineStop
                            : action == "pause"  ? EnginePause
                            : action == "resume" ? EngineResume
                                                 : -1;
        Require(control >= 0, "unknown play control");
        Require(module.Api().control(module.Api().context, control) ==
                    EngineSceneOk,
                "play control failed");
      } else if (kind == "frames") {
        const int count = step["count"].toInt(-1);
        totalFrames += count;
        Require(count >= 0 && count <= 600 && totalFrames <= 2000,
                "bounded frame count required");
        for (int i = 0; i < count; ++i)
          tick();
      } else if (kind == "key" || kind == "focus") {
        EditorCommand input;
        input.type =
            kind == "key" ? EditorCommand::InputKey : EditorCommand::InputFocus;
        if (kind == "key") {
          const int key = step["key"].toInt(-1);
          Require(key >= 0 && key <= 507 && step["down"].isBool(),
                  "Engine Keys 0..507 and boolean down required");
          input.w = key;
          input.h = step["down"].toBool() ? 1 : 0;
        } else {
          Require(step["enabled"].isBool(), "boolean focus enabled required");
          input.w = step["enabled"].toBool() ? 1 : 0;
        }
        host.PostCommand(input);
        host.Update(0);
      } else if (kind == "pointer") {
        const auto delta = Vector(step["delta"], {});
        EditorCommand input;
        input.type = EditorCommand::InputPointer;
        input.yaw = delta.x;
        input.pitch = delta.y;
        input.zoom = delta.z;
        host.PostCommand(input);
        host.Update(0);
      } else if (kind == "cursor") {
        auto cursor = module.ViewportCursorApi();
        Require(cursor.set, "cursor API unavailable");
        Require(cursor.set(cursor.context, float(step["x"].toDouble()),
                           float(step["y"].toDouble()), 640, 480,
                           step["valid"].toBool(true) ? 1 : 0) == EngineSceneOk,
                "cursor update failed");
      } else
        throw std::runtime_error("unknown project.input step");
      results.append(QJsonObject{{"kind", kind}, {"objects", objects()}});
    }
    result["steps"] = results;
    result["frames"] = totalFrames;
    module.Api().control(module.Api().context, EngineStop);
  } else if (command == "project.reload-input") {
    EditorCommand focus;
    focus.type = EditorCommand::InputFocus;
    focus.w = 1;
    host.PostCommand(focus);
    host.Update(0);
    auto exercise = [&] {
      Require(module.Api().control(module.Api().context, EnginePlay) ==
                  EngineSceneOk,
              "Play failed");
      for (int i = 0; i < 3; ++i)
        tick();
      QJsonObject state{{"before", objects()}};
      EditorCommand key;
      key.type = EditorCommand::InputKey;
      key.w = in["key"].toInt(84);
      key.h = 1;
      host.PostCommand(key);
      tick();
      key.h = 0;
      host.PostCommand(key);
      for (int i = 0; i < 4; ++i)
        tick();
      state["afterKey"] = objects();
      if (in.contains("clickX")) {
        auto cursor = module.ViewportCursorApi();
        Require(cursor.set, "cursor API unavailable");
        cursor.set(cursor.context, float(in["clickX"].toDouble()),
                   float(in["clickY"].toDouble()), 640, 480, 1);
        key.w = static_cast<uint32_t>(Keys::LeftButton);
        key.h = 1;
        host.PostCommand(key);
        tick();
        key.h = 0;
        host.PostCommand(key);
        for (int i = 0; i < 3; ++i)
          tick();
        state["afterClick"] = objects();
      }
      Require(module.Api().control(module.Api().context, EngineStop) ==
                  EngineSceneOk,
              "Stop failed");
      host.Update(0);
      return state;
    };
    result["original"] = exercise();
    const auto old = module.Api().context;
    EditorCommand reload;
    reload.type = EditorCommand::ReloadProject;
    reload.path = in["candidate"].toString();
    Require(!reload.path.isEmpty(), "candidate DLL is required");
    host.PostCommand(reload);
    host.Update(0);
    Require(capture.status == EngineSceneOk,
            capture.error.isEmpty() ? "Reload command rejected"
                                    : capture.error.toUtf8().constData());
    Require(module.Api().context != old, "candidate did not replace session");
    result["reloaded"] = exercise();
    result["replaced"] = true;
  } else {
    EditorMainWindow window(&host);
    host.SetUiListener(&window);
    QStringList dialogs;
    QTimer dismiss;
    QObject::connect(&dismiss, &QTimer::timeout, [&] {
      for (auto *w : QApplication::topLevelWidgets())
        if (auto *box = qobject_cast<QMessageBox *>(w)) {
          dialogs << box->text();
          box->accept();
        }
    });
    dismiss.start(20);
    QAction *build = nullptr;
    for (auto *a : window.findChildren<QAction *>())
      if (a->shortcut() == QKeySequence("Ctrl+Alt+R"))
        build = a;
    Require(build, "build action missing");
    const auto old = module.Api().context;
    build->trigger();
    Require(window.isEnabled(), "build disabled entire main window");
    QElapsedTimer timer;
    timer.start();
    bool busy = false;
    do {
      QApplication::processEvents();
      host.Update(.016);
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      busy = window.findChild<QProcess *>("projectCodeBuild") != nullptr;
      Require(timer.elapsed() < 180000, "build timed out");
      Sleep(10);
    } while (busy);
    for (int i = 0; i < 4; ++i) {
      QApplication::processEvents();
      host.Update(.016);
    }
    result["dialogs"] = QJsonArray::fromStringList(dialogs);
    result["windowEnabled"] = window.isEnabled();
    result["replaced"] = module.Api().context != old;
    auto *console = window.findChild<QPlainTextEdit *>("Console");
    Require(console, "console missing");
    result["console"] = console->toPlainText();
    result["buildSucceeded"] = dialogs.isEmpty();
    host.SetUiListener(nullptr);
  }
  host.SetModuleApi({});
  host.Stop();
  return result;
}
QJsonObject Run(const QString &command, const QJsonObject &in) {
  if (command == "render.point-shadows") return PointShadowDiagnostics::Run(in);
  if (command == "render.transparency") return TransparencyDiagnostics::Run(in);
  if (command == "render.diagnostics") {
    const int width = in["width"].toInt(640), height = in["height"].toInt(480);
    Require(width >= 100 && width <= 1920 && height >= 100 && height <= 1080,
            "viewport must be 100..1920 by 100..1080");
    const auto runtime = std::filesystem::path(
        QCoreApplication::applicationDirPath().toStdWString());
    EnginePaths::SetContentRoot(runtime / "EngineContent");
    HWND hwnd = CreateWindowExW(
        0, L"STATIC", L"Render diagnostics commandlet", WS_OVERLAPPEDWINDOW, 0,
        0, width, height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Require(hwnd != nullptr, "viewport creation failed");
    struct WindowGuard {
      HWND h;
      ~WindowGuard() { DestroyWindow(h); }
    } guard{hwnd};
    Game game;
    Require(game.InitializeViewport(hwnd, width, height),
            "D3D viewport unavailable");
    game.SetSimulationEnabled(false);
    game.GetSettings().SetVSync(false);
    const int rendering = in["rendering"].toInt(0);
    Require(rendering >= 0 && rendering <= 2, "rendering must be 0..2");
    game.GetSettings().SetRenderingType(static_cast<RenderType>(rendering));
    game.SetRenderDiagnosticsVisible(in["panel"].toBool(true));
    auto mesh = std::make_unique<InstancedMeshComponent>();
    auto *ptr = mesh.get();
    const int lodCount = in["lodCount"].toInt(3);
    Require(lodCount >= 1 && lodCount <= 4, "lodCount must be 1..4");
    for (int level = 0; level < lodCount; ++level)
      Require(ptr->SetLod(level, MeshPrimitives::Cube()),
              "LOD geometry rejected");
    auto floats = [](const QJsonValue &value) {
      std::vector<float> result;
      Require(value.isArray(), "threshold array required");
      for (const auto n : value.toArray()) {
        Require(n.isDouble() && n.toDouble() > 0 &&
                    std::isfinite(float(n.toDouble())),
                "finite positive numeric threshold required");
        result.push_back(float(n.toDouble()));
      }
      return result;
    };
    const auto distances = in.contains("distances") ? floats(in["distances"])
                                                    : std::vector<float>{9, 17};
    Require(distances.size() <= 4, "too many distance thresholds");
    ptr->SetLodDistances(distances);
    if (in.contains("screenHeights")) {
      const auto values = floats(in["screenHeights"]);
      Require(values.size() <= 4, "too many screen thresholds");
      ptr->SetLodScreenHeights(values);
    }
    const float limit = float(in["drawDistance"].toDouble(60));
    Require(std::isfinite(limit) && limit >= 0,
            "drawDistance must be finite and nonnegative");
    ptr->SetDrawDistance(limit);
    const float fade = float(in["fade"].toDouble(0));
    Require(fade >= 0 && fade <= 1, "fade must be 0..1");
    ptr->SetLodFadeFraction(fade);
    const auto instances = in["instances"].toArray();
    Require(instances.size() <= 10000, "at most 10000 instances");
    for (const auto value : instances) {
      Require(value.isObject(), "instance must be an object");
      const auto object = value.toObject();
      MeshInstance item;
      item.Position = Vector(object["position"], {});
      item.Scale = float(object["scale"].toDouble(1));
      Require(std::isfinite(item.Scale) && item.Scale > 0,
              "finite positive scale required");
      item.bVisible = object["visible"].toBool(true);
      ptr->AddInstance(item);
    }
    std::unique_ptr<GameComponent> owner = std::move(mesh);
    game.GetScene().AddComponent("Diagnostics instances", owner);
    auto views = in["views"].toArray();
    if (views.empty())
      views.append(QJsonObject{{"position", QJsonArray{0, 24, 0}}});
    Require(views.size() <= 32, "at most 32 views");
    QJsonArray frames;
    for (int index = 0; index < views.size(); ++index) {
      Require(views[index].isObject(), "view must be object");
      const auto view = views[index].toObject();
      auto *camera = game.GetPlayer();
      camera->SetCameraMode(CameraMode::Free);
      camera->SetPosition(Vector(view["position"], {0, 24, 0}));
      const auto direction = Vector(view["direction"], {0, -1, 0}),
                 up = Vector(view["up"], {0, 0, 1});
      Require(glm::length(direction) > 0 && glm::length(up) > 0 &&
                  glm::length(glm::cross(glm::normalize(direction),
                                         glm::normalize(up))) > .001f,
              "camera direction and up must be nonzero and nonparallel");
      camera->SetViewDirection(direction, up);
      const float fov = float(view["fov"].toDouble(60)),
                  orthoHeight = float(view["orthoHeight"].toDouble(24));
      Require(fov > 0 && fov < 180 && std::isfinite(orthoHeight) &&
                  orthoHeight > 0,
              "valid camera projection required");
      camera->SetProjection(view["orthographic"].toBool(true), fov, .1f, 2000,
                            orthoHeight);
      const auto image = in["image"].toString();
      if (index == views.size() - 1 && !image.isEmpty()) {
        Require(
            QFileInfo(image).isAbsolute() && !QFileInfo::exists(image) &&
                QFileInfo(image).dir().exists(),
            "image must be a new absolute BMP path with an existing parent");
        game.RequestFrameDump(image.toUtf8().toStdString());
      }
      const bool previous = game.GetCompletedFrameStats().valid;
      Require(game.RenderViewportFrame(0), "render frame failed");
      const auto &frame = game.GetCompletedFrameStats();
      const auto &d = frame.lodComponents.at(0);
      if (index == views.size() - 1 && !image.isEmpty())
        Require(QFileInfo(image).size() > 54, "frame dump was not written");
      QJsonArray buckets, thresholds, lines;
      for (auto count : d.perLod)
        buckets.append(int(count));
      for (auto value : d.thresholds)
        thresholds.append(value);
      for (const auto &line : game.GetRenderDiagnosticsLines())
        lines.append(QString::fromStdString(line));
      frames.append(QJsonObject{
          {"valid", frame.valid},
          {"previousValid", previous},
          {"frameIndex", double(frame.frameIndex)},
          {"drawCalls", int(frame.counters.DrawCalls)},
          {"instancesSubmitted", int(frame.counters.InstancesDrawn)},
          {"prepared", d.prepared},
          {"total", int(d.total)},
          {"drawn", int(d.drawn)},
          {"hidden", int(d.hidden)},
          {"distanceCulled", int(d.distanceCulled)},
          {"frustumCulled", int(d.frustumCulled)},
          {"minDistance", d.minDistance},
          {"maxDistance", d.maxDistance},
          {"screenSize", d.screenSize},
          {"perLod", buckets},
          {"thresholds", thresholds},
          {"lines", lines}});
    }
    return {{"frames", frames}};
  }
  if (command.startsWith("savegame.")) {
    Game game;
    game.Saves().SetCatalog(SceneProject::GetComponentTypes());
    const auto root = in["root"].toString();
    Require(!root.isEmpty(),
            "savegame commands require an explicit scratch root");
    game.Saves().Configure(in["gameId"].toString("commandlet").toStdString(),
                           std::filesystem::path(root.toStdWString()));
    const auto slot = in["slot"].toString("manual-1").toStdString();
    if (command == "savegame.list") {
      QJsonArray slotNames;
      for (const auto &name : game.Saves().ListSlots())
        slotNames.append(QString::fromStdString(name));
      return {{"slots", slotNames}};
    }
    if (command == "savegame.delete") {
      game.Saves().DeleteSlot(slot);
      return {{"deleted", QString::fromStdString(slot)}};
    }
    if (command == "savegame.save") {
      Require(in["scene"].isObject(), "scene document required");
      auto data = SceneDocument::Parse(
          QJsonDocument(in["scene"].toObject()).toJson().toStdString());
      SceneDocument::Restore(data, SceneProject::GetComponentTypes(),
                             game.GetScene());
      game.Saves().RequestSave(slot);
      game.Saves().ProcessPending();
      Require(game.Saves().LastResult().Succeeded(),
              game.Saves().LastResult().message.c_str());
    } else if (command == "savegame.load" || command == "savegame.validate") {
      game.Saves().RequestLoad(slot);
      game.Saves().ProcessPending();
      Require(game.Saves().LastResult().Succeeded(),
              game.Saves().LastResult().message.c_str());
    } else
      throw std::runtime_error("unknown savegame command");
    return {{"state", QJsonDocument::fromJson(
                          QByteArray::fromStdString(game.Saves().Capture()))
                          .object()}};
  }
  if (command == "asset.import") {
    namespace fs = std::filesystem;
    const auto source = QFileInfo(in["source"].toString()).absoluteFilePath();
    Require(in["source"].isString() && QFileInfo::exists(source),
            "source file is required");
    Require(in["content"].isString() && !in["content"].toString().isEmpty(),
            "content output directory is required");
    const auto content =
        fs::absolute(fs::path(in["content"].toString().toStdWString()));
    fs::create_directories(content);
    const auto runtime =
        fs::path(QCoreApplication::applicationDirPath().toStdWString());
    GameEditModule module(runtime / "Templates/SceneProject/Game.dll",
                          runtime / "EngineContent", content);
    const auto api = module.Api();
    char error[2048]{}, path[4096]{};
    const auto status =
        api.importAsset(api.context, source.toUtf8().constData(), 0, path,
                        sizeof(path), error, sizeof(error));
    Require(status == EngineSceneOk, error);
    uint64_t object = 0;
    Require(api.createModel(api.context, path, "Imported", nullptr, &object,
                            error, sizeof(error)) == EngineSceneOk,
            error);
    QJsonObject out{{"asset", QString::fromUtf8(path)},
                    {"placed", object != 0}};
    EngineSceneInfo scene{};
    api.readActiveScene(
        api.context, &scene,
        [](void *context, const EngineSceneObjectInfo *info) {
          auto &result = *static_cast<QJsonObject *>(context);
          result["material"] = QString::fromUtf8(info->materialAsset);
          result["type"] = QString::fromUtf8(info->typeName);
        },
        &out);
    const auto dataApi=module.ProjectDataApi();
    Require(dataApi.read(dataApi.context,object,[](void* p,const char* data){
        (*static_cast<QJsonObject*>(p))["materialSlots"]=QJsonDocument::fromJson(QByteArray(data)).object().value("materialSlots");
    },&out)==EngineSceneOk,"cannot read material slots");
    // Inspect the asset that the engine actually created. BLEND is converted to
    // GLB by Blender; feeding the original file to Assimp reports a false
    // failure.
    const auto asset = QString::fromUtf8(path);
    Require(asset.startsWith("/Game/"),
            "import returned an invalid asset path");
    const auto physical = content / fs::u8path(std::string(path).substr(6));
    QFile bytes(QString::fromStdWString(physical.wstring()));
    Require(bytes.open(QIODevice::ReadOnly),
            "cannot read imported model bytes");
    const auto data = bytes.readAll();
    auto model = LoadSkeletalModelFromMemory(
        data.constData(), size_t(data.size()),
        QFileInfo(bytes.fileName()).suffix().toStdString());
    if (model.Success && model.Model) {
      out["meshes"] = int(model.Model->Meshes.size());
      out["indices"] = model.Model->TotalIndexCount;
      out["clips"] = int(model.Model->Clips.size());
    } else
      out["memoryLoadError"] = QString::fromStdString(model.ErrorMessage);
    return out;
  }
  if (command.startsWith("ui."))
    EditorTheme::Apply(
        *static_cast<QApplication *>(QCoreApplication::instance()));
  if (command == "mesh.tangents") {
    Require(in["vertices"].isArray() && in["indices"].isArray(), "vertices and indices must be arrays");
    const auto vertices = in["vertices"].toArray();
    const auto indices = in["indices"].toArray();
    Require(!vertices.isEmpty() && vertices.size() <= 100000 && indices.size() <= 300000 &&
            indices.size() % 3 == 0, "invalid mesh size");
    MeshLodData mesh;
    for (const auto value : vertices) {
      Require(value.isObject(), "vertex must be an object");
      const auto vertex = value.toObject();
      Require(vertex.contains("position") && vertex.contains("normal") && vertex["uv"].isArray(),
              "vertex requires position, normal and uv");
      const auto p = Vector(vertex["position"], {});
      const auto n = Vector(vertex["normal"], {});
      const auto uv = vertex["uv"].toArray();
      Require(uv.size() == 2 && uv[0].isDouble() && uv[1].isDouble() &&
              std::isfinite(float(uv[0].toDouble())) && std::isfinite(float(uv[1].toDouble())),
              "uv must contain two finite numbers");
      MeshVertex v;
      v.Position = {p.x, p.y, p.z, 1};
      v.Normal = {n.x, n.y, n.z, 0};
      v.TexCoord = {float(uv[0].toDouble()), float(uv[1].toDouble()), 0, 0};
      mesh.Vertices.push_back(v);
    }
    for (const auto value : indices) {
      const double index = value.toDouble(-1);
      Require(value.isDouble() && index >= 0 && index < vertices.size() && std::floor(index) == index,
              "index must be an integer within vertices");
      mesh.Indices.push_back(static_cast<uint32_t>(index));
    }
    GenerateTangents(mesh);
    QJsonArray tangents;
    for (const auto& vertex : mesh.Vertices) {
      const auto& t = vertex.Tangent;
      tangents.push_back(QJsonArray{t.x, t.y, t.z, t.w});
    }
    return {{"tangents", tangents}};
  }
  if (command == "game.settings") {
    Game game;
    auto &settings = game.GetSettings();
    const int rendering = in["rendering"].toInt(0);
    Require(rendering >= 0 && rendering <= 2, "rendering must be 0, 1 or 2");
    settings.SetRenderingType(static_cast<RenderType>(rendering));
    settings.SetVSync(in["vsync"].toBool(true));
    settings.SetAntialiasing(in["antialiasing"].toInt(0));
    return {{"rendering", int(settings.GetRenderingType())},
            {"vsync", settings.IsVSyncEnabled()},
            {"antialiasing", int(settings.GetAntialiasing())}};
  }
  if (command == "mesh.primitive") {
    const auto shape = in["shape"].toString("cube");
    const float size = float(in["size"].toDouble(1));
    const int slices = in["slices"].toInt(16);
    Require(size > 0 && slices >= 3 && slices <= 512,
            "positive size and 3..512 slices required");
    MeshLodData mesh;
    if (shape == "cube")
      mesh = MeshPrimitives::Cube(size);
    else if (shape == "sphere")
      mesh = MeshPrimitives::Sphere(size / 2, slices, slices);
    else if (shape == "plane")
      mesh = MeshPrimitives::Plane(size, size, 1);
    else if (shape == "cylinder")
      mesh = MeshPrimitives::Cylinder(size / 2, size, slices);
    else if (shape == "cone")
      mesh = MeshPrimitives::Cone(size / 2, size, slices);
    else if (shape == "pyramid")
      mesh = MeshPrimitives::Pyramid(size, size);
    else
      throw std::runtime_error("unknown primitive");
    glm::vec3 minimum(FLT_MAX), maximum(-FLT_MAX);
    for (const auto &v : mesh.Vertices) {
      const glm::vec3 p(v.Position.x, v.Position.y, v.Position.z);
      minimum = glm::min(minimum, p);
      maximum = glm::max(maximum, p);
    }
    return {{"vertices", int(mesh.Vertices.size())},
            {"triangles", int(mesh.Indices.size() / 3)},
            {"minimum", Array(minimum)},
            {"maximum", Array(maximum)}};
  }
  if (command == "project.build" || command == "project.reload-input" ||
      command == "project.input" || command == "material.graph" ||
      command == "project.scene")
    return ProjectCommand(command, in);
  if (command == "scene.property") {
    SceneEditSession session(SceneProject::GetComponentTypes());
    auto api = session.GetApi();
    auto author = session.GetAuthoringApi();
    auto data = session.GetProjectDataApi();
    EngineSceneTransform transform{};
    transform.rotation[3] = 1;
    for (auto &s : transform.scale)
      s = 1;
    uint64_t id = 0;
    Require(api.scene.createObject(
                api.scene.context,
                in["type"].toString("Gameplay.Camera").toUtf8().constData(),
                "probe", &transform, &id) == EngineSceneOk,
            "object creation failed");
    std::string schema;
    Require(
        author.describe(
            author.context, id,
            [](void *p, const char *s) { *static_cast<std::string *>(p) = s; },
            &schema) == EngineSceneOk,
        "schema unavailable");
    uint32_t status = EngineSceneOk;
    char error[2048]{};
    if (in.contains("path")) {
      QJsonArray wrapper{in["value"]};
      auto value = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
      value = value.mid(1, value.size() - 2);
      status = author.setProperty(author.context, id,
                                  in["path"].toString().toUtf8().constData(),
                                  value.constData(), error, sizeof(error));
    }
    std::string json;
    Require(
        data.read(
            data.context, id,
            [](void *p, const char *s) { *static_cast<std::string *>(p) = s; },
            &json) == EngineSceneOk,
        "property data unavailable");
    return {
        {"status", int(status)},
        {"error", QString::fromUtf8(error)},
        {"schema",
         QJsonDocument::fromJson(QByteArray::fromStdString(schema)).object()},
        {"data",
         QJsonDocument::fromJson(QByteArray::fromStdString(json)).object()}};
  }
  if (command == "ui.style") {
    const auto qss = qApp->styleSheet();
    QWidget window;
    QDoubleSpinBox spin(&window);
    QComboBox combo(&window);
    spin.setGeometry(12, 12, 160, 28);
    combo.addItem(QString::fromUtf8("Камера"));
    combo.setGeometry(12, 50, 160, 28);
    window.resize(240, 100);
    window.show();
    QApplication::processEvents();
    const auto image = in["image"].toString();
    if (!image.isEmpty())
      Require(window.grab().save(image), "save render failed");
    return {{"warnings", QJsonArray::fromStringList(warnings)}, {"style", qss}};
  }
  if (command == "ui.inspector") {
    ObjectInspectorPanel panel;
    panel.resize(in["width"].toInt(310), in["height"].toInt(600));
    EditorQtHost::Snapshot snapshot;
    EditorQtHost::SceneObject object;
    object.id = 1;
    object.name = "probe";
    object.type = "Primitive.Cube";
    object.transform.scale[0] = object.transform.scale[1] =
        object.transform.scale[2] = 1;
    snapshot.objects.push_back(object);
    snapshot.selected = 1;
    panel.Refresh(snapshot, 1, true);
    panel.show();
    QApplication::processEvents();
    auto *scroll = panel.findChild<QScrollArea *>();
    Require(scroll, "missing inspector scroll area");
    QJsonArray fields;
    bool reachable = true;
    for (auto *field : panel.findChildren<QDoubleSpinBox *>())
      if (field->objectName().startsWith("Transform")) {
        scroll->ensureWidgetVisible(field, 0, 0);
        QApplication::processEvents();
        const auto rect =
            QRect(field->mapTo(scroll->viewport(), QPoint()), field->size());
        const bool visible = scroll->viewport()->rect().contains(rect);
        reachable &= visible;
        fields.append(
            QJsonObject{{"name", field->objectName()},
                        {"reachable", visible},
                        {"width", field->width()},
                        {"left", rect.left()},
                        {"right", rect.right()},
                        {"scroll", scroll->horizontalScrollBar()->value()}});
      }
    const auto image = in["image"].toString();
    if (!image.isEmpty())
      Require(panel.grab().save(image), "save render failed");
    return {{"fields", fields},
            {"reachable", reachable},
            {"horizontalRange", scroll->horizontalScrollBar()->maximum()}};
  }
  if (command == "ui.edit-text") {
    const auto path = in["file"].toString();
    Require(!path.isEmpty(), "file is required (modified in place)");
    UIEditorWindow editor(path);
    Require(editor.Root(), "cannot load UI file");
    auto *element = editor.Root()->Find(in["element"].toString().toStdString());
    Require(element, "element not found");
    editor.Select(element);
    auto *text = editor.findChild<QLineEdit *>("UIText");
    Require(text, "text field not found");
    editor.show();
    text->setFocus();
    QApplication::processEvents();
    text->selectAll();
    QInputMethodEvent input;
    input.setCommitString(in["text"].toString());
    QApplication::sendEvent(text, &input);
    if (in["close"].toString() == "cancel") {
      QTimer::singleShot(0, [] {
        for (auto *w : QApplication::topLevelWidgets())
          if (auto *box = qobject_cast<QMessageBox *>(w))
            box->done(QMessageBox::Cancel);
      });
      return {{"closeAccepted", editor.ConfirmClose()},
              {"pending", text->text()}};
    }
    // Deliberately no editingFinished/focus change: Save must flush pending
    // edits.
    Require(editor.Save(), "save failed");
    QFile file(path);
    Require(file.open(QIODevice::ReadOnly), "read saved UI failed");
    return {{"saved", QString::fromUtf8(file.readAll())}};
  }
  if (command == "input.rebind") {
    struct Probe {
      int focus = -1;
      int calls = 0;
    } old, next;
    auto api = [](Probe &p) {
      EngineViewportInputApi a{};
      a.version = 1;
      a.context = &p;
      a.focus = [](void *p, uint32_t f) -> uint32_t {
        auto &v = *static_cast<Probe *>(p);
        v.focus = f;
        ++v.calls;
        return EngineSceneOk;
      };
      return a;
    };
    EngineModuleHost hostInfo{};
    EditorQtHost host(hostInfo);
    host.SetViewportInputApi(api(old));
    EditorCommand focus;
    focus.type = EditorCommand::InputFocus;
    focus.w = in["focused"].toBool(true) ? 1 : 0;
    host.PostCommand(focus);
    host.Update(0);
    host.SetViewportInputApi(api(next));
    return {{"oldFocus", old.focus},
            {"newFocus", next.focus},
            {"newCalls", next.calls}};
  }
  if (command == "trace.raycast") {
    Scene scene;
    auto mesh = std::make_unique<MeshComponent>();
    MeshLodData data;
    for (auto point :
         {glm::vec3(-1, -1, 0), glm::vec3(1, -1, 0), glm::vec3(0, 1, 0)}) {
      MeshVertex v{};
      v.Position = {point.x, point.y, point.z, 1};
      data.Vertices.push_back(v);
    }
    data.Indices = {0, 1, 2};
    mesh->SetMesh(data);
    mesh->SetPosition(Vector(in["position"], {0, 0, 0}));
    std::unique_ptr<GameComponent> component = std::move(mesh);
    scene.AddComponent("triangle", component);
    auto result = scene.Raycast({Vector(in["origin"], {0, 0, 5}),
                                 Vector(in["direction"], {0, 0, -1}),
                                 float(in["distance"].toDouble(10))});
    QJsonObject out{{"hit", bool(result)}};
    if (result) {
      out["distance"] = result.hit->distance;
      out["position"] = Array(result.hit->position);
      out["normal"] = Array(result.hit->normal);
    }
    return out;
  }
  if (command == "gizmo.snap") {
    auto delta = Vector(in["delta"], {});
    double source[] = {delta.x, delta.y, delta.z}, out[3]{};
    GizmoMath::SnapVector(source, in["step"].toDouble(.5), out);
    return {{"delta", QJsonArray{out[0], out[1], out[2]}}};
  }
  if (command == "gizmo.center") {
    auto delta = Vector(in["delta"], {}), position = Vector(in["position"], {});
    double source[] = {delta.x, delta.y, delta.z};
    EngineSceneTransform start{};
    start.rotation[3] = 1;
    for (int i = 0; i < 3; ++i)
      start.position[i] = position[i];
    auto rotation = in["rotation"].toArray();
    if (!rotation.isEmpty()) {
      Require(rotation.size() == 4, "rotation must be a quaternion");
      for (int i = 0; i < 4; ++i)
        start.rotation[i] = float(rotation[i].toDouble());
    }
    const auto result = GizmoMath::ApplyCenterTranslation(
        start, source, in["snap"].toBool(true), in["step"].toDouble(.5),
        in["local"].toBool(false));
    return {{"position", QJsonArray{result.position[0], result.position[1],
                                    result.position[2]}}};
  }
  if (command == "camera.screen-ray") {
    using namespace DirectX;
    const auto origin = Vector(in["origin"], {0, 0, 5});
    const auto direction = Vector(in["direction"], {0, 0, -1});
    const auto width = in["width"].toInt(200), height = in["height"].toInt(100);
    Require(width > 0 && height > 0, "viewport must be positive");
    const auto up = std::abs(direction.y) > .9f ? XMVectorSet(0, 0, -1, 0)
                                                : XMVectorSet(0, 1, 0, 0);
    XMFLOAT4X4 view, projection;
    XMStoreFloat4x4(
        &view, XMMatrixTranspose(XMMatrixLookToRH(
                   XMVectorSet(origin.x, origin.y, origin.z, 0),
                   XMVectorSet(direction.x, direction.y, direction.z, 0), up)));
    const float fov = float(in["fov"].toDouble(60)),
                size = float(in["orthoSize"].toDouble(10));
    Require(fov > 0 && fov < 180 && size > 0, "invalid camera settings");
    XMStoreFloat4x4(
        &projection,
        XMMatrixTranspose(
            in["orthographic"].toBool()
                ? XMMatrixOrthographicRH(size * width / height, size, .1f, 1000)
                : XMMatrixPerspectiveFovRH(fov * XM_PI / 180,
                                           float(width) / height, .1f, 1000)));
    auto ray = Trace::ScreenPointToRay({float(in["x"].toDouble(width * .5)),
                                        float(in["y"].toDouble(height * .5))},
                                       width, height, view, projection);
    QJsonObject out{{"status", Trace::StatusName(ray.status)}};
    if (ray) {
      out["origin"] = Array(ray.ray.origin);
      out["direction"] = Array(ray.ray.direction);
    }
    return out;
  }
  if (command == "session.lifecycle") {
    struct Counter final : GameSubsystem {
      int ready = 0, closing = 0, ticks = 0;
      float elapsed = 0;
      void OnSceneReady(Scene &) override { ++ready; }
      void OnSceneClosing(Scene &) override { ++closing; }
      void Tick(float dt) override {
        ++ticks;
        elapsed += dt;
      }
    };
    const GameSubsystemType type{"Commandlet.Counter",
                                 &CreateGameSubsystem<Counter>};
    GameSession session;
    std::string error;
    Require(session.Initialize({&type, 1}, error), error.c_str());
    auto *counter = session.GetSubsystem<Counter>();
    counter->bTickEnabled = in["tickEnabled"].toBool(true);
    const int scenes = in["scenes"].toInt(2), frames = in["frames"].toInt(3);
    Require(scenes >= 0 && scenes <= 100 && frames >= 0 && frames <= 10000,
            "bounded scenes/frames required");
    for (int i = 0; i < scenes; ++i) {
      Game game;
      game.AttachSession(session);
      game.InitializeGameplay();
      for (int f = 0; f < frames; ++f)
        game.StepProjectSimulation(float(in["dt"].toDouble(.02)));
    }
    QJsonObject out{{"ready", counter->ready},
                    {"closing", counter->closing},
                    {"ticks", counter->ticks},
                    {"elapsed", counter->elapsed}};
    session.Stop();
    out["running"] = session.IsRunning();
    return out;
  }
  throw std::runtime_error("unknown command; use --list");
}
} // namespace
int main(int argc, char **argv) {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  // Native material preview needs a real child HWND. Keep that window hidden;
  // other commandlets use offscreen Qt and their own explicit D3D HWND.
  qputenv("QT_QPA_PLATFORM", argc > 1 && std::string(argv[1]) == "material.graph"
                                ? "windows" : "offscreen");
  QApplication app(argc, argv);
  qInstallMessageHandler(Messages);
  // Qt's offscreen Windows backend has no system font database.
  const int font = QFontDatabase::addApplicationFont(
      qEnvironmentVariable("WINDIR") + "/Fonts/segoeui.ttf");
  if (font >= 0)
    app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(), 9));
  QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") +
                                    "/Fonts/seguisym.ttf");
  const auto args = app.arguments();
  if (args.size() == 4) {
    crashDumpPath = (args[3] + ".dmp").toStdWString();
    SetUnhandledExceptionFilter(DumpCrash);
  }
  if (args.size() == 2 && args[1] == "--help") {
    std::puts("EngineCommandlets <command> <request.json> "
              "<result.json>\n--list lists commands. Contracts: "
              "Engine/docs/COMMANDLETS.md\nUI runs offscreen; project commands "
              "require D3D and modify the named scratch project.");
    return 0;
  }
  if (args.size() == 2 && args[1] == "--list") {
    std::puts("ui.style\nui.inspector\nui.edit-text\ninput.rebind\ntrace."
              "raycast\ngizmo.snap\ngizmo.center\ncamera.screen-ray\nscene."
              "property\nsession.lifecycle\nproject.build\nproject.reload-"
              "input\nproject.input\nproject.scene\nrender."
              "diagnostics\nrender.point-shadows\nrender.transparency\nmaterial.graph\nmesh.primitive\nmesh.tangents\ngame."
              "settings\nasset.import\nsavegame.save\nsavegame.load\nsavegame."
              "validate\nsavegame.list\nsavegame.delete");
    return 0;
  }
  QJsonObject result;
  int code = 0;
  try {
    Require(args.size() == 4,
            "usage: EngineCommandlets <command> <request.json> <result.json>");
    QFile file(args[2]);
    Require(file.open(QIODevice::ReadOnly), "request cannot be read");
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(file.readAll(), &error);
    Require(error.error == QJsonParseError::NoError && document.isObject(),
            "request must be a JSON object");
    result = Run(args[1], document.object());
    if (args[1] == "project.build" && !result["buildSucceeded"].toBool())
      code = 1;
    result["ok"] = code == 0;
  } catch (const std::exception &e) {
    result = {{"ok", false}, {"error", QString::fromUtf8(e.what())}};
    code = 2;
  }
  const auto bytes = QJsonDocument(result).toJson();
  if (args.size() == 4) {
    QFile output(args[3]);
    if (!output.open(QIODevice::WriteOnly) ||
        output.write(bytes) != bytes.size())
      return 3;
  }
  std::fwrite(bytes.constData(), 1, bytes.size(), stdout);
  return code;
}
