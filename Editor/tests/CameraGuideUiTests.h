#include "CameraGuideCard.h"
#include <QScrollArea>
#include <QScrollBar>
inline void RunCameraGuideChecks(EditorQtHost &host,
                                 const EngineEditSessionApi &api,
                                 EngineGetEditExtensionFn query,
                                 const std::filesystem::path &root,
                                 const std::function<void()> &pump) {
  uint64_t id = 0;
  const auto rotation = glm::angleAxis(glm::radians(-22.f), glm::vec3(1, 0, 0));
  EngineSceneTransform t{
      {0, 4, 6}, {rotation.x, rotation.y, rotation.z, rotation.w}, {1, 1, 1}};
  Check(api.scene.createObject(api.scene.context, "Gameplay.Camera",
                               "Обзорная камера", &t, &id) == EngineSceneOk,
        "create scene camera");
  EngineAuthoringApi authoring{};
  Check(query(api.context, EngineAuthoringExtension, 1, sizeof(authoring),
              &authoring) != 0,
        "camera properties API");
  auto property = [&](const char *name, const char *value) {
    char error[512]{};
    Check(authoring.setProperty(authoring.context, id, name, value, error,
                                sizeof(error)) == EngineSceneOk,
          "camera property edit");
  };
  property("/farClip", "50");
  EngineEditorCameraApi editor{};
  Check(query(api.context, EngineEditorCameraExtension, 1, sizeof(editor),
              &editor) != 0,
        "editor camera API");
  EngineEditorCameraState pose{17, 16, 22, -2.22f, -.47f};
  Check(editor.write(editor.context, &pose) == EngineSceneOk,
        "camera guide overview pose");
  EditorCommand refresh;
  refresh.type = EditorCommand::RefreshScene;
  host.PostCommand(refresh);
  EditorCommand select;
  select.type = EditorCommand::SelectObject;
  select.objectId = id;
  host.PostCommand(select);
  auto settle = [&] {
    for (int i = 0; i < 65; ++i)
      pump();
  };
  settle();
  Ui([](auto *w) {
    w->resize(1560, 1000);
    for (auto *d : w->template findChildren<QDockWidget *>())
      d->setVisible(d->objectName() == "ViewportDock" ||
                    d->objectName() == "InspectorDock");
    w->resizeDocks({w->template findChild<QDockWidget *>("InspectorDock")},
                   {430}, Qt::Horizontal);
    SettingsService::Instance().SetValue("view/grid", true);
    SettingsService::Instance().SetValue("view/cameras", true);
    SettingsService::Instance().SetValue("view/cameraFull", false);
    SettingsService::Instance().SetValue("view/cameraXray", false);
    auto *card = w->template findChild<QWidget *>("CameraGuideCard");
    Check(card && !card->isHidden(), "camera guide card visible");
    auto *fov = w->template findChild<QDoubleSpinBox *>("Property/fov");
    Check(fov && fov->isVisibleTo(w), "camera optics no longer hidden");
    auto *depth = w->template findChild<QDoubleSpinBox *>("CameraGuideDepth");
    Check(depth != nullptr, "guide depth control available");
    depth->setFocus();
    depth->setValue(12);
    QMetaObject::invokeMethod(depth, "editingFinished", Qt::DirectConnection);
    w->template findChild<QCheckBox *>("CameraGuideFull")->setChecked(false);
    w->template findChild<QCheckBox *>("CameraGuideXray")->setChecked(false);
    // Put the camera guide near the top without hiding scene properties.
    if (auto *scroll =
            w->template findChild<QScrollArea *>("InspectorScrollArea"))
      scroll->ensureWidgetVisible(card, 0, 12);
  });
  settle();
  auto capture = [&](const char *name) {
    if (!qEnvironmentVariableIsSet("CAMERA_GUIDE_CAPTURE"))
      return;
    Ui([](auto *w) {
      w->raise();
      w->activateWindow();
    });
    settle();
    std::ofstream(root / "capture-ready.txt") << name;
    const auto end = GetTickCount64() + 300000;
    for (;;) {
      std::string ack;
      std::ifstream(root / "capture-done.txt") >> ack;
      if (ack == name)
        break;
      Check(GetTickCount64() < end, "camera capture timeout");
      pump();
    }
  };
  capture("01-camera-perspective");
  Ui([](auto *w) {
    auto *value = w->template findChild<QDoubleSpinBox *>("Property/fov");
    value->setValue(75);
    QMetaObject::invokeMethod(value, "editingFinished", Qt::DirectConnection);
  });
  settle();
  bool updated = false;
  for (const auto &o : host.ReadSnapshot().objects)
    if (o.id == id)
      updated =
          QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData))
              .object()["fov"]
              .toDouble() == 75;
  Check(updated, "camera FOV edits reach scene through inspector");
  Ui([](auto *w) {
    w->template findChild<QCheckBox *>("CameraGuidesEnabled")
        ->setChecked(false);
  });
  settle();
  Ui([](auto *) {
    Check(!SettingsService::Instance().GetValue("view/cameras", true).toBool(),
          "camera guide master off");
  });
  Ui([](auto *w) {
    w->template findChild<QCheckBox *>("CameraGuidesEnabled")->setChecked(true);
  });
  property("/orthographic", "true");
  property("/orthoHeight", "8");
  property("/viewPlane", "2");
  host.PostCommand(refresh);
  settle();
  Ui([](auto *w) {
    w->template findChild<QCheckBox *>("CameraGuideXray")->setChecked(true);
  });
  settle();
  capture("02-camera-orthographic");
  Ui([](auto *w) {
    Check(w->template findChild<QLabel *>("CameraGuideSummary")
              ->text()
              .contains("8.00"),
          "orthographic dimensions visible");
  });
  std::printf("Camera guides: UI and scene property checks passed\n");
  std::fflush(stdout);
}
