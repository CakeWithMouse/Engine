#include "SceneObjectType.h"
#include "ObjectInspectorPanel.h"
#include "CollapsibleCard.h"
#include "TransformCard.h"
#include "MaterialCard.h"
#include "ScriptCard.h"
#include "AiMachineCard.h"
#include "GeometryCard.h"
#include "CommandRegistry.h"
#include "NoWheelDoubleSpinBox.h"
#include "EditorLocale.h"
#include "IconProvider.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QQuaternion>
#include <QScrollArea>
#include <QVBoxLayout>

ObjectInspectorPanel::ObjectInspectorPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildUi();
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });
    themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
        if (lightColor_) lightColor_->setIcon(IconProvider::BulbIcon());
    });
}

ObjectInspectorPanel::~ObjectInspectorPanel()
{
    if (langSubId_) EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
    if (themeSubId_) EditorTheme::Instance().UnsubscribeThemeChanged(themeSubId_);
}

void ObjectInspectorPanel::SetEnabled(bool enabled)
{
    setEnabled(enabled);
}

void ObjectInspectorPanel::SetMaterialsAvailable(bool available)
{
    if (cardMaterial_)
        cardMaterial_->SetMaterialsAvailable(available);
}

void ObjectInspectorPanel::BuildUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setObjectName(QStringLiteral("InspectorScrollArea"));

    auto* content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("InspectorScrollContent"));
    auto* inspectorLayout = new QVBoxLayout(content);
    inspectorLayout->setContentsMargins(4, 4, 4, 4);
    inspectorLayout->setSpacing(4);

    noSelectionLabel_ = new QLabel(Tr("Объект не выбран", "No object selected"), content);
    noSelectionLabel_->setObjectName(QStringLiteral("NoSelectionLabel"));
    noSelectionLabel_->setAlignment(Qt::AlignCenter);
    inspectorLayout->addWidget(noSelectionLabel_);

    properties_ = new QWidget(content);
    propertiesLayout_ = new QVBoxLayout(properties_);
    propertiesLayout_->setContentsMargins(0,0,0,0);
    properties_->hide();
    BuildGeneralCard(inspectorLayout);

    cardTransform_ = new TransformCard(this);
    cardTransform_->SetOnChanged([this] {
        if (refreshing_ || selectedId_ == 0) return;
        emit TransformEditingFinished(selectedId_, CurrentTransformValue());
    });
    inspectorLayout->addWidget(cardTransform_);

    cardGeometry_ = new GeometryCard(this);
    cardGeometry_->onChooseModel = [this] {
        emit ChooseModelRequested();
        QVariantMap params;
        params[QStringLiteral("objectId")] = static_cast<qulonglong>(selectedId_);
        CommandRegistry::Instance().Execute(QStringLiteral("Scene.ReplaceModel"), params);
    };

    cardMaterial_ = new MaterialCard(this);
    cardMaterial_->onEditMaterial = [this](const QString& path) { emit EditMaterialRequested(path); };
    cardMaterial_->onChooseMaterial = [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("Scene.AssignMaterial"));
    };
    cardMaterial_->onClearMaterial = [this] { emit ClearMaterialRequested(); };

    cardScript_ = new ScriptCard(this);
    cardScript_->setObjectName(QStringLiteral("ScriptCard"));
    cardScript_->onOpenScript = [this](const QString& path) { emit OpenScriptRequested(path); };
    cardScript_->onChooseScript = [this] { emit ChooseScriptRequested(); };
    cardScript_->onClearScript = [this] { emit ClearScriptRequested(); };
    cardScript_->onVariablesChanged = [this](const QString& json) { emit ScriptVariablesChanged(json); };

    // Автомат ИИ — свойства stateMachine и machineMemory «ИИ-персонажа», правятся как остальные свойства (Undo).
    cardAi_ = new AiMachineCard(this);
    cardAi_->setObjectName(QStringLiteral("AiMachineCard"));
    cardAi_->onSetMachine = [this](const QString& machine) { SubmitProperty(QStringLiteral("/stateMachine"), machine); };
    cardAi_->onMemoryChanged = [this](const QString& json) { SubmitProperty(QStringLiteral("/machineMemory"), json); };
    cardAi_->onCreate = [this] { emit CreateAiMachineRequested(); };
    cardAi_->onOpen = [this](const QString& path) { emit OpenAiMachineRequested(path); };
    cardAi_->onChoose = [this] { emit ChooseAiMachineRequested(); };
    cardAi_->fileExists = [this](const QString& path) {
        const QString root = property("contentRoot").toString();
        return !root.isEmpty() && path.startsWith(QStringLiteral("/Game/")) && QFileInfo::exists(QDir(root).filePath(path.mid(6)));
    };

    BuildPointLightCard(inspectorLayout);
    inspectorLayout->addWidget(cardGeometry_);
    inspectorLayout->addWidget(cardMaterial_);
    inspectorLayout->addWidget(cardScript_);
    inspectorLayout->addWidget(cardAi_);
    BuildCharacterCard(inspectorLayout);
    inspectorLayout->addWidget(properties_);

    cardGeneral_->hide();
    cardTransform_->hide();
    cardGeometry_->hide();
    cardMaterial_->hide();
    cardScript_->hide();
    cardAi_->hide();
    lightInspector_->hide();
    characterInspector_->hide();
    noSelectionLabel_->show();

    inspectorLayout->addStretch(1);
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    setMinimumHeight(0);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

void ObjectInspectorPanel::BuildGeneralCard(QVBoxLayout* layout)
{
    cardGeneral_ = new CollapsibleCard(Tr("Общие", "General"), this, true);
    auto* cl = cardGeneral_->ContentLayout();

    auto* rowHeader = new QWidget(cardGeneral_->Content());
    auto* layoutHeader = new QHBoxLayout(rowHeader);
    layoutHeader->setContentsMargins(0, 0, 0, 0);
    layoutHeader->setSpacing(6);

    typeBadgeLabel_ = new QLabel(QStringLiteral("None"), rowHeader);
    typeBadgeLabel_->setObjectName(QStringLiteral("TypeBadgeLabel"));
    layoutHeader->addWidget(typeBadgeLabel_);

    selectionLabel_ = new QLabel(Tr("Объект не выбран", "No object selected"), rowHeader);
    selectionLabel_->setObjectName(QStringLiteral("SelectedObjectLabel"));
    selectionLabel_->setWordWrap(true);
    layoutHeader->addWidget(selectionLabel_, 1);
    cl->addWidget(rowHeader);

    auto* rowName = new QWidget(cardGeneral_->Content());
    auto* lName = new QHBoxLayout(rowName);
    lName->setContentsMargins(0, 0, 0, 0);
    lName->setSpacing(6);

    labelName_ = new QLabel(Tr("Имя", "Name"), rowName);
    labelName_->setObjectName(QStringLiteral("CardFieldLabel"));
    labelName_->setFixedWidth(40);
    lName->addWidget(labelName_);

    objectName_ = new QLineEdit(rowName);
    objectName_->setMaxLength(120);
    objectName_->setFixedHeight(22);
    connect(objectName_, &QLineEdit::editingFinished, this, [this] {
        if (refreshing_ || selectedId_ == 0) return;
        emit NameEditingFinished(selectedId_, objectName_->text().trimmed());
    });
    lName->addWidget(objectName_, 1);
    cl->addWidget(rowName);

    layout->addWidget(cardGeneral_);
}

void ObjectInspectorPanel::BuildCharacterCard(QVBoxLayout* layout)
{
    cardCharacter_ = new CollapsibleCard(Tr("Персонаж", "Character"), this, true);
    characterInspector_ = cardCharacter_;
    auto* cl = cardCharacter_->ContentLayout();

    auto* rowSpeed = new QWidget(cardCharacter_->Content());
    auto* lSpeed = new QHBoxLayout(rowSpeed);
    lSpeed->setContentsMargins(0, 0, 0, 0); lSpeed->setSpacing(6);
    characterSpeedLabel_ = new QLabel(Tr("Скорость, ед/с", "Speed, u/s"), rowSpeed);
    characterSpeedLabel_->setObjectName(QStringLiteral("CardFieldLabel"));
    lSpeed->addWidget(characterSpeedLabel_);

    characterSpeed_ = new NoWheelDoubleSpinBox(rowSpeed);
    characterSpeed_->setObjectName(QStringLiteral("CharacterSpeed"));
    characterSpeed_->setFixedHeight(22); characterSpeed_->setRange(0, 10000); characterSpeed_->setKeyboardTracking(false);
    connect(characterSpeed_, &QDoubleSpinBox::editingFinished, this, [this] {
        if (!refreshing_ && selectedId_) emit CharacterSpeedEditingFinished(selectedId_, characterSpeed_->value());
    });
    lSpeed->addWidget(characterSpeed_, 1);
    cl->addWidget(rowSpeed);

    layout->addWidget(cardCharacter_);
    cardCharacter_->hide();
}

void ObjectInspectorPanel::RetranslateUi()
{
    if (noSelectionLabel_) noSelectionLabel_->setText(Tr("Объект не выбран", "No object selected"));
    if (cardGeneral_) cardGeneral_->SetTitle(Tr("Общие", "General"));
    if (cardLight_) cardLight_->SetTitle(Tr("Свет", "Light"));
    if (cardCharacter_) cardCharacter_->SetTitle(Tr("Персонаж", "Character"));
    if (cardTransform_) cardTransform_->RetranslateUi();
    if (cardGeometry_) cardGeometry_->RetranslateUi();
    if (cardMaterial_) cardMaterial_->RetranslateUi();
    if (cardScript_) cardScript_->RetranslateUi();
    if (cardAi_) cardAi_->RetranslateUi();

    if (lightEnabled_) lightEnabled_->setText(Tr("Включён", "Enabled"));
    if (lightShadows_) lightShadows_->setText(Tr("Тени", "Shadows"));
    if (lightColor_) lightColor_->setText(Tr("Выбрать цвет…", "Choose Color…"));
    if (lightRowLabels_[3]) lightRowLabels_[3]->setText(Tr("Интенсивность", "Intensity"));
    if (lightRowLabels_[4]) lightRowLabels_[4]->setText(Tr("Радиус", "Radius"));
    if (characterSpeedLabel_) characterSpeedLabel_->setText(Tr("Скорость, ед/с", "Speed, u/s"));
    if (labelName_) labelName_->setText(Tr("Имя", "Name"));

    if (selectedId_ == 0 && selectionLabel_)
        selectionLabel_->setText(Tr("Объект не выбран", "No object selected"));

    propertiesKey_.clear();
    if (selectedId_ != 0 && !lastSnapshot_.objects.empty())
        Refresh(lastSnapshot_, selectedId_, true);
}

void ObjectInspectorPanel::InvalidateAssetCache()
{
    if (cardGeometry_) cardGeometry_->InvalidateCache();
    if (cardMaterial_) cardMaterial_->InvalidateCache();
    if (selectedId_ != 0 && !lastSnapshot_.objects.empty())
        Refresh(lastSnapshot_, selectedId_, true);
}

void ObjectInspectorPanel::ClearFocusIfInside()
{
    if (auto* focus = QApplication::focusWidget(); focus && isAncestorOf(focus)) focus->clearFocus();
}

void ObjectInspectorPanel::CommitPendingEdits()
{
    if (HasFocusedChild())
    {
        ClearFocusIfInside();
        return;
    }
    // An inactive window reports no focus widget on Windows, so clearing focus
    // cannot fire editingFinished. Commit the transform directly; the receiver
    // drops no-op changes.
    if (!refreshing_ && selectedId_ != 0 && cardTransform_ && cardTransform_->isVisible())
        emit TransformEditingFinished(selectedId_, CurrentTransformValue());
}

bool ObjectInspectorPanel::HasFocusedChild() const
{
    const auto* focus = QApplication::focusWidget(); return focus && isAncestorOf(focus);
}

InspectorTransformValue ObjectInspectorPanel::CurrentTransformValue() const
{
    InspectorTransformValue value;
    if (cardTransform_) cardTransform_->GetValues(value.position.data(), value.euler.data(), value.scale.data());
    return value;
}

void ObjectInspectorPanel::Refresh(
    const EditorQtHost::Snapshot& view,
    quint64 selectedId,
    bool selectionChanged
)
{
    lastSnapshot_ = view;
    refreshing_ = true;
    selectedId_ = selectedId;

    QWidget* focus = QApplication::focusWidget();
    const bool canReplaceWholeInspector = selectionChanged || !focus || !isAncestorOf(focus);

    auto canReplaceField = [&](QWidget* field) {
        return selectionChanged || !focus || (field != focus && !field->isAncestorOf(focus));
    };

    if (selectedId_ == 0)
    {
        properties_->hide();
        noSelectionLabel_->setText(Tr("Объект не выбран", "No object selected"));
        noSelectionLabel_->show();
        cardGeneral_->hide(); cardTransform_->hide(); cardGeometry_->hide();
        cardMaterial_->hide(); cardScript_->hide(); cardAi_->hide(); lightInspector_->hide(); characterInspector_->hide();
        lastCardsValid_ = false;
        refreshing_ = false;
        return;
    }

    noSelectionLabel_->hide();
    cardGeneral_->show();
    cardTransform_->show();
    if (selectionChanged) {
        cardGeometry_->Clear();
        cardMaterial_->Clear();
    }

    bool selectedLight = false;

    for (const auto& object : view.objects)
    {
        if (object.id != selectedId_)
            continue;

        RefreshProjectProperties(view, object);
        // selectedLight updated below after determining type
        typeBadgeLabel_->setText(QString::fromStdString(object.type));
        selectionLabel_->setText(QStringLiteral("%1 · ID %2").arg(QString::fromStdString(object.name)).arg(object.id));
        characterInspector_->setVisible(object.gameplayKind == 1 && object.propertySchema.find("/moveSpeed") == std::string::npos);
        if (object.gameplayKind == 1 && canReplaceField(characterSpeed_))
            characterSpeed_->setValue(QJsonDocument::fromJson(QByteArray::fromStdString(object.projectData)).object()["moveSpeed"].toDouble(5));

        selectedType_ = object.type;
        const std::string& type = object.type;
        const bool isModel = EditorCore::IsModelObjectType(type);
        const bool isPrimitive = EditorCore::IsPrimitiveObjectType(type);
        const bool isPointLight = object.hasPointLight || (type == "Light.Point") || (type == "Engine.PointLight");
        const bool isDirLight = (type == "Light.Directional");
        selectedLight = isPointLight || isDirLight;
        const bool isCharacter = (object.gameplayKind == 1 || type == "Gameplay.Character" || type == "Character");
        const bool isCamera = (type == "Engine.Camera" || type == "Gameplay.Camera" || type == "Camera");
        const bool isParticles = (type == "Particles" || type.find("Particle") != std::string::npos);

        if (isDirLight) {
            const auto data = QJsonDocument::fromJson(QByteArray::fromStdString(object.projectData)).object();
            const auto colorArr = data["color"].toArray(); float col[3] = {1.0f, 1.0f, 1.0f};
            if (colorArr.size() == 3) for (int i = 0; i < 3; ++i) col[i] = static_cast<float>(colorArr[i].toDouble(1.0));
            for (int i = 0; i < 3; ++i) if (canReplaceField(lightValues_[i])) lightValues_[i]->setValue(col[i]);
            UpdateLightColorButton(col);
            if (canReplaceField(lightValues_[3])) lightValues_[3]->setValue(data["intensity"].toDouble(1.0));
            lightShadows_->show(); lightShadows_->setChecked(data["shadows"].toBool(false));
            lightRowLabels_[4]->setText(Tr("Дальность теней", "Shadow Distance"));
            lightValues_[4]->setRange(100.0, 100000.0); lightValues_[4]->setSingleStep(10.0);
            if (canReplaceField(lightValues_[4])) lightValues_[4]->setValue(data["shadowDistance"].toDouble(2000.0));
            cardLight_->SetExpanded(true);
            cardLight_->show();

        } else if (isPointLight) {
            for (int i = 0; i < 5; ++i) {
                if (!canReplaceField(lightValues_[i])) continue;
                if (i < 3) lightValues_[i]->setValue(object.pointLight.color[i]);
                else if (i == 3) lightValues_[i]->setValue(object.pointLight.intensity);
                else lightValues_[i]->setValue(object.pointLight.radius);
            }
            lightEnabled_->setChecked(object.pointLight.enabled != 0);
            lightShadows_->hide();
            lightRowLabels_[4]->setText(Tr("Радиус", "Radius"));
            lightValues_[4]->setRange(0.01, 10000.0); lightValues_[4]->setSingleStep(0.5);
            UpdateLightColorButton(object.pointLight.color); cardLight_->show();
        } else { cardLight_->hide(); }

        const bool showGeometry = isModel || isPrimitive;
        const bool showMaterial = isModel || isPrimitive;
        const bool showProperties = (!isModel && !isPrimitive && !isPointLight && !isDirLight && !isCamera && !isParticles);

        const QString cr = property("contentRoot").toString();
        const QString modelPath = QString::fromStdString(object.modelAsset);
        const QString materialPath = QString::fromStdString(object.materialAsset);
        const bool materialLight = isPointLight || isDirLight;
        const QString primType = isPrimitive ? (type.rfind("Primitive.", 0) == 0 ? QString::fromStdString(type.substr(10)) : QString::fromStdString(type)) : QString();
        const bool cardsDirty = !lastCardsValid_ || selectionChanged
            || showGeometry != lastCardsShowGeometry_
            || showMaterial != lastCardsShowMaterial_
            || modelPath != lastCardsModel_
            || primType != lastCardsPrimitive_
            || materialPath != lastCardsMaterial_
            || materialLight != lastCardsMaterialLight_
            || cr != lastCardsRoot_;
        if (showGeometry) {
            cardGeometry_->UpdateChooseModelButton(isModel, isPrimitive);
        }
        if (cardsDirty) {
            cardGeometry_->setVisible(showGeometry);
            if (isModel) {
                cardGeometry_->SetModelData(modelPath, true, cr, selectionChanged);
            } else if (isPrimitive) {
                cardGeometry_->SetPrimitiveData(primType);
            }
            cardMaterial_->SetData(materialPath, materialLight, cr, selectionChanged);
            cardMaterial_->setVisible(showMaterial);
            lastCardsShowGeometry_ = showGeometry;
            lastCardsShowMaterial_ = showMaterial;
            lastCardsModel_ = modelPath;
            lastCardsPrimitive_ = primType;
            lastCardsMaterial_ = materialPath;
            lastCardsMaterialLight_ = materialLight;
            lastCardsRoot_ = cr;
            lastCardsValid_ = true;
        }
        if (properties_->isHidden() == showProperties)
            properties_->setVisible(showProperties);
        // Скрипт — у любого объекта сцены, если игровой модуль их поддерживает.
        cardScript_->SetData(QString::fromStdString(object.scriptAsset), true);
        cardScript_->SetVariables(QString::fromStdString(object.scriptAsset), QString::fromStdString(object.scriptVariables),
                                  true);
        cardScript_->setVisible(view.scripts);
        // Карточка автомата — у типов со свойствами stateMachine и machineMemory (AiCharacter и его наследники).
        const bool aiMachine = HasAiMachine(object.propertySchema);
        if (aiMachine)
        {
            const auto data = QJsonDocument::fromJson(QByteArray::fromStdString(object.projectData)).object();
            cardAi_->SetData(data.value(QStringLiteral("stateMachine")).toString(), data.value(QStringLiteral("machineMemory")).toString(), true);
        }
        cardAi_->setVisible(aiMachine);

        if (canReplaceWholeInspector)
        {
            objectName_->setText(QString::fromStdString(object.name));

            const auto& transform = object.transform;
            const QQuaternion rotation(
                transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);

            const auto euler = rotation.toEulerAngles();
            const double pos[3] = {transform.position[0], transform.position[1], transform.position[2]};
            const double rotDeg[3] = {euler.x(), euler.y(), euler.z()};
            const double scale[3] = {transform.scale[0], transform.scale[1], transform.scale[2]};

            cardTransform_->SetValues(pos, rotDeg, scale, canReplaceField);
        }

        break;
    }

    lightInspector_->setVisible(selectedLight);
    refreshing_ = false;
}

void ObjectInspectorPanel::SetScriptVariablesReader(std::function<QJsonArray(const QString&)> reader)
{
    if (cardAi_) cardAi_->readVariables = reader;
    if (cardScript_) cardScript_->readVariables = std::move(reader);
}

void ObjectInspectorPanel::SetAiMachineProblems(const QString& machine, const QStringList& lines)
{
    if (cardAi_) cardAi_->SetProblems(machine, lines);
}

void ObjectInspectorPanel::SetAiMachineLive(const QStringList& lines)
{
    if (cardAi_) cardAi_->SetLive(lines);
}

bool ObjectInspectorPanel::HasAiMachine(const std::string& propertySchema)
{
    return propertySchema.find("\"/stateMachine\"") != std::string::npos && propertySchema.find("\"/machineMemory\"") != std::string::npos;
}
