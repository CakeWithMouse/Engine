#pragma once

#include "ObjectInspectorPanel.h"
#include "SceneObjectsPanel.h"
#include "MainToolBar.h"
#include "ViewportHeaderBar.h"
#include "EditorLocale.h"

#include <QWidget>
#include <QLabel>
#include <QAbstractButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QAction>
#include <vector>
#include <iostream>
#include <set>
#include <stdexcept>

namespace UiSmokeTests
{

inline bool HasCyrillic(const QString& s)
{
    for (const QChar& c : s)
    {
        const ushort u = c.unicode();
        if (u >= 0x0400 && u <= 0x04FF)
            return true;
    }
    return false;
}

inline void CollectTexts(QWidget* root, std::vector<QString>& outTexts)
{
    if (!root) return;
    for (auto* label : root->findChildren<QLabel*>()) {
        if (!label->text().isEmpty()) outTexts.push_back(label->text());
        if (!label->toolTip().isEmpty()) outTexts.push_back(label->toolTip());
    }
    for (auto* btn : root->findChildren<QAbstractButton*>()) {
        if (!btn->text().isEmpty()) outTexts.push_back(btn->text());
        if (!btn->toolTip().isEmpty()) outTexts.push_back(btn->toolTip());
    }
    for (auto* edit : root->findChildren<QLineEdit*>()) {
        if (!edit->placeholderText().isEmpty()) outTexts.push_back(edit->placeholderText());
        if (!edit->toolTip().isEmpty()) outTexts.push_back(edit->toolTip());
    }
    for (auto* box : root->findChildren<QGroupBox*>()) {
        if (!box->title().isEmpty()) outTexts.push_back(box->title());
    }
    for (auto* act : root->findChildren<QAction*>()) {
        if (!act->text().isEmpty()) outTexts.push_back(act->text());
        if (!act->toolTip().isEmpty()) outTexts.push_back(act->toolTip());
    }
    for (auto* act : root->actions()) {
        if (!act->text().isEmpty()) outTexts.push_back(act->text());
        if (!act->toolTip().isEmpty()) outTexts.push_back(act->toolTip());
    }
}

inline void Run()
{
    std::cout << "[Test] Running UiSmokeTests..." << std::endl;
    int checks = 0;

    // 1. Instantiate panels
    ObjectInspectorPanel inspector;
    SceneObjectsPanel sceneObjects;
    MainToolBar toolBar;
    ViewportHeaderBar headerBar;

    // 2. Test English mode: ensure no visible/tooltip texts contain Cyrillic
    EditorLocale::Instance().SetLanguage(EditorLanguage::En);
    inspector.RetranslateUi();
    sceneObjects.RetranslateUi();
    toolBar.RetranslateUi();
    headerBar.RetranslateUi();

    std::vector<QString> enTexts;
    CollectTexts(&inspector, enTexts);
    CollectTexts(&sceneObjects, enTexts);
    CollectTexts(&toolBar, enTexts);
    CollectTexts(&headerBar, enTexts);

    if (enTexts.empty()) {
        throw std::runtime_error("UiSmoke: No UI texts found in instantiated panels");
    }

    for (const auto& text : enTexts) {
        if (HasCyrillic(text)) {
            throw std::runtime_error("UiSmoke: Cyrillic character found in EN mode: '" + text.toStdString() + "'");
        }
        ++checks;
    }

    // 3. Test Russian mode: ensure texts are translated and no raw English keys remain
    EditorLocale::Instance().SetLanguage(EditorLanguage::Ru);
    inspector.RetranslateUi();
    sceneObjects.RetranslateUi();
    toolBar.RetranslateUi();
    headerBar.RetranslateUi();

    std::vector<QString> ruTexts;
    CollectTexts(&inspector, ruTexts);
    CollectTexts(&sceneObjects, ruTexts);
    CollectTexts(&toolBar, ruTexts);
    CollectTexts(&headerBar, ruTexts);

    const std::set<QString> enKeys = {
        QStringLiteral("No object selected"),
        QStringLiteral("General"),
        QStringLiteral("Transform"),
        QStringLiteral("Material"),
        QStringLiteral("Geometry"),
        QStringLiteral("Point Light"),
        QStringLiteral("Character"),
        QStringLiteral("Choose Model…"),
        QStringLiteral("Search objects..."),
        QStringLiteral("Translate (W/G)"),
        QStringLiteral("Rotate (E/R)"),
        QStringLiteral("Scale (R/S)"),
        QStringLiteral("Grid Snapping (Shift+S)"),
        QStringLiteral("Move Snap Step"),
        QStringLiteral("Detailed Play Settings…")
    };

    int cyrillicCount = 0;
    for (const auto& text : ruTexts) {
        if (enKeys.find(text) != enKeys.end()) {
            throw std::runtime_error("UiSmoke: Untranslated English table key found in RU mode: '" + text.toStdString() + "'");
        }
        if (HasCyrillic(text)) {
            ++cyrillicCount;
        }
        ++checks;
    }

    if (cyrillicCount == 0) {
        throw std::runtime_error("UiSmoke: Expected Cyrillic strings in RU mode, found 0");
    }

    // Restore default Ru language
    EditorLocale::Instance().SetLanguage(EditorLanguage::Ru);

    std::cout << "UiSmokeTests: " << checks << " checks passed (" << cyrillicCount << " Cyrillic strings in RU mode)." << std::endl;
}

} // namespace UiSmokeTests

