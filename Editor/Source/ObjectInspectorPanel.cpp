#include "ObjectInspectorPanel.h"
#include "CollapsibleCard.h"
#include "TransformCard.h"
#include "MaterialCard.h"
#include "GeometryCard.h"
#include "NoWheelDoubleSpinBox.h"
#include "EditorLocale.h"
#include "IconProvider.h"

#include <QApplication>
#include <QColorDialog>
#include <QCheckBox>
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

static QWidget* CreateAxisCell(NoWheelDoubleSpinBox* spin, const QString& axisLetter, const QString& badgeColor, QWidget* parent)
{
    auto* cell = new QWidget(parent);
    auto* layout = new QHBoxLayout(cell);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* badge = new QLabel(axisLetter, cell);
    badge->setFixedSize(16, 22);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QStringLiteral(
        "background-color: %1; color: white; font-weight: bold; font-size: 10px; "
        "border-top-left-radius: 4px; border-bottom-left-radius: 4px;"
    ).arg(badgeColor));
    layout->addWidget(badge);

    spin->setParent(cell);
    spin->setFixedHeight(22);
    spin->setProperty("axisCell", true);
    layout->addWidget(spin, 1);
    return cell;
}

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
    cardGeometry_->onChooseModel = [this] { emit ChooseModelRequested(); };
    cardGeometry_->onClearModel = [this] { emit ClearModelRequested(); };
    cardGeometry_->onChooseTexture = [this] { emit ChooseTextureRequested(); };
    cardGeometry_->onClearTexture = [this] { emit ClearTextureRequested(); };

    cardMaterial_ = new MaterialCard(this);
    cardMaterial_->onEditMaterial = [this](const QString& path) { emit EditMaterialRequested(path); };
    cardMaterial_->onChooseMaterial = [this] { emit ChooseMaterialRequested(); };
    cardMaterial_->onClearMaterial = [this] { emit ClearMaterialRequested(); };

    BuildPointLightCard(inspectorLayout);
    inspectorLayout->addWidget(cardGeometry_);
    inspectorLayout->addWidget(cardMaterial_);
    BuildCharacterCard(inspectorLayout);
    inspectorLayout->addWidget(properties_);

    cardGeneral_->hide();
    cardTransform_->hide();
    cardGeometry_->hide();
    cardMaterial_->hide();
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

void ObjectInspectorPanel::BuildPointLightCard(QVBoxLayout* layout)
{
    cardLight_ = new CollapsibleCard(Tr("Свет", "Light"), this, true);
    lightInspector_ = cardLight_;
    auto* cl = cardLight_->ContentLayout();
    auto emitChanged = [this] {
        if (refreshing_ || selectedId_ == 0) return;
        if (selectedType_ == "Light.Directional") {
            SubmitProperty(QStringLiteral("/intensity"), lightValues_[3]->value());
            SubmitProperty(QStringLiteral("/shadowDistance"), lightValues_[4]->value());
            return;
        }
        emit PointLightEditingFinished(selectedId_, CurrentPointLightValue());
    };
    auto* rowTop = new QWidget(cardLight_->Content()); auto* lTop = new QHBoxLayout(rowTop);
    lTop->setContentsMargins(0, 0, 0, 0); lTop->setSpacing(6);
    lightEnabled_ = new QCheckBox(Tr("Включён", "Enabled"), rowTop);
    lightEnabled_->setObjectName(QStringLiteral("LightEnabled")); lightEnabled_->hide();
    connect(lightEnabled_, &QCheckBox::clicked, this, emitChanged);
    lightShadows_ = new QCheckBox(Tr("Тени", "Shadows"), rowTop);
    lightShadows_->setObjectName(QStringLiteral("LightShadows"));
    connect(lightShadows_, &QCheckBox::clicked, this, [this, emitChanged] {
        if (refreshing_ || selectedId_ == 0) return;
        if (selectedType_ == "Light.Directional") { SubmitProperty(QStringLiteral("/shadows"), lightShadows_->isChecked()); return; }
        emitChanged();
    });
    lTop->addWidget(lightShadows_);
    lightColor_ = new QPushButton(Tr("Выбрать цвет…", "Choose Color…"), rowTop);
    lightColor_->setObjectName(QStringLiteral("LightColor")); lightColor_->setFixedHeight(24); lightColor_->setIcon(IconProvider::BulbIcon());
    connect(lightColor_, &QPushButton::clicked, this, [this] {
        if (refreshing_ || selectedId_ == 0) return;
        const QColor cur = QColor::fromRgbF(lightValues_[0]->value(), lightValues_[1]->value(), lightValues_[2]->value());
        if (selectedType_ == "Light.Directional") {
            const auto chosen = QColorDialog::getColor(cur, this, Tr("Цвет света", "Light Color"));
            if (chosen.isValid()) {
                const float c[3] = {static_cast<float>(chosen.redF()), static_cast<float>(chosen.greenF()), static_cast<float>(chosen.blueF())};
                UpdateLightColorButton(c); for (int i = 0; i < 3; ++i) lightValues_[i]->setValue(c[i]);
                SubmitProperty(QStringLiteral("/color"), QJsonArray{c[0], c[1], c[2]});
            } return;
        }
        emit LightColorRequested(selectedId_, cur);
    });
    lTop->addWidget(lightColor_, 1); cl->addWidget(rowTop);
    auto* rowRgb = new QWidget(cardLight_->Content()); auto* lRgb = new QHBoxLayout(rowRgb);
    lRgb->setContentsMargins(0, 0, 0, 0); lRgb->setSpacing(4);
    const QString rgbL[3] = {QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B")};
    const QString rgbC[3] = {QStringLiteral("#EF4444"), QStringLiteral("#22C55E"), QStringLiteral("#3B82F6")};
    for (int i = 0; i < 3; ++i) {
        auto* spin = lightValues_[i] = new NoWheelDoubleSpinBox(rowRgb);
        spin->setObjectName(QStringLiteral("LightValue%1").arg(i)); spin->setRange(0.0, 1.0); spin->setDecimals(2); spin->setSingleStep(0.05); spin->setKeyboardTracking(false);
        connect(spin, &QDoubleSpinBox::editingFinished, this, [this, emitChanged] {
            if (refreshing_ || selectedId_ == 0) return;
            if (selectedType_ == "Light.Directional") {
                const float c[3] = {static_cast<float>(lightValues_[0]->value()), static_cast<float>(lightValues_[1]->value()), static_cast<float>(lightValues_[2]->value())};
                UpdateLightColorButton(c); SubmitProperty(QStringLiteral("/color"), QJsonArray{c[0], c[1], c[2]}); return;
            }
            emitChanged();
        });
        lRgb->addWidget(CreateAxisCell(spin, rgbL[i], rgbC[i], rowRgb), 1);
    }
    cl->addWidget(rowRgb);
    auto addFieldRow = [&](int idx, const QString& label, double minV, double maxV, double step) {
        auto* row = new QWidget(cardLight_->Content()); auto* l = new QHBoxLayout(row);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(6);
        lightRowLabels_[idx] = new QLabel(label, row); lightRowLabels_[idx]->setObjectName(QStringLiteral("CardFieldLabel")); lightRowLabels_[idx]->setFixedWidth(80);
        l->addWidget(lightRowLabels_[idx]);
        auto* spin = lightValues_[idx] = new NoWheelDoubleSpinBox(row);
        spin->setObjectName(QStringLiteral("LightValue%1").arg(idx)); spin->setFixedHeight(22); spin->setRange(minV, maxV); spin->setDecimals(2); spin->setSingleStep(step); spin->setKeyboardTracking(false);
        connect(spin, &QDoubleSpinBox::editingFinished, this, emitChanged); l->addWidget(spin, 1); cl->addWidget(row);
    };
    addFieldRow(3, Tr("Интенсивность", "Intensity"), 0.0, 10000.0, 0.5);
    addFieldRow(4, Tr("Радиус", "Radius"), 0.01, 10000.0, 0.5);
    layout->addWidget(cardLight_); cardLight_->hide();
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

InspectorPointLightValue ObjectInspectorPanel::CurrentPointLightValue() const
{
    InspectorPointLightValue value;
    for (int i = 0; i < 3; ++i) value.color[i] = lightValues_[i]->value();
    value.intensity = lightValues_[3]->value();
    value.radius = lightValues_[4]->value();
    value.enabled = lightEnabled_->isChecked();
    value.castShadows = lightShadows_ ? lightShadows_->isChecked() : false;
    return value;
}

void ObjectInspectorPanel::UpdateLightColorButton(const float color[3])
{
    const QColor backgroundColor = QColor::fromRgbF(color[0], color[1], color[2]);
    const double luminance = color[0] * 0.299 + color[1] * 0.587 + color[2] * 0.114;
    lightColor_->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                               .arg(backgroundColor.name())
                               .arg(luminance > 0.5 ? QStringLiteral("black") : QStringLiteral("white")));
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
        cardMaterial_->hide(); lightInspector_->hide(); characterInspector_->hide();
        refreshing_ = false;
        return;
    }

    noSelectionLabel_->hide();
    cardGeneral_->show();
    cardTransform_->show();
    cardMaterial_->show();
    cardGeometry_->Clear();
    cardMaterial_->Clear();

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
        const bool isModel = (type == "Engine.Model");
        const bool isPrimitive = (type.rfind("Primitive.", 0) == 0);
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

        const bool showGeometry = isModel;
        const bool showMaterial = isModel || isPrimitive;
        const bool showProperties = (!isModel && !isPrimitive && !isPointLight && !isDirLight && !isCamera && !isParticles);

        const QString cr = property("contentRoot").toString();
        cardGeometry_->setVisible(showGeometry);
        if (showGeometry) {
            cardGeometry_->SetData(QString::fromStdString(object.modelAsset), true, cr, selectionChanged);
            cardGeometry_->SetTexture(QString::fromStdString(object.textureAsset), cr, selectionChanged);
        }
        cardMaterial_->SetData(QString::fromStdString(object.materialAsset), isPointLight || isDirLight, cr, selectionChanged);
        cardMaterial_->setVisible(showMaterial);
        properties_->setVisible(showProperties);

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
