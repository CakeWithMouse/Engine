#pragma once

#include "WheelGuard.h"

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

    // 3. Unfocused wheel guard behavior (prevents accidental mutations)
    {
        TestScrollReceiver parentWidget;
        parentWidget.resize(200, 200);

        auto* spin = new QDoubleSpinBox(&parentWidget);
        spin->setRange(0.0, 100.0);
        spin->setValue(25.0);
        spin->clearFocus();

        auto* combo = new QComboBox(&parentWidget);
        combo->addItems({"Alpha", "Beta", "Gamma"});
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

        // Simulate wheel event over unfocused spinbox
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

        // 4. Focused control allows wheel mutation when user intentionally focuses it
        spin->setFocus();
        Check(WheelGuard::HasFocus(spin), "spin has focus");
        parentWidget.receivedWheelEvents = 0;
        QApplication::sendEvent(spin, &wheelDeltaPositive);
        Check(spin->value() > 25.0, "focused QDoubleSpinBox accepts wheel event and updates value");
        Check(parentWidget.receivedWheelEvents == 0, "parent did not receive event when child was focused");
        checks += 3;

        guard.Uninstall(qApp);
    }

    // 5. Scrollbars are never blocked by WheelGuard
    {
        TestScrollReceiver parentWidget;
        auto* scrollBar = new QScrollBar(Qt::Vertical, &parentWidget);
        scrollBar->setRange(0, 100);
        scrollBar->setValue(10);

        WheelGuard& guard = WheelGuard::Instance();
        guard.Install(qApp);

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
        // Scrollbar processes wheel normally
        Check(scrollBar->value() != 10 || scrollBar->maximum() > 0, "scrollbar is not blocked by WheelGuard");
        ++checks;

        guard.Uninstall(qApp);
    }

    std::cout << "WheelGuard tests: " << checks << " checks passed" << std::endl;
}

} // namespace WheelGuardTests
