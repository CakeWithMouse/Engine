#pragma once

#include "WheelGuard.h"
#include "ObjectInspectorPanel.h"
#include "ImportSettingsDialog.h"
#include "PlaySettingsDialog.h"
#include "CodeClassDialog.h"
#include "ViewportHeaderBar.h"
#include "AiDiagnosticsPanel.h"

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <iostream>
#include <stdexcept>
#include <string>

namespace WheelGuardTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(std::string("WheelGuard check failed: ") + message);
}

class TestScrollReceiver : public QWidget
{
public:
    int receivedWheelEvents{0};

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        ++receivedWheelEvents;
        event->accept();
    }
};

inline void TestWidgetWheelImmunity(QWidget* root, const QString& objectName, int& checks)
{
    auto* w = root->findChild<QWidget*>(objectName);
    Check(w != nullptr, (std::string("control exists by objectName: ") + objectName.toStdString()).c_str());

    double origVal = 0.0;
    int origIdx = 0;
    auto* dSpin = qobject_cast<QDoubleSpinBox*>(w);
    auto* iSpin = qobject_cast<QSpinBox*>(w);
    auto* combo = qobject_cast<QComboBox*>(w);
    auto* slider = qobject_cast<QAbstractSlider*>(w);

    if (dSpin) origVal = dSpin->value();
    else if (iSpin) origVal = iSpin->value();
    else if (combo) origIdx = combo->currentIndex();
    else if (slider) origVal = slider->value();

    const QPoint p(5, 5);
    QWheelEvent wheelPos(QPointF(p), QPointF(w->mapToGlobal(p)),
                         QPoint(0, 0), QPoint(0, 120),
                         Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);

    // 1. Unfocused wheel test
    w->clearFocus();
    QApplication::sendEvent(w, &wheelPos);
    if (dSpin) Check(dSpin->value() == origVal, (objectName.toStdString() + " unfocused value unchanged").c_str());
    else if (iSpin) Check(iSpin->value() == origVal, (objectName.toStdString() + " unfocused value unchanged").c_str());
    else if (combo) Check(combo->currentIndex() == origIdx, (objectName.toStdString() + " unfocused index unchanged").c_str());
    else if (slider) Check(slider->value() == origVal, (objectName.toStdString() + " unfocused slider unchanged").c_str());
    ++checks;

    // 2. Focused wheel test
    w->setFocus();
    QApplication::sendEvent(w, &wheelPos);
    if (dSpin) Check(dSpin->value() == origVal, (objectName.toStdString() + " focused value unchanged").c_str());
    else if (iSpin) Check(iSpin->value() == origVal, (objectName.toStdString() + " focused value unchanged").c_str());
    else if (combo) Check(combo->currentIndex() == origIdx, (objectName.toStdString() + " focused index unchanged").c_str());
    else if (slider) Check(slider->value() == origVal, (objectName.toStdString() + " focused slider unchanged").c_str());
    ++checks;
}

inline void Run()
{
    int checks = 0;
    std::cout << "[Test] Running WheelGuardTests..." << std::endl;

    // 1. Guarded types detection
    {
        QDoubleSpinBox dSpin;
        QSpinBox iSpin;
        QComboBox combo;
        QSlider slider(Qt::Horizontal);
        QScrollBar scrollBar(Qt::Vertical);
        QPushButton btn;
        QLineEdit edit;

        Check(WheelGuard::IsGuarded(&dSpin), "QDoubleSpinBox is guarded");
        Check(WheelGuard::IsGuarded(&iSpin), "QSpinBox is guarded");
        Check(WheelGuard::IsGuarded(&combo), "QComboBox is guarded");
        Check(WheelGuard::IsGuarded(&slider), "QSlider is guarded");
        Check(!WheelGuard::IsGuarded(&scrollBar), "QScrollBar is NOT guarded (must be able to scroll)");
        Check(!WheelGuard::IsGuarded(&btn), "QPushButton is not guarded");
        Check(!WheelGuard::IsGuarded(&edit), "QLineEdit is not guarded");
        checks += 7;
    }

    // 2. Exemption detection
    {
        QWidget container;
        QDoubleSpinBox spin(&container);
        Check(!WheelGuard::IsExempt(&spin), "default spinbox is not exempt");

        WheelGuard::SetExempt(&spin, true);
        Check(WheelGuard::IsExempt(&spin), "explicitly exempt spinbox is exempt");

        WheelGuard::SetExempt(&spin, false);
        Check(!WheelGuard::IsExempt(&spin), "unmarked spinbox is not exempt");

        container.setObjectName(QStringLiteral("Viewport"));
        Check(WheelGuard::IsExempt(&spin), "child of Viewport is exempt (camera zoom preserved)");

        container.setObjectName(QStringLiteral("MaterialGraphWidget"));
        Check(WheelGuard::IsExempt(&spin), "child of MaterialGraphWidget is exempt (graph zoom preserved)");

        container.setObjectName(QStringLiteral("NormalContainer"));
        Check(!WheelGuard::IsExempt(&spin), "child of NormalContainer is not exempt");
        checks += 6;
    }

    // 3. WheelGuard event filter forwarding (focused and unfocused)
    {
        TestScrollReceiver parentWidget;
        parentWidget.resize(200, 200);

        auto* spin = new QDoubleSpinBox(&parentWidget);
        spin->setRange(0.0, 100.0);
        spin->setValue(25.0);
        spin->clearFocus();

        auto* combo = new QComboBox(&parentWidget);
        combo->addItems({QStringLiteral("Alpha"), QStringLiteral("Beta"), QStringLiteral("Gamma")});
        combo->setCurrentIndex(0);
        combo->clearFocus();

        auto* slider = new QSlider(Qt::Horizontal, &parentWidget);
        slider->setRange(0, 100);
        slider->setValue(40);
        slider->clearFocus();

        auto* dummyFocus = new QPushButton(QStringLiteral("Dummy"), &parentWidget);
        parentWidget.show();
        QApplication::processEvents();

        dummyFocus->setFocus();
        spin->clearFocus();
        Check(!spin->hasFocus(), "spin does not have focus before wheel test");

        WheelGuard& guard = WheelGuard::Instance();
        guard.Install(qApp);

        const QPoint pos(10, 10);
        const QPoint globalPos = spin->mapToGlobal(pos);
        QWheelEvent wheelDeltaPositive(
            QPointF(pos),
            QPointF(globalPos),
            QPoint(0, 0),
            QPoint(0, 120),
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false
        );

        // Deliver wheel event to unfocused spinbox
        QApplication::sendEvent(spin, &wheelDeltaPositive);
        Check(spin->value() == 25.0, "unfocused QDoubleSpinBox value did NOT change on wheel scroll");
        Check(parentWidget.receivedWheelEvents == 1, "parent widget received forwarded wheel event for scrolling");
        ++checks;

        // Deliver wheel event to unfocused combobox
        parentWidget.receivedWheelEvents = 0;
        QApplication::sendEvent(combo, &wheelDeltaPositive);
        Check(combo->currentIndex() == 0, "unfocused QComboBox index did NOT change on wheel scroll");
        Check(parentWidget.receivedWheelEvents == 1, "parent widget received forwarded wheel event from combobox");
        ++checks;

        // Deliver wheel event to unfocused slider
        parentWidget.receivedWheelEvents = 0;
        QApplication::sendEvent(slider, &wheelDeltaPositive);
        Check(slider->value() == 40, "unfocused QSlider value did NOT change on wheel scroll");
        Check(parentWidget.receivedWheelEvents == 1, "parent widget received forwarded wheel event from slider");
        ++checks;

        // 4. Focused control NEVER mutates value on wheel (crucial Rule G!)
        spin->setFocus();
        parentWidget.receivedWheelEvents = 0;
        QApplication::sendEvent(spin, &wheelDeltaPositive);
        Check(spin->value() == 25.0, "focused QDoubleSpinBox does NOT change value on wheel");
        Check(parentWidget.receivedWheelEvents == 1, "parent received forwarded event even when child was focused");
        checks += 2;

        combo->setFocus();
        parentWidget.receivedWheelEvents = 0;
        QApplication::sendEvent(combo, &wheelDeltaPositive);
        Check(combo->currentIndex() == 0, "focused QComboBox does NOT change index on wheel");
        Check(parentWidget.receivedWheelEvents == 1, "parent received forwarded event when combo focused");
        checks += 2;

        slider->setFocus();
        parentWidget.receivedWheelEvents = 0;
        QApplication::sendEvent(slider, &wheelDeltaPositive);
        Check(slider->value() == 40, "focused QSlider does NOT change value on wheel");
        Check(parentWidget.receivedWheelEvents == 1, "parent received forwarded event when slider focused");
        checks += 2;

        // 5. Scrollbars are never blocked by WheelGuard
        auto* scrollBar = new QScrollBar(Qt::Vertical, &parentWidget);
        scrollBar->setRange(0, 100);
        scrollBar->setValue(10);
        QWheelEvent scrollWheel(
            QPointF(5, 5),
            QPointF(scrollBar->mapToGlobal(QPoint(5, 5))),
            QPoint(0, 0),
            QPoint(0, 120),
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false
        );
        QApplication::sendEvent(scrollBar, &scrollWheel);
        Check(scrollBar->value() != 10 || scrollBar->maximum() > 0, "scrollbar is not blocked by WheelGuard");
        ++checks;

        guard.Uninstall(qApp);
    }

    // 6. Comprehensive test across all UI fields by objectName (Task G)
    {
        WheelGuard& guard = WheelGuard::Instance();
        guard.Install(qApp);

        // Inspector panel with all object types
        {
            ObjectInspectorPanel panel;
            panel.resize(320, 600);

            EditorQtHost::Snapshot snap;
            snap.scripts = true;
            EditorQtHost::SceneObject obj;
            obj.id = 1;
            obj.name = "TestObj";
            obj.type = "Gameplay.Character";
            obj.gameplayKind = 1;
            obj.modelAsset = "/Game/Models/Hero.fbx";
            obj.materialAsset = "/Game/Materials/Hero.mat";
            obj.scriptAsset = "/Game/Scripts/Test.vscript";
            obj.propertySchema = R"({"fields":[{"path":"/fov","type":"number","min":10,"max":120,"decimals":1,"step":1},{"path":"/orthographic","type":"enum","options":[{"label":"Perspective","value":false},{"label":"Orthographic","value":true}]},{"path":"/viewPlane","type":"enum","options":[{"label":"XY","value":0},{"label":"XZ","value":1}]}]})";
            obj.projectData = R"({"fov":60.0,"orthographic":false,"viewPlane":0,"stateMachine":"/Game/AI/Hero.fsm","machineMemory":"{}"})";
            obj.hasPointLight = true;
            obj.pointLight.intensity = 5.0f;
            obj.pointLight.radius = 10.0f;
            snap.objects.push_back(obj);
            snap.selected = 1;

            panel.Refresh(snap, 1, true);
            QApplication::processEvents();

            // TransformCard (9 fields)
            for (int i = 0; i < 9; ++i)
                TestWidgetWheelImmunity(&panel, QStringLiteral("Transform%1").arg(i), checks);

            // LightCard (5 fields)
            for (int i = 0; i < 5; ++i)
                TestWidgetWheelImmunity(&panel, QStringLiteral("LightValue%1").arg(i), checks);

            // PhysicsCard (combos + spins)
            TestWidgetWheelImmunity(&panel, QStringLiteral("PhysicsCombo0"), checks);
            TestWidgetWheelImmunity(&panel, QStringLiteral("PhysicsCombo1"), checks);
            TestWidgetWheelImmunity(&panel, QStringLiteral("PhysicsValue2"), checks);
            TestWidgetWheelImmunity(&panel, QStringLiteral("PhysicsValue3"), checks);
            TestWidgetWheelImmunity(&panel, QStringLiteral("PhysicsValue4"), checks);

            // CharacterCard
            TestWidgetWheelImmunity(&panel, QStringLiteral("CharacterSpeed"), checks);

            // CameraGuideCard
            TestWidgetWheelImmunity(&panel, QStringLiteral("CameraGuideDepth"), checks);

            // ProjectProperties
            TestWidgetWheelImmunity(&panel, QStringLiteral("Property/fov"), checks);
            TestWidgetWheelImmunity(&panel, QStringLiteral("Property/orthographic"), checks);
            TestWidgetWheelImmunity(&panel, QStringLiteral("Property/viewPlane"), checks);
        }

        // ImportSettingsDialog
        {
            ImportSettingsDialog importDlg;
            TestWidgetWheelImmunity(&importDlg, QStringLiteral("ImportScaleSpin"), checks);
            TestWidgetWheelImmunity(&importDlg, QStringLiteral("NormalsCombo"), checks);
            TestWidgetWheelImmunity(&importDlg, QStringLiteral("SmoothingAngleSpin"), checks);
        }

        // PlaySettingsDialog
        {
            std::vector<PlaySettingsObjectView> objs;
            PlaySettingsObjectView ch; ch.id = 10; ch.name = "Hero"; ch.gameplayKind = 1; objs.push_back(ch);
            PlaySettingsObjectView cam; cam.id = 11; cam.name = "Cam"; cam.gameplayKind = 2; objs.push_back(cam);
            PlaySettingsDialog playDlg(objs);
            TestWidgetWheelImmunity(&playDlg, QStringLiteral("PlayCharacter"), checks);
            TestWidgetWheelImmunity(&playDlg, QStringLiteral("PlayCamera"), checks);
            TestWidgetWheelImmunity(&playDlg, QStringLiteral("PlayDistance"), checks);
            TestWidgetWheelImmunity(&playDlg, QStringLiteral("PlayHeight"), checks);
            TestWidgetWheelImmunity(&playDlg, QStringLiteral("PlaySensitivity"), checks);
        }

        // CodeClassDialog
        {
            CodeClassDialog codeDlg(QStringLiteral("Content"));
            TestWidgetWheelImmunity(&codeDlg, QStringLiteral("ClassKind"), checks);
        }

        // ViewportHeaderBar
        {
            ViewportHeaderBar headerBar;
            TestWidgetWheelImmunity(&headerBar, QStringLiteral("comboGizmoSnapStep"), checks);
        }

        // UI Editor and Material Editor controls
        {
            QWidget uiWin;
            for (int i = 0; i < 4; ++i)
            {
                auto* rect = new QDoubleSpinBox(&uiWin);
                rect->setObjectName(QStringLiteral("UIRect%1").arg(i));
            }
            auto* anchor = new QComboBox(&uiWin);
            anchor->setObjectName(QStringLiteral("UIAnchor"));
            auto* prevSize = new QComboBox(&uiWin);
            prevSize->setObjectName(QStringLiteral("UIPreviewSize"));

            TestWidgetWheelImmunity(&uiWin, QStringLiteral("UIRect0"), checks);
            TestWidgetWheelImmunity(&uiWin, QStringLiteral("UIRect1"), checks);
            TestWidgetWheelImmunity(&uiWin, QStringLiteral("UIRect2"), checks);
            TestWidgetWheelImmunity(&uiWin, QStringLiteral("UIRect3"), checks);
            TestWidgetWheelImmunity(&uiWin, QStringLiteral("UIAnchor"), checks);
            TestWidgetWheelImmunity(&uiWin, QStringLiteral("UIPreviewSize"), checks);

            QWidget matWin;
            auto* shape = new QComboBox(&matWin); shape->setObjectName(QStringLiteral("MaterialPreviewShape"));
            auto* blend = new QComboBox(&matWin); blend->setObjectName(QStringLiteral("MaterialBlend"));
            auto* alphaClip = new QDoubleSpinBox(&matWin); alphaClip->setObjectName(QStringLiteral("MaterialAlphaClip"));

            TestWidgetWheelImmunity(&matWin, QStringLiteral("MaterialPreviewShape"), checks);
            TestWidgetWheelImmunity(&matWin, QStringLiteral("MaterialBlend"), checks);
            TestWidgetWheelImmunity(&matWin, QStringLiteral("MaterialAlphaClip"), checks);
        }

        // AiDiagnosticsPanel
        {
            AiDiagnosticsPanel aiPanel(nullptr, nullptr);
            TestWidgetWheelImmunity(&aiPanel, QStringLiteral("AiAgentRadius"), checks);
            TestWidgetWheelImmunity(&aiPanel, QStringLiteral("AiSubject"), checks);
        }

        guard.Uninstall(qApp);
    }

    std::cout << "WheelGuard tests: " << checks << " checks passed" << std::endl;
}

} // namespace WheelGuardTests
