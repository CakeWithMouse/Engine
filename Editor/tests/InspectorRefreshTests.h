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
    Check(modelEdit && textureEdit && materialEdit, "card asset edits found by objectName");
    Check(modelEdit->text() == QStringLiteral("/Game/Models/m.fbx"), "model path applied on first Refresh");
    Check(textureEdit->text() == QStringLiteral("/Game/Textures/t.png"), "texture path applied on first Refresh");
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
    Check(modelEdit->text() == QStringLiteral("/Game/Models/m2.fbx"), "changed model path applied without selectionChanged");
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
    Check(modelEdit->text() == QStringLiteral("/Game/Models/m2.fbx"), "model path restored after type round-trip");
    ++checks;

    panel.Refresh(snap, 7, true);
    QApplication::processEvents();
    Check(modelEdit->text() == QStringLiteral("/Game/Models/m2.fbx"), "selectionChanged=true re-applies same data");
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

} // namespace InspectorRefreshTests