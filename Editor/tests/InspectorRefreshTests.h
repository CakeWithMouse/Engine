#pragma once

#include "ObjectInspectorPanel.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

namespace InspectorRefreshTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(std::string("InspectorRefresh check failed: ") + message);
}

class CardEventCounter : public QObject
{
public:
    int layoutRequests = 0, shows = 0, hides = 0, resizes = 0;

    bool eventFilter(QObject* obj, QEvent* ev) override
    {
        switch (ev->type())
        {
        case QEvent::LayoutRequest: ++layoutRequests; break;
        case QEvent::Show: ++shows; break;
        case QEvent::Hide: ++hides; break;
        case QEvent::Resize: ++resizes; break;
        default: break;
        }
        return QObject::eventFilter(obj, ev);
    }

    int Total() const { return layoutRequests + shows + hides + resizes; }
    void Reset() { layoutRequests = shows = hides = resizes = 0; }
};

inline QWidget* CardOf(QWidget* panel, const QString& editObjectName)
{
    auto* edit = panel->findChild<QLineEdit*>(editObjectName);
    if (!edit) return nullptr;
    QWidget* w = edit;
    while (w && w->objectName() != QStringLiteral("CollapsibleCard"))
        w = w->parentWidget();
    return w;
}

inline void InstallCounter(QWidget* card, CardEventCounter* counter)
{
    card->installEventFilter(counter);
    const auto children = card->findChildren<QWidget*>();
    for (auto* child : children)
        child->installEventFilter(counter);
}

inline void WriteFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        throw std::runtime_error("InspectorRefresh: cannot create fixture file");
    f.write("x");
    f.close();
}

inline void Run()
{
    std::cout << "[Test] Running InspectorRefreshTests..." << std::endl;
    int checks = 0;

    QTemporaryDir root;
    Check(root.isValid(), "temp content root created");
    QDir().mkpath(root.path() + "/Models");
    QDir().mkpath(root.path() + "/Textures");
    QDir().mkpath(root.path() + "/Materials");
    WriteFile(root.path() + "/Models/m.fbx");
    WriteFile(root.path() + "/Models/m2.fbx");
    WriteFile(root.path() + "/Textures/t.png");
    WriteFile(root.path() + "/Materials/mat.mat");

    EditorQtHost::Snapshot snap;
    EditorQtHost::SceneObject model;
    model.id = 7;
    model.type = "Engine.Model";
    model.name = "TestModel";
    model.modelAsset = "/Game/Models/m.fbx";
    model.textureAsset = "/Game/Textures/t.png";
    model.materialAsset = "/Game/Materials/mat.mat";
    model.transform.rotation[3] = 1.0f;
    model.transform.scale[0] = model.transform.scale[1] = model.transform.scale[2] = 1.0f;
    snap.objects = {model};
    snap.selected = 7;

    ObjectInspectorPanel panel;
    panel.setProperty("contentRoot", root.path());
    panel.resize(320, 700);
    panel.show();
    QApplication::processEvents();

    panel.Refresh(snap, 7, true);
    QApplication::processEvents();

    auto* modelEdit = panel.findChild<QLineEdit*>(QStringLiteral("ModelAsset"));
    auto* textureEdit = panel.findChild<QLineEdit*>(QStringLiteral("TextureAsset"));
    auto* materialEdit = panel.findChild<QLineEdit*>(QStringLiteral("MaterialAsset"));
    Check(modelEdit && !textureEdit && materialEdit, "card asset edits found by objectName, TextureAsset removed");
    Check(modelEdit->text() == QStringLiteral("Models/m.fbx"), "relative model path applied on first Refresh");
    Check(materialEdit->text() == QStringLiteral("/Game/Materials/mat.mat"), "material path applied on first Refresh");
    ++checks;

    QWidget* geomCard = CardOf(&panel, QStringLiteral("ModelAsset"));
    QWidget* matCard = CardOf(&panel, QStringLiteral("MaterialAsset"));
    Check(geomCard && matCard, "geometry and material cards located by objectName");
    Check(!geomCard->isHidden() && !matCard->isHidden(), "both cards visible for Engine.Model");
    ++checks;

    for (int i = 0; i < 10; ++i)
        QApplication::processEvents();

    CardEventCounter geomCounter, matCounter;
    InstallCounter(geomCard, &geomCounter);
    InstallCounter(matCard, &matCounter);

    for (int i = 0; i < 20; ++i)
    {
        panel.Refresh(snap, 7, false);
        QApplication::processEvents();
    }
    Check(geomCounter.Total() == 0, "geometry card: 0 LayoutRequest/Show/Hide/Resize over 20 unchanged Refresh");
    Check(matCounter.Total() == 0, "material card: 0 LayoutRequest/Show/Hide/Resize over 20 unchanged Refresh");
    ++checks;

    snap.objects[0].modelAsset = "/Game/Models/m2.fbx";
    panel.Refresh(snap, 7, false);
    QApplication::processEvents();
    Check(modelEdit->text() == QStringLiteral("Models/m2.fbx"), "changed model path applied without selectionChanged");
    ++checks;

    snap.objects[0].type = "Light.Point";
    snap.objects[0].hasPointLight = true;
    panel.Refresh(snap, 7, false);
    for (int i = 0; i < 10; ++i)
        QApplication::processEvents();
    Check(geomCard->isHidden(), "geometry card hidden for point light");
    Check(matCard->isHidden(), "material card hidden for point light");
    geomCounter.Reset();
    matCounter.Reset();
    for (int i = 0; i < 20; ++i)
    {
        panel.Refresh(snap, 7, false);
        QApplication::processEvents();
    }
    Check(geomCounter.Total() == 0, "geometry card: 0 events over 20 unchanged light Refresh");
    Check(matCounter.Total() == 0, "material card: 0 events over 20 unchanged light Refresh");
    ++checks;

    snap.objects[0].type = "Engine.Model";
    snap.objects[0].hasPointLight = false;
    panel.Refresh(snap, 7, false);
    QApplication::processEvents();
    Check(!geomCard->isHidden() && !matCard->isHidden(), "cards visible again after switching back to model");
    Check(modelEdit->text() == QStringLiteral("Models/m2.fbx"), "model path restored after type round-trip");
    ++checks;

    panel.Refresh(snap, 7, true);
    QApplication::processEvents();
    Check(modelEdit->text() == QStringLiteral("Models/m2.fbx"), "selectionChanged=true re-applies same data");
    ++checks;

    auto* posSpin = panel.findChild<QDoubleSpinBox*>(QStringLiteral("Transform0"));
    Check(posSpin != nullptr, "Transform0 spin found by objectName");
    panel.activateWindow();
    posSpin->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    Check(QApplication::focusWidget() == posSpin, "Transform0 spin holds focus");
    const double oldPos = posSpin->value();
    snap.objects[0].transform.position[0] = 42.0f;
    panel.Refresh(snap, 7, false);
    QApplication::processEvents();
    Check(posSpin->value() == oldPos, "focused transform field preserved on Refresh");
    posSpin->clearFocus();
    QApplication::processEvents();
    panel.Refresh(snap, 7, false);
    QApplication::processEvents();
    Check(posSpin->value() == 42.0, "unfocused transform field updated on Refresh");
    ++checks;

    std::cout << "InspectorRefreshTests: " << checks << " checks passed." << std::endl;
}

// Regression for the mouse-move card jitter (T1 repeat). A selected object that
// carries a property schema but whose generic properties card is suppressed
// (showProperties=false, e.g. Light.Directional) used to be re-shown by
// RefreshProjectProperties and re-hidden by Refresh on EVERY refresh. Each
// viewport mouse-move posts InputCursor -> Complete -> RefreshSceneUi -> Refresh,
// so that show/hide churn invalidated the scroll layout once per move and the
// cards oscillated in height near the vertical-scrollbar threshold. Repeated
// identical-data Refresh must now be a layout no-op.
inline void RunMouseRefreshJitter()
{
    std::cout << "[Test] Running InspectorRefresh mouse-move jitter regression..." << std::endl;
    int checks = 0;

    QTemporaryDir root;
    Check(root.isValid(), "jitter: temp content root created");

    EditorQtHost::Snapshot snap;
    EditorQtHost::SceneObject light;
    light.id = 5;
    light.type = "Light.Directional";
    light.name = "Sun";
    light.transform.rotation[3] = 1.0f;
    light.transform.scale[0] = light.transform.scale[1] = light.transform.scale[2] = 1.0f;
    light.propertySchema = R"({"version":1,"fields":[
        {"path":"/enabled","type":"bool","label":"Enabled","group":"Directional Light"},
        {"path":"/intensity","type":"number","label":"Intensity","min":0,"max":1000,"step":0.1,"group":"Directional Light"}]})";
    light.projectData = R"({"enabled":true,"intensity":1.0})";
    snap.objects = {light};
    snap.selected = 5;

    ObjectInspectorPanel panel;
    panel.setProperty("contentRoot", root.path());
    // Height where the vertical scrollbar sits at the content-height threshold:
    // the pre-fix churn oscillated the cards here on every refresh.
    panel.resize(300, 430);
    panel.show();
    panel.Refresh(snap, 5, true);
    for (int i = 0; i < 20; ++i)
        QApplication::processEvents();

    auto* content = panel.findChild<QWidget*>(QStringLiteral("InspectorScrollContent"));
    Check(content != nullptr, "jitter: scroll content located by objectName");

    CardEventCounter counter;
    InstallCounter(content, &counter);
    for (int move = 0; move < 30; ++move)
    {
        panel.Refresh(snap, 5, false);
        QApplication::processEvents();
    }
    Check(counter.shows == 0, "jitter: no properties re-show churn over 30 mouse-move refreshes");
    Check(counter.hides == 0, "jitter: no properties re-hide churn over 30 mouse-move refreshes");
    Check(counter.resizes == 0, "jitter: no card height oscillation over 30 mouse-move refreshes");
    ++checks;

    std::cout << "InspectorRefresh mouse-move jitter regression: " << checks << " checks passed." << std::endl;
}

inline void RunModelReplaceButtonRegression()
{
    std::cout << "[Test] Running Inspector choose model button regression (Bug 1)..." << std::endl;
    int checks = 0;

    EditorQtHost::Snapshot snap;
    EditorQtHost::SceneObject modelObj;
    modelObj.id = 10;
    modelObj.type = "Engine.Model";
    modelObj.name = "MyModel";
    modelObj.modelAsset = "/Game/Models/Original.fbx";
    modelObj.transform.scale[0] = modelObj.transform.scale[1] = modelObj.transform.scale[2] = 1.0f;

    EditorQtHost::SceneObject primObj;
    primObj.id = 20;
    primObj.type = "Primitive.Cube";
    primObj.name = "MyBox";
    primObj.transform.scale[0] = primObj.transform.scale[1] = primObj.transform.scale[2] = 1.0f;

    snap.objects = {modelObj, primObj};
    snap.selected = 10;

    int undoCount = 0;
    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Scene.ReplaceModel"),
        QStringLiteral("Replace Model"),
        QString(),
        QStringLiteral("Test replace model"),
        [&](const CommandContext& ctx) {
            const uint64_t targetId = ctx.parameters.value(QStringLiteral("objectId"), 0).toULongLong();
            for (auto& o : snap.objects) {
                if (o.id == targetId && o.type == "Engine.Model") {
                    o.modelAsset = ctx.parameters.value(QStringLiteral("model"), QStringLiteral("/Game/Models/Replaced.fbx")).toString().toStdString();
                    ++undoCount;
                    return true;
                }
            }
            return false;
        },
        []() { return true; }
    });

    ObjectInspectorPanel panel;
    panel.show();
    QApplication::processEvents();

    auto* chooseBtn = panel.findChild<QPushButton*>(QStringLiteral("ChooseModel"));
    Check(chooseBtn != nullptr, "ChooseModel button found by objectName");
    auto* modelEdit = panel.findChild<QLineEdit*>(QStringLiteral("ModelAsset"));
    Check(modelEdit != nullptr, "ModelAsset edit found by objectName");

    // 1. Select Engine.Model -> button enabled
    panel.Refresh(snap, 10, true);
    QApplication::processEvents();
    Check(chooseBtn->isEnabled(), "ChooseModel button enabled for Engine.Model");
    Check(chooseBtn->toolTip().isEmpty(), "ChooseModel tooltip empty for Engine.Model");
    ++checks;

    // 2. Scene.ReplaceModel -> path replaced, 1 Undo point
    QVariantMap params;
    params[QStringLiteral("objectId")] = static_cast<qulonglong>(10);
    params[QStringLiteral("model")] = QStringLiteral("/Game/Models/Replaced.fbx");
    const bool replaced = CommandRegistry::Instance().Execute(QStringLiteral("Scene.ReplaceModel"), params);
    Check(replaced, "Scene.ReplaceModel executed successfully");
    Check(snap.objects[0].modelAsset == "/Game/Models/Replaced.fbx", "Model path replaced in scene object");
    Check(undoCount == 1, "Exactly one Undo point recorded");
    panel.Refresh(snap, 10, false);
    QApplication::processEvents();
    Check(modelEdit->text() == QStringLiteral("Models/Replaced.fbx"), "Inspector reflects replaced model path");
    Check(chooseBtn->isEnabled(), "ChooseModel button remains enabled after replacement");
    ++checks;

    // 3. Select primitive -> disabled with primitive blocking tooltip
    snap.selected = 20;
    panel.Refresh(snap, 20, true);
    QApplication::processEvents();
    Check(!chooseBtn->isEnabled(), "ChooseModel button disabled for Primitive.Cube");
    Check(chooseBtn->toolTip().contains(QStringLiteral("C-ABI")), "ChooseModel tooltip indicates primitive replacement blocked");
    panel.Refresh(snap, 20, false);
    QApplication::processEvents();
    Check(!chooseBtn->isEnabled(), "ChooseModel button remains disabled on unchanged primitive Refresh");
    ++checks;

    // 4. Select model again -> enabled (catches stuck disabled state)
    snap.selected = 10;
    panel.Refresh(snap, 10, true);
    QApplication::processEvents();
    Check(chooseBtn->isEnabled(), "ChooseModel button enabled again for Engine.Model (not stuck)");
    Check(chooseBtn->toolTip().isEmpty(), "ChooseModel tooltip cleared again for Engine.Model");

    // Stuck state regression: simulate button disabled by prior primitive/external state,
    // followed by an unchanged Refresh(..., false). Must be re-enabled on EVERY Refresh.
    chooseBtn->setEnabled(false);
    chooseBtn->setToolTip(QStringLiteral("stuck"));
    panel.Refresh(snap, 10, false);
    QApplication::processEvents();
    Check(chooseBtn->isEnabled(), "ChooseModel button re-enabled on every Refresh even when cardsDirty is false");
    Check(chooseBtn->toolTip().isEmpty(), "ChooseModel tooltip cleared on every Refresh even when cardsDirty is false");
    ++checks;

    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Scene.ReplaceModel"));
    std::cout << "Inspector choose model button regression: " << checks << " checks passed." << std::endl;
}

} // namespace InspectorRefreshTests