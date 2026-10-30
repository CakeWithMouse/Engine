#include "ObjectInspectorPanel.h"
#include "CollapsibleCard.h"
#include "TransformCard.h"
#include "MaterialCard.h"
#include "NoWheelDoubleSpinBox.h"
#include "EditorLocale.h"

#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
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
    spin->setStyleSheet(QStringLiteral(
        "QDoubleSpinBox { background-color: #1F232B; color: #F1F5F9; border: 1px solid #333842; "
        "border-left: none; border-top-right-radius: 4px; border-bottom-right-radius: 4px; "
        "font-size: 11px; padding: 1px 2px; }"
        "QDoubleSpinBox:focus { border: 1px solid #3B82F6; }"
    ));
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
}

ObjectInspectorPanel::~ObjectInspectorPanel()
{
    if (langSubId_)
    {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
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
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background-color: #12141A; }"));

    auto* content = new QWidget(scrollArea);
    content->setStyleSheet(QStringLiteral("background-color: #12141A;"));
    auto* inspectorLayout = new QVBoxLayout(content);
    inspectorLayout->setContentsMargins(6, 6, 6, 6);
    inspectorLayout->setSpacing(6);

    BuildGeneralCard(inspectorLayout);

    cardTransform_ = new TransformCard(this);
    cardTransform_->SetOnChanged([this] {
        if (refreshing_ || selectedId_ == 0) return;
        emit TransformEditingFinished(selectedId_, CurrentTransformValue());
    });
    inspectorLayout->addWidget(cardTransform_);

    cardMaterial_ = new MaterialCard(this);
    cardMaterial_->onEditMaterial = [this](const QString& path) { emit EditMaterialRequested(path); };
    cardMaterial_->onChooseMaterial = [this] { emit ChooseMaterialRequested(); };
    cardMaterial_->onClearMaterial = [this] { emit ClearMaterialRequested(); };
    cardMaterial_->onChooseModel = [this] { emit ChooseModelRequested(); };
    cardMaterial_->onChooseTexture = [this] { emit ChooseTextureRequested(); };
    cardMaterial_->onClearTexture = [this] { emit ClearTextureRequested(); };
    inspectorLayout->addWidget(cardMaterial_);

    BuildPointLightCard(inspectorLayout);
    BuildCharacterCard(inspectorLayout);

    inspectorLayout->addStretch(1);
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    setMinimumHeight(0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
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
    typeBadgeLabel_->setStyleSheet(QStringLiteral(
        "background-color: #2D333F; color: #94A3B8; font-weight: bold; font-size: 10px; "
        "padding: 2px 6px; border-radius: 4px; border: 1px solid #3E4554;"
    ));
    layoutHeader->addWidget(typeBadgeLabel_);

    selectionLabel_ = new QLabel(Tr("Объект не выбран", "No object selected"), rowHeader);
    selectionLabel_->setObjectName(QStringLiteral("SelectedObjectLabel"));
    selectionLabel_->setWordWrap(true);
    selectionLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #38BDF8; font-size: 11px;"));
    layoutHeader->addWidget(selectionLabel_, 1);
    cl->addWidget(rowHeader);

    auto* rowName = new QWidget(cardGeneral_->Content());
    auto* lName = new QHBoxLayout(rowName);
    lName->setContentsMargins(0, 0, 0, 0);
    lName->setSpacing(6);

    auto* lblName = new QLabel(Tr("Имя", "Name"), rowName);
    lblName->setFixedWidth(40);
    lblName->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    lName->addWidget(lblName);

    objectName_ = new QLineEdit(rowName);
    objectName_->setMaxLength(120);
    objectName_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; "
        "border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
        "QLineEdit:focus { border: 1px solid #3B82F6; }"
    ));
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
    cardLight_ = new CollapsibleCard(Tr("Точечный свет", "Point Light"), this, true);
    lightInspector_ = cardLight_;
    auto* cl = cardLight_->ContentLayout();

    auto emitPointLightChanged = [this] {
        if (refreshing_ || selectedId_ == 0) return;
        emit PointLightEditingFinished(selectedId_, CurrentPointLightValue());
    };

    auto* rowTop = new QWidget(cardLight_->Content());
    auto* lTop = new QHBoxLayout(rowTop);
    lTop->setContentsMargins(0, 0, 0, 0);
    lTop->setSpacing(6);

    lightEnabled_ = new QCheckBox(Tr("Включён", "Enabled"), rowTop);
    lightEnabled_->setObjectName(QStringLiteral("LightEnabled"));
    lightEnabled_->setStyleSheet(QStringLiteral("color: #E2E8F0; font-size: 11px; font-weight: bold;"));
    connect(lightEnabled_, &QCheckBox::clicked, this, emitPointLightChanged);
    lTop->addWidget(lightEnabled_);

    lightColor_ = new QPushButton(Tr("Выбрать цвет…", "Choose Color…"), rowTop);
    lightColor_->setObjectName(QStringLiteral("LightColor"));
    lightColor_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #333842; color: #FFFFFF; font-weight: bold; border: 1px solid #4B5263; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
    ));
    connect(lightColor_, &QPushButton::clicked, this, [this] {
        if (refreshing_ || selectedId_ == 0) return;
        const QColor current = QColor::fromRgbF(lightValues_[0]->value(), lightValues_[1]->value(), lightValues_[2]->value());
        emit LightColorRequested(selectedId_, current);
    });
    lTop->addWidget(lightColor_, 1);
    cl->addWidget(rowTop);

    auto* rowRgb = new QWidget(cardLight_->Content());
    auto* lRgb = new QHBoxLayout(rowRgb);
    lRgb->setContentsMargins(0, 0, 0, 0);
    lRgb->setSpacing(4);

    const QString rgbLetters[3] = {QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B")};
    const QString rgbColors[3] = {QStringLiteral("#EF4444"), QStringLiteral("#22C55E"), QStringLiteral("#3B82F6")};

    for (int i = 0; i < 3; ++i)
    {
        auto* spin = lightValues_[i] = new NoWheelDoubleSpinBox(rowRgb);
        spin->setObjectName(QStringLiteral("LightValue%1").arg(i));
        spin->setRange(0.0, 1.0);
        spin->setDecimals(2);
        spin->setSingleStep(0.05);
        spin->setKeyboardTracking(false);
        connect(spin, &QDoubleSpinBox::editingFinished, this, emitPointLightChanged);
        lRgb->addWidget(CreateAxisCell(spin, rgbLetters[i], rgbColors[i], rowRgb), 1);
    }
    cl->addWidget(rowRgb);

    auto* rowInt = new QWidget(cardLight_->Content());
    auto* lInt = new QHBoxLayout(rowInt);
    lInt->setContentsMargins(0, 0, 0, 0);
    lInt->setSpacing(6);

    lightRowLabels_[3] = new QLabel(Tr("Интенсивность", "Intensity"), rowInt);
    lightRowLabels_[3]->setFixedWidth(80);
    lightRowLabels_[3]->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    lInt->addWidget(lightRowLabels_[3]);

    lightValues_[3] = new NoWheelDoubleSpinBox(rowInt);
    lightValues_[3]->setRange(0.0, 10000.0);
    lightValues_[3]->setDecimals(2);
    lightValues_[3]->setSingleStep(0.5);
    lightValues_[3]->setKeyboardTracking(false);
    lightValues_[3]->setStyleSheet(QStringLiteral("QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; border-radius: 4px; font-size: 11px; }"));
    connect(lightValues_[3], &QDoubleSpinBox::editingFinished, this, emitPointLightChanged);
    lInt->addWidget(lightValues_[3], 1);
    cl->addWidget(rowInt);

    auto* rowRad = new QWidget(cardLight_->Content());
    auto* lRad = new QHBoxLayout(rowRad);
    lRad->setContentsMargins(0, 0, 0, 0);
    lRad->setSpacing(6);

    lightRowLabels_[4] = new QLabel(Tr("Радиус", "Radius"), rowRad);
    lightRowLabels_[4]->setFixedWidth(80);
    lightRowLabels_[4]->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    lRad->addWidget(lightRowLabels_[4]);

    lightValues_[4] = new NoWheelDoubleSpinBox(rowRad);
    lightValues_[4]->setRange(0.01, 10000.0);
    lightValues_[4]->setDecimals(2);
    lightValues_[4]->setSingleStep(0.5);
    lightValues_[4]->setKeyboardTracking(false);
    lightValues_[4]->setStyleSheet(QStringLiteral("QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; border-radius: 4px; font-size: 11px; }"));
    connect(lightValues_[4], &QDoubleSpinBox::editingFinished, this, emitPointLightChanged);
    lRad->addWidget(lightValues_[4], 1);
    cl->addWidget(rowRad);

    layout->addWidget(cardLight_);
    cardLight_->hide();
}

void ObjectInspectorPanel::BuildCharacterCard(QVBoxLayout* layout)
{
    cardCharacter_ = new CollapsibleCard(Tr("Персонаж", "Character"), this, true);
    characterInspector_ = cardCharacter_;
    auto* cl = cardCharacter_->ContentLayout();

    auto* rowSpeed = new QWidget(cardCharacter_->Content());
    auto* lSpeed = new QHBoxLayout(rowSpeed);
    lSpeed->setContentsMargins(0, 0, 0, 0);
    lSpeed->setSpacing(6);

    characterSpeedLabel_ = new QLabel(Tr("Скорость, ед/с", "Speed, u/s"), rowSpeed);
    characterSpeedLabel_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    lSpeed->addWidget(characterSpeedLabel_);

    characterSpeed_ = new NoWheelDoubleSpinBox(rowSpeed);
    characterSpeed_->setObjectName(QStringLiteral("CharacterSpeed"));
    characterSpeed_->setRange(0, 10000);
    characterSpeed_->setKeyboardTracking(false);
    characterSpeed_->setStyleSheet(QStringLiteral("QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; border-radius: 4px; font-size: 11px; }"));
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
    if (cardGeneral_) cardGeneral_->SetTitle(Tr("Общие", "General"));
    if (cardLight_) cardLight_->SetTitle(Tr("Точечный свет", "Point Light"));
    if (cardCharacter_) cardCharacter_->SetTitle(Tr("Персонаж", "Character"));
    if (cardTransform_) cardTransform_->RetranslateUi();
    if (cardMaterial_) cardMaterial_->RetranslateUi();

    if (lightEnabled_) lightEnabled_->setText(Tr("Включён", "Enabled"));
    if (lightColor_) lightColor_->setText(Tr("Выбрать цвет…", "Choose Color…"));
    if (lightRowLabels_[3]) lightRowLabels_[3]->setText(Tr("Интенсивность", "Intensity"));
    if (lightRowLabels_[4]) lightRowLabels_[4]->setText(Tr("Радиус", "Radius"));
    if (characterSpeedLabel_) characterSpeedLabel_->setText(Tr("Скорость, ед/с", "Speed, u/s"));

    if (selectedId_ == 0 && selectionLabel_)
        selectionLabel_->setText(Tr("Объект не выбран", "No object selected"));
}

void ObjectInspectorPanel::ClearFocusIfInside()
{
    if (auto* focus = QApplication::focusWidget(); focus && isAncestorOf(focus))
        focus->clearFocus();
}

bool ObjectInspectorPanel::HasFocusedChild() const
{
    const auto* focus = QApplication::focusWidget();
    return focus && isAncestorOf(focus);
}

InspectorTransformValue ObjectInspectorPanel::CurrentTransformValue() const
{
    InspectorTransformValue value;
    if (cardTransform_)
        cardTransform_->GetValues(value.position.data(), value.euler.data(), value.scale.data());
    return value;
}

InspectorPointLightValue ObjectInspectorPanel::CurrentPointLightValue() const
{
    InspectorPointLightValue value;
    for (int i = 0; i < 3; ++i)
        value.color[i] = lightValues_[i]->value();
    value.intensity = lightValues_[3]->value();
    value.radius = lightValues_[4]->value();
    value.enabled = lightEnabled_->isChecked();
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
    refreshing_ = true;
    selectedId_ = selectedId;

    QWidget* focus = QApplication::focusWidget();
    const bool canReplaceWholeInspector = selectionChanged || !focus || !isAncestorOf(focus);

    auto canReplaceField = [&](QWidget* field) {
        return selectionChanged || !focus || (field != focus && !field->isAncestorOf(focus));
    };

    if (selectedId_ == 0)
    {
        selectionLabel_->setText(Tr("Объект не выбран", "No object selected"));
        typeBadgeLabel_->setText(QStringLiteral("None"));
        objectName_->clear();
        cardMaterial_->Clear();
        lightInspector_->setVisible(false);
        characterInspector_->hide();
        refreshing_ = false;
        return;
    }

    cardMaterial_->Clear();

    bool selectedLight = false;

    for (const auto& object : view.objects)
    {
        if (object.id != selectedId_)
            continue;

        selectedLight = object.hasPointLight;
        typeBadgeLabel_->setText(QString::fromStdString(object.type));
        selectionLabel_->setText(QStringLiteral("%1 · ID %2").arg(QString::fromStdString(object.name)).arg(object.id));
        characterInspector_->setVisible(object.gameplayKind == 1);
        if (object.gameplayKind == 1 && canReplaceField(characterSpeed_))
            characterSpeed_->setValue(QJsonDocument::fromJson(QByteArray::fromStdString(object.projectData)).object()["moveSpeed"].toDouble(5));

        if (object.hasPointLight)
        {
            for (int i = 0; i < 5; ++i)
            {
                if (!canReplaceField(lightValues_[i]))
                    continue;

                if (i < 3)
                    lightValues_[i]->setValue(object.pointLight.color[i]);
                else if (i == 3)
                    lightValues_[i]->setValue(object.pointLight.intensity);
                else
                    lightValues_[i]->setValue(object.pointLight.radius);
            }

            lightEnabled_->setChecked(object.pointLight.enabled != 0);
            UpdateLightColorButton(object.pointLight.color);
        }

        const bool canChooseModel = (object.type == "Engine.Model" || object.type.rfind("Primitive.", 0) == 0);
        cardMaterial_->SetData(
            QString::fromStdString(object.materialAsset),
            QString::fromStdString(object.modelAsset),
            QString::fromStdString(object.textureAsset),
            canChooseModel,
            object.hasPointLight
        );

        if (canReplaceWholeInspector)
        {
            objectName_->setText(QString::fromStdString(object.name));

            const auto& transform = object.transform;
            const QQuaternion rotation(
                transform.rotation[3],
                transform.rotation[0],
                transform.rotation[1],
                transform.rotation[2]
            );

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
