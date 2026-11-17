#pragma once

#include "ObjectInspectorPanel.h"
#include "CollapsibleCard.h"
#include "CameraGuideCard.h"
#include "InfoButton.h"

#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QScreen>
#include <iostream>
#include <stdexcept>

namespace CrossTypeInfoButtonTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

inline void CheckCardInfoBtn(ObjectInspectorPanel& inspector, const QString& cardName, bool expectVisible)
{
    auto* card = inspector.findChild<QWidget*>(cardName);
    Check(card != nullptr, (cardName + " exists").toLatin1().constData());
    auto* coll = dynamic_cast<CollapsibleCard*>(card);
    Check(coll != nullptr, (cardName + " is a CollapsibleCard").toLatin1().constData());
    auto* infoBtn = coll->InfoBtn();
    Check(infoBtn != nullptr, (cardName + " has InfoBtn").toLatin1().constData());
    Check(infoBtn->size() == QSize(18, 18), (cardName + " InfoButton size is 18x18").toLatin1().constData());
    if (expectVisible)
    {
        Check(!infoBtn->isHidden(), (cardName + " InfoButton is visible").toLatin1().constData());
        Check(!infoBtn->CurrentText().isEmpty(), (cardName + " InfoButton has non-empty text").toLatin1().constData());

        infoBtn->click();
        QApplication::processEvents();

        QWidget* popup = nullptr;
        for (auto* top : QApplication::topLevelWidgets())
        {
            if (top->objectName() == QStringLiteral("InfoPopupWindow") && top->isVisible())
            {
                popup = top;
                break;
            }
        }
        Check(popup != nullptr, (cardName + " InfoPopupWindow opened").toLatin1().constData());
        Check(popup->isVisible(), (cardName + " InfoPopupWindow is visible").toLatin1().constData());
        const auto geom = popup->geometry();
        const auto screenGeom = QGuiApplication::primaryScreen()->availableGeometry();
        Check(geom.width() > 50 && geom.height() > 20, (cardName + " InfoPopupWindow has valid dimensions").toLatin1().constData());
        Check(screenGeom.contains(geom), (cardName + " InfoPopupWindow is strictly within screen bounds").toLatin1().constData());

        QKeyEvent escEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(popup, &escEvent);
        QApplication::processEvents();
        Check(!popup->isVisible(), (cardName + " InfoPopupWindow closed on Esc").toLatin1().constData());
    }
    else
    {
        Check(infoBtn->isHidden(), (cardName + " InfoButton is hidden (no SetInfo)").toLatin1().constData());
    }
}

inline void Run()
{
    std::cout << "[Test] Running CrossTypeInfoButtonTests..." << std::endl;

    ObjectInspectorPanel inspector;
    inspector.resize(320, 600);
    inspector.show();
    QApplication::processEvents();

    // 1. Mesh
    {
        EditorQtHost::Snapshot snap;
        EditorQtHost::SceneObject obj;
        obj.id = 1;
        obj.type = "Mesh";
        obj.name = "HeroMesh";
        obj.modelAsset = "/Game/Models/Hero.fbx";
        obj.materialAsset = "/Game/Materials/Hero.mat";
        obj.hasPhysics = true;
        obj.physics = {1u, 1u, 0u, 0.5f, 0.f, 1000.f};
        snap.objects = {obj};
        snap.scripts = true;
        inspector.Refresh(snap, 1, true);
        QApplication::processEvents();

        CheckCardInfoBtn(inspector, QStringLiteral("TransformCard"), false);
        CheckCardInfoBtn(inspector, QStringLiteral("GeneralCard"), false);
        CheckCardInfoBtn(inspector, QStringLiteral("GeometryCard"), true);
        CheckCardInfoBtn(inspector, QStringLiteral("MaterialCard"), true);
        CheckCardInfoBtn(inspector, QStringLiteral("PhysicsCard"), true);
    }

    // 2. Light
    {
        EditorQtHost::Snapshot snap;
        EditorQtHost::SceneObject obj;
        obj.id = 2;
        obj.type = "Light.Point";
        obj.name = "HeroLight";
        obj.hasPointLight = true;
        obj.pointLight = {{1.f, 1.f, 1.f}, 5.f, 10.f, 1u};
        snap.objects = {obj};
        inspector.Refresh(snap, 2, true);
        QApplication::processEvents();

        CheckCardInfoBtn(inspector, QStringLiteral("LightCard"), true);
    }

    // 3. Camera
    {
        EditorQtHost::Snapshot snap;
        EditorQtHost::SceneObject obj;
        obj.id = 3;
        obj.type = "Gameplay.Camera";
        obj.name = "MainCamera";
        snap.objects = {obj};
        inspector.Refresh(snap, 3, true);
        QApplication::processEvents();

        CheckCardInfoBtn(inspector, QStringLiteral("CameraGuideCard"), true);
    }

    // 4. Character
    {
        EditorQtHost::Snapshot snap;
        EditorQtHost::SceneObject obj;
        obj.id = 4;
        obj.type = "Character";
        obj.name = "PlayerChar";
        obj.propertySchema = R"({"fields":[{"path":"/stateMachine","type":"string"},{"path":"/machineMemory","type":"string"}]})";
        obj.projectData = R"({"stateMachine":"/Game/AI/Hero.fsm","machineMemory":"{}"})";
        snap.objects = {obj};
        inspector.Refresh(snap, 4, true);
        QApplication::processEvents();

        CheckCardInfoBtn(inspector, QStringLiteral("CharacterCard"), true);
        CheckCardInfoBtn(inspector, QStringLiteral("AiMachineCard"), true);
    }

    std::cout << "CrossTypeInfoButtonTests: all checks passed." << std::endl;
}

} // namespace CrossTypeInfoButtonTests
