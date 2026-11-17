#pragma once

#include "ObjectInspectorPanel.h"
#include "CollapsibleCard.h"
#include "InspectorCardStyle.h"
#include "CameraGuideCard.h"
#include "AiDiagnosticsPanel.h"
#include "InfoButton.h"
#include "PropertySection.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QToolButton>
#include <QDir>
#include <QImage>
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
    snap.scripts = true;
    EditorQtHost::SceneObject obj;
    obj.id = 1;
    obj.name = "TestObject";
    obj.type = "Light.Point";
    obj.modelAsset = "/Game/Models/Hero.fbx";
    obj.materialAsset = "/Game/Materials/Hero.mat";
    obj.scriptAsset = "/Game/Scripts/Test.vscript";
    obj.propertySchema = R"({"fields":[{"path":"/stateMachine","type":"string"},{"path":"/machineMemory","type":"string"}]})";
    obj.projectData = R"({"stateMachine":"/Game/AI/Hero.fsm","machineMemory":"{}"})";
    obj.hasPointLight = true;
    obj.pointLight.intensity = 5.0f;
    obj.pointLight.radius = 10.0f;
    snap.objects.push_back(obj);
    snap.selected = 1;

    panel.Refresh(snap, 1, true);
    QApplication::processEvents();

    // 1. Verify all CollapsibleCards have identical content margins, spacing, and background styling
    const QStringList cardNames = {
        QStringLiteral("GeneralCard"),
        QStringLiteral("TransformCard"),
        QStringLiteral("GeometryCard"),
        QStringLiteral("MaterialCard"),
        QStringLiteral("LightCard"),
        QStringLiteral("CharacterCard"),
        QStringLiteral("PhysicsCard"),
        QStringLiteral("ScriptCard"),
        QStringLiteral("AiMachineCard"),
        QStringLiteral("CameraGuideCard")
    };

    for (const auto& name : cardNames)
    {
        auto* card = panel.findChild<QWidget*>(name);
        Check(card != nullptr, (name + " found in inspector panel").toLatin1().constData());
        auto* coll = dynamic_cast<CollapsibleCard*>(card);
        Check(coll != nullptr, (name + " is a CollapsibleCard").toLatin1().constData());
        Check(card->property("collapsibleCard").toBool() == true, (name + " has collapsibleCard property").toLatin1().constData());
        auto* header = card->findChild<QPushButton*>(QStringLiteral("CardHeaderBtn"));
        Check(header != nullptr, (name + " has CardHeaderBtn").toLatin1().constData());
        Check(header->maximumHeight() == InspectorCardStyle::kHeaderHeight, (name + " CardHeaderBtn height == 22").toLatin1().constData());
        auto* content = card->findChild<QWidget*>(QStringLiteral("CardContent"));
        Check(content != nullptr, (name + " has CardContent").toLatin1().constData());
        auto* layout = content->layout();
        Check(layout != nullptr, (name + " ContentLayout exists").toLatin1().constData());
        Check(layout->contentsMargins() == InspectorCardStyle::CardMargins(), (name + " content margins == (6, 4, 6, 4)").toLatin1().constData());
        Check(layout->spacing() == InspectorCardStyle::kCardSpacing, (name + " content spacing == 4").toLatin1().constData());
        checks += 9;
    }

    QList<QWidget*> cards;
    for (const auto& name : cardNames)
    {
        if (auto* w = panel.findChild<QWidget*>(name))
            cards.append(w);
    }
    Check(cards.size() == 10, "All 10 inspector CollapsibleCards found");
    ++checks;

    const QColor darkBg = EditorTheme::Instance().Palette().bgPanel;

    for (auto* card : cards)
    {
        Check(card->property("collapsibleCard").toBool() == true, "card has collapsibleCard property");
        auto* header = card->findChild<QPushButton*>(QStringLiteral("CardHeaderBtn"));
        Check(header != nullptr, "CardHeaderBtn exists in CollapsibleCard");
        Check(header->maximumHeight() == InspectorCardStyle::kHeaderHeight, "CardHeaderBtn height == 22");

        auto* content = card->findChild<QWidget*>(QStringLiteral("CardContent"));
        Check(content != nullptr, "CardContent exists in CollapsibleCard");
        auto* layout = content->layout();
        Check(layout != nullptr, "Card ContentLayout exists");
        Check(layout->contentsMargins() == InspectorCardStyle::CardMargins(), "Card content margins == (6, 4, 6, 4)");
        Check(layout->spacing() == InspectorCardStyle::kCardSpacing, "Card content spacing == 4");

        // Verify body background styling via render
        card->resize(260, 60);
        if (card->layout()) card->layout()->activate();
        QImage imgDark(260, 60, QImage::Format_ARGB32_Premultiplied);
        imgDark.fill(Qt::transparent);
        card->render(&imgDark);
        const QColor pixelDark = imgDark.pixelColor(3, 30);
        Check(pixelDark.alpha() > 200, "card body has non-transparent background");
        Check(pixelDark.rgb() == darkBg.rgb(), "card body background matches theme bgPanel");

        ++checks;
    }

    // Verify Light theme card background
    EditorTheme::Instance().SetMode(ThemeMode::Light);
    QApplication::processEvents();
    const QColor lightBg = EditorTheme::Instance().Palette().bgPanel;
    for (auto* card : cards)
    {
        card->ensurePolished();
        QImage imgLight(260, 60, QImage::Format_ARGB32_Premultiplied);
        imgLight.fill(Qt::transparent);
        card->render(&imgLight);
        const QColor pixelLight = imgLight.pixelColor(3, 30);
        Check(pixelLight.alpha() > 200, "card body has non-transparent background in light theme");
        Check(pixelLight.rgb() == lightBg.rgb(), "card body background matches light theme bgPanel");
        Check(pixelLight.rgb() != darkBg.rgb(), "card background changes with theme");
        ++checks;
    }
    EditorTheme::Instance().SetMode(ThemeMode::Dark);
    QApplication::processEvents();

    // Negative check: verify that a plain unstyled QFrame does not receive card background styling
    {
        QFrame unstyledFrame;
        unstyledFrame.resize(260, 60);
        QImage unstyledImg(260, 60, QImage::Format_ARGB32_Premultiplied);
        unstyledImg.fill(Qt::transparent);
        unstyledFrame.render(&unstyledImg);
        Check(unstyledImg.pixelColor(10, 30).rgb() != darkBg.rgb(), "unstyled frame does not receive card background styling");
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

    // 4. Test MaterialCard Create vs Edit button dynamic toggle (Task D)
    {
        MaterialCard matCard;
        matCard.resize(260, 100);
        auto* btn = matCard.findChild<QPushButton*>(QStringLiteral("CreateMaterial"));
        Check(btn != nullptr, "initial MaterialCard has CreateMaterial button");
        Check(btn->text().contains(QStringLiteral("Создать")) || btn->text().contains(QStringLiteral("Create")), "initial button text is Create");

        matCard.SetData(QStringLiteral("/Game/Materials/Custom.mat"), false);
        auto* editBtn = matCard.findChild<QPushButton*>(QStringLiteral("EditMaterial"));
        Check(editBtn != nullptr, "assigned MaterialCard has EditMaterial button");
        Check(editBtn->text().contains(QStringLiteral("Редактировать")) || editBtn->text().contains(QStringLiteral("Edit")), "assigned button text is Edit Material");

        matCard.Clear();
        auto* createAgain = matCard.findChild<QPushButton*>(QStringLiteral("CreateMaterial"));
        Check(createAgain != nullptr, "cleared MaterialCard has CreateMaterial button");
        Check(createAgain->text().contains(QStringLiteral("Создать")) || createAgain->text().contains(QStringLiteral("Create")), "cleared button text is Create");
        checks += 6;
    }

    // 5. Test CameraGuideCard InfoButton and Metrics Card (Task R)
    {
        CameraGuideCard camCard;
        camCard.resize(260, 200);
        auto* toolBtn = camCard.findChild<QToolButton*>(QStringLiteral("CameraGuideInfo"));
        Check(toolBtn != nullptr, "CameraGuideCard has CameraGuideInfo tool button");
        auto* infoBtn = dynamic_cast<InfoButton*>(toolBtn);
        Check(infoBtn != nullptr, "CameraGuideCard has CameraGuideInfo InfoButton");
        Check(infoBtn->objectName() == QStringLiteral("CameraGuideInfo"), "CameraGuideInfo objectName is correct");
        Check(infoBtn->CurrentText().contains(QStringLiteral("Зелёная")) || infoBtn->CurrentText().contains(QStringLiteral("Green")),
              "CameraGuideInfo contains guide color explanation");

        // Verify click on InfoButton does NOT toggle card expansion
        Check(camCard.IsExpanded() == true, "CameraGuideCard is expanded initially");
        infoBtn->click();
        Check(camCard.IsExpanded() == true, "Click on InfoButton does NOT collapse card");

        // Verify metrics card exists and is collapsed by default
        auto* metrics = camCard.MetricsCard();
        Check(metrics != nullptr, "CameraGuideCard has MetricsCard");
        Check(metrics->objectName() == QStringLiteral("CameraGuideMetricsCard"), "MetricsCard objectName is correct");
        Check(!metrics->IsExpanded(), "CameraGuideMetricsCard is collapsed by default");

        // Verify metric value labels exist
        const QStringList metricValNames = {
            QStringLiteral("CameraGuideProjection"),
            QStringLiteral("CameraGuideNearFar"),
            QStringLiteral("CameraGuideFrame"),
            QStringLiteral("CameraGuideTicks"),
            QStringLiteral("CameraGuideObserver"),
            QStringLiteral("CameraGuidePlane")
        };
        for (const auto& mName : metricValNames)
        {
            auto* lbl = metrics->findChild<QLabel*>(mName);
            Check(lbl != nullptr, (mName + " metric label found").toLatin1().constData());
            ++checks;
        }

        // Verify old multi-line summary label was removed
        auto* summary = camCard.findChild<QLabel*>(QStringLiteral("CameraGuideSummary"));
        Check(summary == nullptr, "CameraGuideSummary was removed in favor of metrics card");
        checks += 8;
    }

    // 6. Test InspectorCardStyle::AddInfoToRow helper (Task R)
    {
        QWidget dummy;
        auto* row = new QHBoxLayout(&dummy);
        auto* btn = InspectorCardStyle::AddInfoToRow(row, QStringLiteral("Текст справки"), QStringLiteral("Заголовок"));
        Check(btn != nullptr, "AddInfoToRow returned InfoButton");
        Check(btn->objectName() == QStringLiteral("RowInfoButton"), "RowInfoButton has correct objectName");
        Check(btn->CurrentTitle() == QStringLiteral("Заголовок"), "RowInfoButton has correct title");
        Check(btn->CurrentText() == QStringLiteral("Текст справки"), "RowInfoButton has correct text");
        checks += 4;
    }

    // 7. Test AiDiagnosticsPanel scroll area and card layout (Task R)
    {
        AiDiagnosticsPanel aiPanel(nullptr, nullptr);
        aiPanel.resize(320, 600);
        auto* scroll = aiPanel.findChild<QScrollArea*>(QStringLiteral("AiDiagnosticsScrollArea"));
        Check(scroll != nullptr, "AiDiagnosticsPanel has QScrollArea wrapping content");
        Check(scroll->widgetResizable() == true, "QScrollArea is widgetResizable");
        auto* content = aiPanel.findChild<QWidget*>(QStringLiteral("AiDiagnosticsScrollContent"));
        Check(content != nullptr, "QScrollArea contains scroll content widget");
        auto* detailsCard = aiPanel.findChild<QWidget*>(QStringLiteral("AiDetailsCard"));
        Check(detailsCard != nullptr, "AiDetailsCard exists in scroll content");
        auto* cellDetails = aiPanel.findChild<QWidget*>(QStringLiteral("AiCellDetails"));
        Check(cellDetails != nullptr, "AiCellDetails widget exists");
        Check(!cellDetails->isVisible(), "AiCellDetails is hidden when cursor is not hovering");
        checks += 6;
    }

    // 8. Schema sections (PropertySection) must carry the same card chrome as CollapsibleCard
    {
        ObjectInspectorPanel probe;
        probe.resize(320, 600);
        EditorQtHost::Snapshot probeSnap;
        probeSnap.scripts = true;
        EditorQtHost::SceneObject probeObj;
        probeObj.id = 7;
        probeObj.name = "Section probe";
        probeObj.type = "UiProbe";
        probeObj.scriptAsset = "/Game/Scripts/Test.vscript";
        probeObj.propertySchema = R"({"fields":[{"path":"/speed","type":"number","group":"Behavior/Movement"},{"path":"/weight","type":"number"}]})";
        probeObj.projectData = R"({"speed":5,"weight":2})";
        probeSnap.objects.push_back(probeObj);
        probeSnap.selected = 7;
        probe.Refresh(probeSnap, 7, true);
        QApplication::processEvents();

        QToolButton* sectionHeader = nullptr;
        for (auto* tool : probe.findChildren<QToolButton*>())
        {
            const QString name = tool->objectName();
            if (name.startsWith(QStringLiteral("Section.UiProbe/")) && name.count(QLatin1Char('/')) == 1)
                sectionHeader = tool;
        }
        Check(sectionHeader != nullptr, "default-group schema section header exists");
        auto* section = dynamic_cast<PropertySection*>(sectionHeader->parentWidget());
        Check(section != nullptr, "section header parent is PropertySection");
        Check(dynamic_cast<QFrame*>(section) != nullptr, "PropertySection is a QFrame so the QSS card box paints");
        Check(section->property("collapsibleCard").toBool() == true, "PropertySection has collapsibleCard property");
        Check(sectionHeader->maximumHeight() == InspectorCardStyle::kHeaderHeight, "section header height == 22");
        auto* bodyLayout = section->body->layout();
        Check(bodyLayout != nullptr && bodyLayout->contentsMargins() == InspectorCardStyle::CardMargins(), "section body margins == (6, 4, 6, 4)");
        Check(bodyLayout != nullptr && bodyLayout->spacing() == InspectorCardStyle::kCardSpacing, "section body spacing == 4");
        checks += 7;

        auto* reference = probe.findChild<QWidget*>(QStringLiteral("GeneralCard"));
        Check(reference != nullptr, "reference GeneralCard found");
        auto* refHeader = reference->findChild<QPushButton*>(QStringLiteral("CardHeaderBtn"));
        section->resize(260, 60);
        reference->resize(260, 60);
        if (section->layout()) section->layout()->activate();
        if (reference->layout()) reference->layout()->activate();
        Check(sectionHeader->height() == refHeader->height(), "section header height matches card header");
        Check(sectionHeader->x() == refHeader->x() && sectionHeader->y() == refHeader->y(), "section header inset matches card header");
        QImage sectionImg(260, 60, QImage::Format_ARGB32_Premultiplied);
        sectionImg.fill(Qt::transparent);
        section->render(&sectionImg);
        QImage referenceImg(260, 60, QImage::Format_ARGB32_Premultiplied);
        referenceImg.fill(Qt::transparent);
        reference->render(&referenceImg);
        const QList<QPoint> chromePoints = { QPoint(0, 30), QPoint(3, 30), QPoint(130, 0), QPoint(200, 10), QPoint(200, 21), QPoint(259, 30) };
        for (const auto& p : chromePoints)
        {
            Check(sectionImg.pixelColor(p) == referenceImg.pixelColor(p), "section chrome pixel matches CollapsibleCard");
            ++checks;
        }
        checks += 3;

        auto* scrollContent = probe.findChild<QWidget*>(QStringLiteral("InspectorScrollContent"));
        Check(scrollContent != nullptr, "inspector scroll content found");
        scrollContent->resize(320, scrollContent->sizeHint().height());
        if (scrollContent->layout()) scrollContent->layout()->activate();
        QImage shot(scrollContent->size(), QImage::Format_ARGB32_Premultiplied);
        shot.fill(Qt::transparent);
        scrollContent->render(&shot);
        const QString shotPath = QDir::tempPath() + QStringLiteral("/inspector-section-parity.png");
        Check(shot.save(shotPath), "inspector parity screenshot saved");
        std::cout << "Inspector parity screenshot: " << shotPath.toStdString() << std::endl;
        ++checks;
    }

    std::cout << "InspectorCardStyle tests: " << checks << " checks passed" << std::endl;
}

} // namespace InspectorCardStyleTests
