#pragma once

#include "UiTestHelper.h"

#include <QWidget>
#include <QPushButton>
#include <QAction>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <iostream>

namespace UiTestHelperTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string("UiTestHelper check failed: ") + message);
    }
}

inline void Run()
{
    std::cout << "[Test] Running UiTestHelperTests..." << std::endl;
    int checks = 0;

    QWidget root;

    // 1. Find and Require
    auto* btn = new QPushButton(&root);
    btn->setObjectName(QStringLiteral("test_btn"));
    Check(UiTestHelper::Find<QPushButton>(&root, QStringLiteral("test_btn")) == btn, "Find matching objectName");
    ++checks;
    Check(UiTestHelper::Find<QPushButton>(&root, QStringLiteral("non_existent")) == nullptr, "Find non-existent is null");
    ++checks;
    Check(UiTestHelper::Find<QLineEdit>(&root, QStringLiteral("test_btn")) == nullptr, "Find wrong type is null");
    ++checks;
    Check(UiTestHelper::Require<QPushButton>(&root, QStringLiteral("test_btn")) == btn, "Require matching objectName");
    ++checks;

    bool threwMissing = false;
    try
    {
        UiTestHelper::Require<QPushButton>(&root, QStringLiteral("missing_btn"), "unit_test");
    }
    catch (const std::runtime_error&)
    {
        threwMissing = true;
    }
    Check(threwMissing, "Require throws std::runtime_error on missing");
    ++checks;

    // 2. ClickButton
    bool clicked = false;
    QObject::connect(btn, &QPushButton::clicked, &root, [&] { clicked = true; });
    UiTestHelper::ClickButton(&root, QStringLiteral("test_btn"));
    Check(clicked, "ClickButton triggers clicked signal");
    ++checks;

    btn->setEnabled(false);
    bool threwDisabledBtn = false;
    try
    {
        UiTestHelper::ClickButton(&root, QStringLiteral("test_btn"));
    }
    catch (const std::runtime_error&)
    {
        threwDisabledBtn = true;
    }
    Check(threwDisabledBtn, "ClickButton throws on disabled button");
    ++checks;

    // 3. TriggerAction
    auto* act = new QAction(&root);
    act->setObjectName(QStringLiteral("test_act"));
    bool triggered = false;
    QObject::connect(act, &QAction::triggered, &root, [&] { triggered = true; });
    UiTestHelper::TriggerAction(&root, QStringLiteral("test_act"));
    Check(triggered, "TriggerAction triggers action signal");
    ++checks;

    act->setEnabled(false);
    bool threwDisabledAct = false;
    try
    {
        UiTestHelper::TriggerAction(&root, QStringLiteral("test_act"));
    }
    catch (const std::runtime_error&)
    {
        threwDisabledAct = true;
    }
    Check(threwDisabledAct, "TriggerAction throws on disabled action");
    ++checks;

    // 4. SetLineEdit
    auto* edit = new QLineEdit(&root);
    edit->setObjectName(QStringLiteral("test_edit"));
    UiTestHelper::SetLineEdit(&root, QStringLiteral("test_edit"), QStringLiteral("EngineV2"));
    Check(edit->text() == QStringLiteral("EngineV2"), "SetLineEdit sets text");
    ++checks;

    // 5. SetSpinBox
    auto* spin = new QDoubleSpinBox(&root);
    spin->setObjectName(QStringLiteral("test_spin"));
    spin->setRange(-100.0, 100.0);
    UiTestHelper::SetSpinBox(&root, QStringLiteral("test_spin"), 12.34);
    Check(std::abs(spin->value() - 12.34) < 1e-4, "SetSpinBox sets value");
    ++checks;

    // 6. SelectComboBox
    auto* combo = new QComboBox(&root);
    combo->setObjectName(QStringLiteral("test_combo"));
    combo->addItem(QStringLiteral("First"), 100);
    combo->addItem(QStringLiteral("Second"), 200);
    UiTestHelper::SelectComboBox(&root, QStringLiteral("test_combo"), 1);
    Check(combo->currentIndex() == 1, "SelectComboBox by index");
    ++checks;

    UiTestHelper::SelectComboBoxData(&root, QStringLiteral("test_combo"), 100);
    Check(combo->currentIndex() == 0, "SelectComboBoxData by value");
    ++checks;

    // 7. WaitUntil
    int pumpCount = 0;
    bool ready = false;
    UiTestHelper::WaitUntil([&] { return ready; }, 1000, 1, [&] {
        if (++pumpCount >= 3)
            ready = true;
    }, "WaitUntil test");
    Check(ready && pumpCount >= 3, "WaitUntil succeeds with predicate");
    ++checks;

    bool waitTimeoutThrew = false;
    try
    {
        UiTestHelper::WaitUntil([] { return false; }, 50, 5, nullptr, "expected timeout");
    }
    catch (const std::runtime_error&)
    {
        waitTimeoutThrew = true;
    }
    Check(waitTimeoutThrew, "WaitUntil throws on timeout");
    ++checks;

    std::cout << "UiTestHelperTests: " << checks << " checks passed." << std::endl;
}

} // namespace UiTestHelperTests

