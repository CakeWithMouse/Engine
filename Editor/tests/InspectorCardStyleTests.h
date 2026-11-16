#pragma once

#include "ObjectInspectorPanel.h"
#include "CollapsibleCard.h"
#include "InspectorCardStyle.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <iostream>
#include <stdexcept>

namespace InspectorCardStyleTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(std::string("InspectorCardStyle check failed: ") + message);
}

inline void Run()
{
    int checks = 0;
    std::cout << "[Test] Running InspectorCardStyleTests..." << std::endl;

    ObjectInspectorPanel panel;
    panel.resize(320, 600);

    // Provide snapshot with point light and model so cards are populated
    EditorQtHost::Snapshot snap;
    EditorQtHost::SceneObject obj;
    obj.id = 1;
    obj.name = "TestObject";
    obj.type = "Light.Point";
    obj.modelAsset = "/Game/Models/Hero.fbx";
    obj.materialAsset = "/Game/Materials/Hero.mat";
    obj.hasPointLight = true;
    obj.pointLight.intensity = 5.0f;
    obj.pointLight.radius = 10.0f;
    snap.objects.push_back(obj);
    snap.selected = 1;

    panel.Refresh(snap, 1, true);
    QApplication::processEvents();

    // 1. Verify all CollapsibleCards have identical content margins and spacing
    const auto cards = panel.findChildren<QWidget*>(QStringLiteral("CollapsibleCard"));
    Check(!cards.isEmpty(), "CollapsibleCards found in inspector panel");
    Check(cards.size() >= 5, "At least 5 CollapsibleCards found");
    ++checks;

    for (auto* card : cards)
    {
        auto* header = card->findChild<QPushButton*>(QStringLiteral("CardHeaderBtn"));
        Check(header != nullptr, "CardHeaderBtn exists in CollapsibleCard");
        Check(header->maximumHeight() == InspectorCardStyle::kHeaderHeight, "CardHeaderBtn height == 22");

        auto* content = card->findChild<QWidget*>(QStringLiteral("CardContent"));
        Check(content != nullptr, "CardContent exists in CollapsibleCard");
        auto* layout = content->layout();
        Check(layout != nullptr, "Card ContentLayout exists");
        Check(layout->contentsMargins() == InspectorCardStyle::CardMargins(), "Card content margins == (6, 4, 6, 4)");
        Check(layout->spacing() == InspectorCardStyle::kCardSpacing, "Card content spacing == 4");
        ++checks;
    }

    // 2. Verify all CardFieldLabel widgets have unified width (kLabelWidth = 80)
    const auto labels = panel.findChildren<QLabel*>(QStringLiteral("CardFieldLabel"));
    Check(!labels.isEmpty(), "CardFieldLabels found in inspector");
    Check(labels.size() >= 5, "At least 5 CardFieldLabels found");
    ++checks;

    for (auto* lbl : labels)
    {
        Check(lbl->minimumWidth() == InspectorCardStyle::kLabelWidth, "CardFieldLabel minimumWidth == 80");
        Check(lbl->maximumWidth() == InspectorCardStyle::kLabelWidth, "CardFieldLabel maximumWidth == 80");
        ++checks;
    }

    // 3. Verify specific field heights (22px) and button heights (24px)
    auto checkFieldHeight = [&](const QString& objectName) {
        auto* w = panel.findChild<QWidget*>(objectName);
        Check(w != nullptr, (objectName + " field found by objectName").toLatin1().constData());
        Check(w->maximumHeight() >= 22 && w->maximumHeight() <= 26, (objectName + " field height is compact (22-26px)").toLatin1().constData());
        ++checks;
    };

    auto checkButtonHeight = [&](const QString& objectName) {
        auto* b = panel.findChild<QPushButton*>(objectName);
        Check(b != nullptr, (objectName + " button found by objectName").toLatin1().constData());
        Check(b->maximumHeight() >= 24 && b->maximumHeight() <= 32, (objectName + " button height is compact (24-32px)").toLatin1().constData());
        ++checks;
    };

    checkFieldHeight(QStringLiteral("ModelAsset"));
    checkFieldHeight(QStringLiteral("MaterialAsset"));
    checkFieldHeight(QStringLiteral("ScriptAsset"));
    checkFieldHeight(QStringLiteral("AiMachine"));
    checkFieldHeight(QStringLiteral("CharacterSpeed"));

    checkButtonHeight(QStringLiteral("ChooseModel"));
    checkButtonHeight(QStringLiteral("ChooseMaterial"));
    checkButtonHeight(QStringLiteral("EditMaterial"));
    checkButtonHeight(QStringLiteral("ClearMaterial"));
    checkButtonHeight(QStringLiteral("OpenScript"));
    checkButtonHeight(QStringLiteral("ChooseScript"));
    checkButtonHeight(QStringLiteral("ClearScript"));
    checkButtonHeight(QStringLiteral("AiMachineOpen"));
    checkButtonHeight(QStringLiteral("AiMachineChoose"));
    checkButtonHeight(QStringLiteral("AiMachineClear"));
    checkButtonHeight(QStringLiteral("LightColor"));

    std::cout << "InspectorCardStyle tests: " << checks << " checks passed" << std::endl;
}

} // namespace InspectorCardStyleTests
