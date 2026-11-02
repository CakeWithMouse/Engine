#pragma once

#include "ObjectInspectorPanel.h"
#include "SceneObjectsPanel.h"
#include "MainToolBar.h"
#include "ViewportHeaderBar.h"
#include "EditorLocale.h"

#include <QWidget>
#include <QHeaderView>
#include <QTreeWidget>
#include <QLabel>
#include <QAbstractButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QAction>
#include <QLayout>
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
        QStringLiteral("Move Snap Step")
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

    // 4. Verify vertical centering of toolbar play buttons within 1px
    toolBar.resize(800, 36);
    toolBar.show();
    QCoreApplication::processEvents();
    for (const auto* name : {"BtnPlay", "BtnPause", "BtnStop", "BtnExternal"})
    {
        auto* button = toolBar.findChild<QAbstractButton*>(QString::fromLatin1(name));
        if (!button || !button->isVisible() || !button->parentWidget()->layout() ||
            button->parentWidget()->layout()->indexOf(button) < 0)
            throw std::runtime_error(std::string("UiSmoke: Missing toolbar control: ") + name);
        ++checks;
    }
    if (toolBar.findChild<QAbstractButton*>(QStringLiteral("ConfigPlayBtn")))
        throw std::runtime_error("UiSmoke: Play settings belong to player properties");
    ++checks;
    auto* playBtn = toolBar.findChild<QAbstractButton*>(QStringLiteral("BtnPlay"));
    if (playBtn)
    {
        const int tbHeight = toolBar.height();
        const int btnY = playBtn->geometry().y();
        const int btnH = playBtn->geometry().height();
        const int topMargin = btnY;
        const int bottomMargin = tbHeight - (btnY + btnH);
        const int diff = std::abs(topMargin - bottomMargin);
        if (diff > 1)
        {
            throw std::runtime_error("UiSmoke: Play button not vertically centered in toolbar (top=" +
                std::to_string(topMargin) + ", bottom=" + std::to_string(bottomMargin) +
                ", diff=" + std::to_string(diff) + " > 1px)");
        }
        ++checks;
    }

    // 5. Verify compact 20px header height
    auto* hierTree = sceneObjects.findChild<QTreeWidget*>(QStringLiteral("SceneHierarchy"));
    if (!hierTree || !hierTree->header() || hierTree->header()->maximumHeight() != 20)
    {
        throw std::runtime_error("UiSmoke: SceneHierarchy header height is not 20px");
    }
    ++checks;

    // Restore default Ru language
    EditorLocale::Instance().SetLanguage(EditorLanguage::Ru);

    std::cout << "UiSmokeTests: " << checks << " checks passed (" << cyrillicCount << " Cyrillic strings in RU mode)." << std::endl;
}

} // namespace UiSmokeTests

