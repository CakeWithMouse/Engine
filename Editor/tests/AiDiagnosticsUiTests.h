#include "SettingsService.h"
#include <QAbstractItemView>
// Real module + D3D viewport regression and visual evidence. Invoked by --ai-diagnostics.
inline void RunAiDiagnosticsChecks(EditorQtHost &host, const EngineEditSessionApi &api, EngineGetEditExtensionFn query,
                                   const std::filesystem::path &root, const std::function<void()> &pump)
{
    EngineAiDiagnosticsApi diagnostic{};
    Check(query(api.context, EngineAiDiagnosticsExtension, 1, sizeof(diagnostic), &diagnostic) != 0,
          "AI diagnostic extension available");
    EngineAiDiagnosticsApi rejected{99, nullptr, nullptr};
    Check(!query(api.context, EngineAiDiagnosticsExtension, 2, sizeof(rejected), &rejected) && rejected.version == 99,
          "AI diagnostic version isolation");
    host.SetAiDiagnosticsApi(diagnostic);
    auto inspect = [&](QJsonObject request) {
        QByteArray bytes;
        const auto text = QJsonDocument(request).toJson(QJsonDocument::Compact);
        Check(diagnostic.inspect(
                  diagnostic.context, text.constData(),
                  [](void *p, const char *s) { *static_cast<QByteArray *>(p) = s; }, &bytes) == EngineSceneOk,
              "diagnostic inspection succeeds");
        return QJsonDocument::fromJson(bytes).object();
    };
    auto base = inspect({{"layers", 7}, {"radius", 1.7}, {"hover", true}, {"hoverX", 0}, {"hoverZ", 6}});
    Check(base["source"] == "preview" && base["configured"].toBool() && base["units"].toArray().isEmpty(),
          "Edit preview does not start controllers");
    Check(base["cell"].toObject()["cost"].toInt() == 255 && !base["cell"].toObject()["walkable"].toBool(),
          "probe identifies blocked navigation");
    struct Volume
    {
        uint64_t id = 0;
        EngineSceneTransform transform{};
    } volume;
    EngineSceneInfo info{};
    api.scene.readScene(
        api.scene.context, &info,
        [](void *p, const EngineSceneObjectInfo *o) {
            if (std::string(o->name) == "Blocked 6")
                *static_cast<Volume *>(p) = {o->id, o->transform};
        },
        &volume);
    Check(volume.id != 0, "navigation volume found");
    auto moved = volume.transform;
    moved.position[0] = 8;
    Check(api.scene.setTransform(api.scene.context, volume.id, &moved) == EngineSceneOk,
          "move navigation volume in Edit");
    auto changed = inspect({{"layers", 1}, {"hover", true}, {"hoverX", 0}, {"hoverZ", 6}});
    Check(changed["cell"].toObject()["cost"].toInt() != 255, "preview rebakes after editing volume");
    Check(api.scene.setTransform(api.scene.context, volume.id, &volume.transform) == EngineSceneOk,
          "restore fixture volume");
    auto route = inspect({{"layers", 128}, {"fromX", -12}, {"fromZ", -7}, {"toX", 12}, {"toZ", -7}});
    Check(route["path"] == "found" && route["lines"].toInt() > 0, "probe uses real A star around wall");
    EngineEditorCameraApi camera{};
    Check(query(api.context, EngineEditorCameraExtension, 1, sizeof(camera), &camera) != 0, "camera extension");
    const EngineEditorCameraState pose{0, 24, 16, -1.5707963f, -1.03f};
    Check(camera.write(camera.context, &pose) == EngineSceneOk, "set diagnostic overview camera");
    Ui([](auto *w) {
        w->resize(1560, 1000);
        SettingsService::Instance().SetValue(QStringLiteral("view/grid"), false);
        for (auto *dock : w->template findChildren<QDockWidget *>())
            if (dock->objectName() != "ViewportDock" && dock->objectName() != "AiDiagnosticsDock")
                dock->hide();
        w->template findChild<QDockWidget *>("AiDiagnosticsDock")->show();
        w->resizeDocks({w->template findChild<QDockWidget *>("AiDiagnosticsDock")}, {350}, Qt::Horizontal);
        w->template findChild<QCheckBox *>("AiEnabled")->setChecked(true);
        w->template findChild<QDoubleSpinBox *>("AiFromX")->setValue(-12);
        w->template findChild<QDoubleSpinBox *>("AiFromZ")->setValue(-7);
        w->template findChild<QDoubleSpinBox *>("AiToX")->setValue(12);
        w->template findChild<QDoubleSpinBox *>("AiToZ")->setValue(-7);
        w->template findChild<QDoubleSpinBox *>("AiExtent")->setValue(35);
    });
    auto settle = [&] {
        for (int i = 0; i < 55; ++i)
            pump();
    };
    auto layers = [&](unsigned mask) {
        Ui([&](auto *w) {
            for (int i = 0; i < 8; ++i)
                w->template findChild<QCheckBox *>(QString("AiLayer%1").arg(i))->setChecked((mask & (1u << i)) != 0);
        });
        settle();
    };
    auto capture = [&](const char *name) {
        Ui([&](auto *w) {
            w->raise();
            w->activateWindow();
        });
        settle();
        if (!qEnvironmentVariableIsSet("AI_DIAGNOSTICS_CAPTURE"))
            return;
        std::ofstream(root / "capture-ready.txt") << name;
        const auto deadline = GetTickCount64() + 300000;
        for (;;)
        {
            std::string ack;
            std::ifstream(root / "capture-done.txt") >> ack;
            if (ack == name)
                break;
            Check(GetTickCount64() < deadline, "capture acknowledgement timeout");
            pump();
        }
    };
    layers(1);
    capture("01-navigation");
    layers(2);
    capture("02-cost");
    Ui([](auto *w) { w->template findChild<QDoubleSpinBox *>("AiRadius")->setValue(1.7); });
    layers(4);
    capture("03-clearance");
    Ui([](auto *w) { w->template findChild<QDoubleSpinBox *>("AiRadius")->setValue(.5); });
    layers(1 | 128);
    capture("04-probe-route");
    layers(16);
    capture("05-preview-field");
    Check(api.control(api.context, EnginePlay) == EngineSceneOk, "start actual AI gameplay");
    for (int i = 0; i < 100; ++i)
        pump();
    Check(api.control(api.context, EnginePause) == EngineSceneOk, "pause actual AI gameplay");
    auto live = inspect({{"layers", 8}, {"selectedOnly", false}});
    Check(live["source"] == "paused" && live["units"].toArray().size() >= 2,
          "paused snapshot contains real controllers");
    int guard = 0, squad = 0;
    for (auto v : live["units"].toArray())
    {
        auto u = v.toObject();
        if (u["name"] == "Guard")
            guard = u["id"].toInt();
        if (u["name"] == "North squad")
            squad = u["id"].toInt();
    }
    Check(guard && squad, "character and squad selectable");
    auto choose = [&](int id) {
        settle();
        Ui([&](auto *w) {
            auto *combo = w->template findChild<QComboBox *>("AiSubject");
            const int index = combo->findData(id);
            Check(index >= 0, "subject listed in UI");
            combo->setCurrentIndex(index);
        });
        settle();
    };
    Ui([](auto *w){w->template findChild<QComboBox *>("AiSubject")->showPopup();});
    settle();
    Ui([](auto *w){auto* combo=w->template findChild<QComboBox *>("AiSubject");Check(combo->view()->isVisible(),"subject popup survives snapshot refresh");combo->hidePopup();});
    choose(guard);
    layers(8);
    capture("06-live-path");
    layers(32);
    capture("07-perception");
    choose(squad);
    layers(16);
    capture("08-live-field");
    layers(64);
    capture("09-formation");
    auto field = inspect({{"layers", 16}, {"controller", squad}});
    Check(field["field"] == "ready" && field["lines"].toInt() > 20, "real squad field ready and drawable");
    auto formation = inspect({{"layers", 64}, {"controller", squad}});
    Check(formation["lines"].toInt() >= 18, "formation draws live member slots");
    Ui([](auto *w) { w->template findChild<QCheckBox *>("AiEnabled")->setChecked(false); });
    settle();
    capture("10-disabled");
    Check(inspect({{"layers", 0}})["source"] == "off", "master switch clears overlay");
    Check(api.control(api.context, EngineStop) == EngineSceneOk, "stop AI returns to Edit");
    auto preview = inspect({{"layers", 1}});
    Check(preview["source"] == "preview" && preview["units"].toArray().isEmpty(),
          "stop does not retain runtime controllers");
    inspect({{"layers", 0}});
    std::puts("AI diagnostics: native viewport evidence and regression checks passed");
}
