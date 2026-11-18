#pragma once

#include "ObjectInspectorPanel.h"
#include "SceneObjectsPanel.h"
#include "CollapsibleCard.h"
#include "TransformCard.h"
#include "GeometryCard.h"
#include "MaterialCard.h"
#include "ScriptCard.h"
#include "AiMachineCard.h"
#include "CameraGuideCard.h"
#include "InfoButton.h"
#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QScreen>
#include <QAction>
#include <iostream>
#include <stdexcept>
#include <set>

namespace ObjectTypeInspectorTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

inline bool IsCardVisible(QWidget* inspector, const QString& objectName)
{
    auto* card = inspector->findChild<QWidget*>(objectName);
    if (!card)
        return false;
    return !card->isHidden();
}

inline void RunMatrixTests()
{
    std::cout << "[Test] Running ObjectTypeInspectorTests::Matrix..." << std::endl;

    ObjectInspectorPanel inspector;
    inspector.resize(320, 600);
    inspector.show();
    QApplication::processEvents();

    // Verify all 10 cards exist by their unique objectName
    Check(inspector.findChild<QWidget*>(QStringLiteral("GeneralCard")) != nullptr, "GeneralCard exists");
    Check(inspector.findChild<QWidget*>(QStringLiteral("TransformCard")) != nullptr, "TransformCard exists");
    Check(inspector.findChild<QWidget*>(QStringLiteral("GeometryCard")) != nullptr, "GeometryCard exists");
    Check(inspector.findChild<QWidget*>(QStringLiteral("MaterialCard")) != nullptr, "MaterialCard exists");
    Check(inspector.findChild<QWidget*>(QStringLiteral("LightCard")) != nullptr, "LightCard exists");
    Check(inspector.findChild<QWidget*>(QStringLiteral("CharacterCard")) != nullptr, "CharacterCard exists");
    Check(inspector.findChild<QWidget*>(QStringLiteral("PhysicsCard")) != nullptr, "PhysicsCard exists");
    Check(inspector.findChild<QWidget*>(QStringLiteral("ScriptCard")) != nullptr, "ScriptCard exists");
    Check(inspector.findChild<QWidget*>(QStringLiteral("AiMachineCard")) != nullptr, "AiMachineCard exists");
    Check(inspector.findChild<QWidget*>(QStringLiteral("CameraGuideCard")) != nullptr, "CameraGuideCard exists");

    EditorQtHost::Snapshot snap;
    snap.scripts = true;

    // 1. Light.Point
    {
        EditorQtHost::SceneObject lightObj;
        lightObj.id = 10;
        lightObj.type = "Light.Point";
        lightObj.name = "TestPointLight";
        lightObj.hasPointLight = true;
        lightObj.pointLight = {{1.f, 1.f, 1.f}, 1.f, 10.f, 1u};
        snap.objects = {lightObj};

        inspector.Refresh(snap, 10, true);
        QApplication::processEvents();

        Check(IsCardVisible(&inspector, QStringLiteral("GeneralCard")), "PointLight: GeneralCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("TransformCard")), "PointLight: TransformCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("LightCard")), "PointLight: LightCard visible");
        Check(!IsCardVisible(&inspector, QStringLiteral("PhysicsCard")), "PointLight: PhysicsCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("GeometryCard")), "PointLight: GeometryCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("MaterialCard")), "PointLight: MaterialCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("ScriptCard")), "PointLight: ScriptCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CameraGuideCard")), "PointLight: CameraGuideCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CharacterCard")), "PointLight: CharacterCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("AiMachineCard")), "PointLight: AiMachineCard hidden");
    }

    // 2. Light.Directional
    {
        EditorQtHost::SceneObject dirLight;
        dirLight.id = 11;
        dirLight.type = "Light.Directional";
        dirLight.name = "TestDirLight";
        snap.objects = {dirLight};

        inspector.Refresh(snap, 11, true);
        QApplication::processEvents();

        Check(IsCardVisible(&inspector, QStringLiteral("GeneralCard")), "DirLight: GeneralCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("TransformCard")), "DirLight: TransformCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("LightCard")), "DirLight: LightCard visible");
        Check(!IsCardVisible(&inspector, QStringLiteral("PhysicsCard")), "DirLight: PhysicsCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("GeometryCard")), "DirLight: GeometryCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("MaterialCard")), "DirLight: MaterialCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("ScriptCard")), "DirLight: ScriptCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CameraGuideCard")), "DirLight: CameraGuideCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CharacterCard")), "DirLight: CharacterCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("AiMachineCard")), "DirLight: AiMachineCard hidden");
    }

    // 3. Gameplay.Camera
    {
        EditorQtHost::SceneObject camObj;
        camObj.id = 20;
        camObj.type = "Gameplay.Camera";
        camObj.name = "TestCamera";
        camObj.gameplayKind = 2;
        snap.objects = {camObj};

        inspector.Refresh(snap, 20, true);
        QApplication::processEvents();

        Check(IsCardVisible(&inspector, QStringLiteral("GeneralCard")), "Camera: GeneralCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("TransformCard")), "Camera: TransformCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("CameraGuideCard")), "Camera: CameraGuideCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("ScriptCard")), "Camera: ScriptCard visible");
        Check(!IsCardVisible(&inspector, QStringLiteral("PhysicsCard")), "Camera: PhysicsCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("GeometryCard")), "Camera: GeometryCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("MaterialCard")), "Camera: MaterialCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("LightCard")), "Camera: LightCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CharacterCard")), "Camera: CharacterCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("AiMachineCard")), "Camera: AiMachineCard hidden");
    }

    // 4. Primitive.Cube (Mesh)
    {
        EditorQtHost::SceneObject cubeObj;
        cubeObj.id = 30;
        cubeObj.type = "Primitive.Cube";
        cubeObj.name = "TestCube";
        snap.objects = {cubeObj};

        inspector.Refresh(snap, 30, true);
        QApplication::processEvents();

        Check(IsCardVisible(&inspector, QStringLiteral("GeneralCard")), "Cube: GeneralCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("TransformCard")), "Cube: TransformCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("GeometryCard")), "Cube: GeometryCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("MaterialCard")), "Cube: MaterialCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("PhysicsCard")), "Cube: PhysicsCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("ScriptCard")), "Cube: ScriptCard visible");
        Check(!IsCardVisible(&inspector, QStringLiteral("LightCard")), "Cube: LightCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CameraGuideCard")), "Cube: CameraGuideCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CharacterCard")), "Cube: CharacterCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("AiMachineCard")), "Cube: AiMachineCard hidden");
    }

    // 5. Engine.Model (Imported Mesh)
    {
        EditorQtHost::SceneObject modelObj;
        modelObj.id = 35;
        modelObj.type = "Engine.Model";
        modelObj.name = "TestModel";
        modelObj.modelAsset = "Content/Mesh.glb";
        snap.objects = {modelObj};

        inspector.Refresh(snap, 35, true);
        QApplication::processEvents();

        Check(IsCardVisible(&inspector, QStringLiteral("GeneralCard")), "Model: GeneralCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("TransformCard")), "Model: TransformCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("GeometryCard")), "Model: GeometryCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("MaterialCard")), "Model: MaterialCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("PhysicsCard")), "Model: PhysicsCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("ScriptCard")), "Model: ScriptCard visible");
        Check(!IsCardVisible(&inspector, QStringLiteral("LightCard")), "Model: LightCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CameraGuideCard")), "Model: CameraGuideCard hidden");
    }

    // 6. Empty / Generic Object
    {
        EditorQtHost::SceneObject emptyObj;
        emptyObj.id = 40;
        emptyObj.type = "Empty";
        emptyObj.name = "TestEmpty";
        snap.objects = {emptyObj};

        inspector.Refresh(snap, 40, true);
        QApplication::processEvents();

        Check(IsCardVisible(&inspector, QStringLiteral("GeneralCard")), "Empty: GeneralCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("TransformCard")), "Empty: TransformCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("ScriptCard")), "Empty: ScriptCard visible");
        Check(!IsCardVisible(&inspector, QStringLiteral("GeometryCard")), "Empty: GeometryCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("MaterialCard")), "Empty: MaterialCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("PhysicsCard")), "Empty: PhysicsCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("LightCard")), "Empty: LightCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CameraGuideCard")), "Empty: CameraGuideCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CharacterCard")), "Empty: CharacterCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("AiMachineCard")), "Empty: AiMachineCard hidden");
    }

    // 7. Gameplay.Character
    {
        EditorQtHost::SceneObject charObj;
        charObj.id = 50;
        charObj.type = "Gameplay.Character";
        charObj.name = "TestHero";
        charObj.gameplayKind = 1;
        snap.objects = {charObj};

        inspector.Refresh(snap, 50, true);
        QApplication::processEvents();

        Check(IsCardVisible(&inspector, QStringLiteral("GeneralCard")), "Character: GeneralCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("TransformCard")), "Character: TransformCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("CharacterCard")), "Character: CharacterCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("PhysicsCard")), "Character: PhysicsCard visible");
        Check(IsCardVisible(&inspector, QStringLiteral("ScriptCard")), "Character: ScriptCard visible");
        Check(!IsCardVisible(&inspector, QStringLiteral("LightCard")), "Character: LightCard hidden");
        Check(!IsCardVisible(&inspector, QStringLiteral("CameraGuideCard")), "Character: CameraGuideCard hidden");
    }

    // 8. Preservation of Saved Components on Generic/Empty Objects
    {
        // Empty with saved active physics
        EditorQtHost::SceneObject emptyWithPhysics;
        emptyWithPhysics.id = 60;
        emptyWithPhysics.type = "Empty";
        emptyWithPhysics.name = "SavedPhysicsEmpty";
        emptyWithPhysics.hasPhysics = true;
        emptyWithPhysics.physics.enabled = 1u;
        snap.objects = {emptyWithPhysics};

        inspector.Refresh(snap, 60, true);
        QApplication::processEvents();
        Check(IsCardVisible(&inspector, QStringLiteral("PhysicsCard")), "Preservation: Empty with saved physics retains PhysicsCard");

        // Empty with saved model
        EditorQtHost::SceneObject emptyWithModel;
        emptyWithModel.id = 61;
        emptyWithModel.type = "Empty";
        emptyWithModel.name = "SavedModelEmpty";
        emptyWithModel.modelAsset = "Content/Mesh.glb";
        snap.objects = {emptyWithModel};

        inspector.Refresh(snap, 61, true);
        QApplication::processEvents();
        Check(IsCardVisible(&inspector, QStringLiteral("GeometryCard")), "Preservation: Empty with saved model retains GeometryCard");

        // Empty with saved material
        EditorQtHost::SceneObject emptyWithMat;
        emptyWithMat.id = 62;
        emptyWithMat.type = "Empty";
        emptyWithMat.name = "SavedMatEmpty";
        emptyWithMat.materialAsset = "Content/Gold.mat";
        snap.objects = {emptyWithMat};

        inspector.Refresh(snap, 62, true);
        QApplication::processEvents();
        Check(IsCardVisible(&inspector, QStringLiteral("MaterialCard")), "Preservation: Empty with saved material retains MaterialCard");
    }
}

inline void RunAddMenuTests()
{
    std::cout << "[Test] Running ObjectTypeInspectorTests::AddMenu..." << std::endl;

    SceneObjectsPanel panel;
    panel.resize(300, 500);
    panel.show();
    QApplication::processEvents();

    const QVector<std::pair<QString, QString>> registeredTypes = {
        {QStringLiteral("Куб"), QStringLiteral("Primitive.Cube")},
        {QStringLiteral("Сфера"), QStringLiteral("Primitive.Sphere")},
        {QStringLiteral("Плоскость"), QStringLiteral("Primitive.Plane")},
        {QStringLiteral("Цилиндр"), QStringLiteral("Primitive.Cylinder")},
        {QStringLiteral("Конус"), QStringLiteral("Primitive.Cone")},
        {QStringLiteral("Пирамида"), QStringLiteral("Primitive.Pyramid")},
        {QStringLiteral("Точечный свет"), QStringLiteral("Light.Point")},
        {QStringLiteral("Направленный свет"), QStringLiteral("Light.Directional")},
        {QStringLiteral("Искры"), QStringLiteral("Particle.Sparks")},
        {QStringLiteral("Дым"), QStringLiteral("Particle.Smoke")},
        {QStringLiteral("Базовое небо"), QStringLiteral("Environment.Sky")},
        {QStringLiteral("Персонаж"), QStringLiteral("Gameplay.Character")},
        {QStringLiteral("Игрок"), QStringLiteral("Gameplay.Player")},
        {QStringLiteral("Камера Play"), QStringLiteral("Gameplay.Camera")},
        {QStringLiteral("Настройки Play"), QStringLiteral("Gameplay.PlaySettings")}
    };

    panel.SetPrimitiveTypes(registeredTypes);
    QApplication::processEvents();

    auto* addBtn = panel.findChild<QPushButton*>(QStringLiteral("SceneAddButton"));
    Check(addBtn != nullptr, "SceneAddButton exists");
    auto* menu = addBtn->menu();
    Check(menu != nullptr, "SceneAddButton has menu");

    // Check all submenus and actions
    int actionCount = 0;
    QString emittedTypeName;
    QObject::connect(&panel, &SceneObjectsPanel::AddPrimitiveRequested,
                     [&](const QString& typeName, const QString&, const std::array<double, 3>&) {
                         emittedTypeName = typeName;
                     });

    const auto subMenus = menu->findChildren<QMenu*>();
    Check(!subMenus.isEmpty(), "AddMenu has submenus");

    for (auto* sub : subMenus)
    {
        for (auto* action : sub->actions())
        {
            if (action->isSeparator()) continue;
            ++actionCount;
            Check(action->objectName().startsWith(QStringLiteral("CreateObject.")),
                  "Action objectName starts with CreateObject.");
            Check(!action->data().toString().isEmpty(), "Action has type name in data");
            Check(action->data().toString() != QStringLiteral("Gameplay.PlaySettings"),
                  "PlaySettings excluded from create menu");

            // Test triggering an action
            emittedTypeName.clear();
            action->trigger();
            Check(emittedTypeName == action->data().toString(),
                  "Triggering menu action emits AddPrimitiveRequested with matching type");
        }
    }

    // 14 creatable types (PlaySettings excluded)
    Check(actionCount == 14, "Exact 14 creatable actions generated in AddMenu");

    // Test with partial types: verify empty categories are hidden
    const QVector<std::pair<QString, QString>> partialTypes = {
        {QStringLiteral("Куб"), QStringLiteral("Primitive.Cube")},
        {QStringLiteral("Точечный свет"), QStringLiteral("Light.Point")}
    };
    panel.SetPrimitiveTypes(partialTypes);
    QApplication::processEvents();

    for (auto* sub : menu->findChildren<QMenu*>())
    {
        if (sub->actions().isEmpty())
        {
            Check(!sub->menuAction()->isVisible(), "Empty submenu is hidden");
        }
    }
}

inline void RunPhysicsColliderTests()
{
    std::cout << "[Test] Running ObjectTypeInspectorTests::PhysicsCollider..." << std::endl;

    ObjectInspectorPanel inspector;
    inspector.resize(320, 600);

    auto* bodyCombo = inspector.findChild<QComboBox*>(QStringLiteral("PhysicsCombo0"));
    auto* shapeCombo = inspector.findChild<QComboBox*>(QStringLiteral("PhysicsCombo1"));
    Check(bodyCombo != nullptr, "PhysicsCombo0 (body type) found");
    Check(shapeCombo != nullptr, "PhysicsCombo1 (collision shape) found");

    // Must have items on startup without manual language switch
    Check(bodyCombo->count() >= 3, "PhysicsCombo0 has at least 3 items on startup");
    Check(shapeCombo->count() == 4, "PhysicsCombo1 includes the Mesh shape on startup");

    // Provide snapshot with Mesh object with physics enabled
    EditorQtHost::Snapshot snap;
    EditorQtHost::SceneObject obj;
    obj.id = 42;
    obj.name = "PhysicsObj";
    obj.type = "Mesh";
    obj.hasPhysics = true;
    obj.physics.enabled = 1;
    obj.physics.bodyType = 1; // Dynamic
    obj.physics.shapeType = 0; // Box
    snap.objects.push_back(obj);
    snap.selected = 42;

    inspector.Refresh(snap, 42, true);
    QApplication::processEvents();

    Check(shapeCombo->currentIndex() == 0, "Initial shape index is 0 (Box)");
    Check(bodyCombo->currentIndex() == 1, "Initial body type index is 1 (Dynamic)");

    // Test changing collider shape
    uint64_t emittedId = 0;
    InspectorPhysicsValue emittedPhysics{};
    QObject::connect(&inspector, &ObjectInspectorPanel::PhysicsEditingFinished,
                     [&](quint64 id, const InspectorPhysicsValue& val) {
                         emittedId = id;
                         emittedPhysics = val;
                     });

    shapeCombo->setCurrentIndex(1); // Sphere
    Check(emittedId == 42, "PhysicsEditingFinished emitted with correct object id");
    Check(emittedPhysics.shapeType == 1, "Emitted physics shape changed to 1 (Sphere)");

    bodyCombo->setCurrentIndex(0); // Static
    Check(emittedPhysics.bodyType == 0, "Emitted physics bodyType changed to 0 (Static)");

    // Engine updates snapshot with new physics values
    snap.objects[0].physics.shapeType = 1;
    snap.objects[0].physics.bodyType = 0;
    inspector.Refresh(snap, 42, true);

    // Test language change preserves selection
    EditorLocale::Instance().SetLanguage(EditorLanguage::En);
    QApplication::processEvents();
    Check(shapeCombo->currentIndex() == 1, "Shape index preserved after EN language change");
    Check(bodyCombo->currentIndex() == 0, "Body type index preserved after EN language change");
    Check(shapeCombo->currentText() == QStringLiteral("Sphere"), "Shape text updated to EN Sphere");

    EditorLocale::Instance().SetLanguage(EditorLanguage::Ru);
    QApplication::processEvents();
    Check(shapeCombo->currentIndex() == 1, "Shape index preserved after RU language change");
    Check(bodyCombo->currentIndex() == 0, "Body type index preserved after RU language change");
    Check(shapeCombo->currentText() == QStringLiteral("Сфера"), "Shape text updated to RU Сфера");

    shapeCombo->setCurrentIndex(3);
    Check(emittedPhysics.shapeType == 3, "Mesh shape is emitted without clamping");
    snap.objects[0].physics.shapeType = 3;
    inspector.Refresh(snap, 42, true);
    Check(shapeCombo->currentIndex() == 3, "Mesh survives engine snapshot refresh");
    EditorLocale::Instance().SetLanguage(EditorLanguage::En);
    QApplication::processEvents();
    Check(shapeCombo->currentIndex() == 3 && shapeCombo->currentText() == QStringLiteral("Mesh (complex)"),
          "Mesh survives EN retranslation");
    EditorLocale::Instance().SetLanguage(EditorLanguage::Ru);
    QApplication::processEvents();
    Check(shapeCombo->currentIndex() == 3 && shapeCombo->currentText() == QStringLiteral("Меш (сложная)"),
          "Mesh survives RU retranslation");
}

inline void Run()
{
    RunMatrixTests();
    RunAddMenuTests();
    RunPhysicsColliderTests();
    std::cout << "ObjectTypeInspectorTests: all checks passed." << std::endl;
}

} // namespace ObjectTypeInspectorTests
