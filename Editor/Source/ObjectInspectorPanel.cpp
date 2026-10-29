#include "ObjectInspectorPanel.h"
#include "QtHost.h"
#include "EditorLocale.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QPushButton>
#include <QApplication>
#include <QQuaternion>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFrame>
#include <QSizePolicy>

ObjectInspectorPanel::ObjectInspectorPanel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(80, 60);
    BuildUi();
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });
}

ObjectInspectorPanel::~ObjectInspectorPanel()
{
    if (langSubId_) {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
}

void ObjectInspectorPanel::SetMaterialsAvailable(bool available)
{
    materialActions_->setEnabled(available);
    materialAsset_->setEnabled(available);
    materialActions_->setToolTip(available ? QString() : Tr("Материалы требуют обновления игрового модуля.",
                                                           "Materials require updating the game module."));
}

void ObjectInspectorPanel::RetranslateUi()
{
    if (generalBox_) generalBox_->setTitle(Tr("Общие", "General"));
    if (posBox_) posBox_->setTitle(Tr("Позиция", "Position"));
    if (rotBox_) rotBox_->setTitle(Tr("Поворот", "Rotation"));
    if (scaleBox_) scaleBox_->setTitle(Tr("Масштаб", "Scale"));
    if (visualBox_) visualBox_->setTitle(Tr("Визуал", "Visuals"));
    if (nameRowLabel_) nameRowLabel_->setText(Tr("Имя", "Name"));

    if (selectedId_ == 0 && selectionLabel_)
    {
        selectionLabel_->setText(Tr("Объект не выбран", "No object selected"));
    }

    if (auto* lightBox = qobject_cast<QGroupBox*>(lightInspector_))
        lightBox->setTitle(Tr("Точечный свет", "Point Light"));
    if (lightColor_) lightColor_->setText(Tr("Выбрать цвет…", "Choose Color…"));
    if (lightEnabled_) lightEnabled_->setText(Tr("Включён", "Enabled"));

    const QString lightLabelsEn[] = { "Red", "Green", "Blue", "Intensity", "Radius" };
    const QString lightLabelsRu[] = { "Красный", "Зелёный", "Синий", "Интенсивность", "Радиус" };
    for (int i = 0; i < 5; ++i)
    {
        if (lightRowLabels_[i])
            lightRowLabels_[i]->setText(Tr(lightLabelsRu[i].toUtf8().constData(), lightLabelsEn[i].toUtf8().constData()));
    }

    if (auto* charBox = qobject_cast<QGroupBox*>(characterInspector_))
        charBox->setTitle(Tr("Персонаж", "Character"));
    if (characterSpeedLabel_) characterSpeedLabel_->setText(Tr("Скорость, ед/с", "Speed, units/s"));

    if (modelRowLabel_) modelRowLabel_->setText(Tr("Модель", "Model"));
    if (chooseModel_) {
        chooseModel_->setText(Tr("Выбрать модель…", "Choose Model…"));
        chooseModel_->setToolTip(Tr("Заменить модель или превратить примитив в модель. Для C++ персонажей визуал задаётся их классом.",
                                    "Replace model or convert primitive to model. For C++ characters visuals are set by their class."));
    }
    if (textureRowLabel_) textureRowLabel_->setText(Tr("Текстура", "Texture"));
    if (chooseTextureButton_) chooseTextureButton_->setText(Tr("Выбрать текстуру…", "Choose Texture…"));
    if (clearTextureButton_) clearTextureButton_->setText(Tr("Убрать", "Clear"));

    if (materialRowLabel_) materialRowLabel_->setText(Tr("Материал", "Material"));
    if (materialAsset_) materialAsset_->setPlaceholderText(Tr("по умолчанию", "default"));
    if (chooseMaterialButton_) chooseMaterialButton_->setText(Tr("Выбрать…", "Choose…"));
    if (editMaterialButton_) editMaterialButton_->setText(Tr("Редактировать", "Edit"));
    if (clearMaterialButton_) clearMaterialButton_->setText(Tr("Убрать", "Clear"));
}

void ObjectInspectorPanel::BuildUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setMinimumSize(60, 50);
    scrollArea->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setMinimumHeight(0);

    auto* content = new QWidget(scrollArea);

    auto* inspectorLayout = new QVBoxLayout(content);
    inspectorLayout->setContentsMargins(6, 6, 6, 6);
    auto group = [content, inspectorLayout](const QString& title, QGroupBox** outBox = nullptr) {
        auto* box = new QGroupBox(title, content);
        if (outBox) *outBox = box;
        inspectorLayout->addWidget(box);
        auto* form = new QFormLayout(box);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        return form;
    };
    auto* form = group(Tr("Общие", "General"), &generalBox_);
    selectionLabel_ = new QLabel(Tr("Объект не выбран", "No object selected"), content);
    selectionLabel_->setObjectName(QStringLiteral("SelectedObjectLabel"));
    selectionLabel_->setWordWrap(true);
    selectionLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #efa52b;"));
    form->addRow(selectionLabel_);

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    // Чтобы сама панель могла нормально сжиматься внутри дока
    setMinimumHeight(0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    objectName_ = new QLineEdit(this);
    objectName_->setMaxLength(120);
    nameRowLabel_ = new QLabel(Tr("Имя", "Name"), content);
    form->addRow(nameRowLabel_, objectName_);

    const QString labels[] =
    {
        QStringLiteral("X"),
        QStringLiteral("Y"),
        QStringLiteral("Z"),
        QStringLiteral("X, °"),
        QStringLiteral("Y, °"),
        QStringLiteral("Z, °"),
        QStringLiteral("X"),
        QStringLiteral("Y"),
        QStringLiteral("Z")
    };

    for (int i = 0; i < 9; ++i)
    {
        if (i % 3 == 0) {
            if (i == 0) form = group(Tr("Позиция", "Position"), &posBox_);
            else if (i == 3) form = group(Tr("Поворот", "Rotation"), &rotBox_);
            else form = group(Tr("Масштаб", "Scale"), &scaleBox_);
        }
        auto* spin = transform_[i] = new QDoubleSpinBox(this);
        spin->setObjectName(QStringLiteral("Transform%1").arg(i));
        spin->setRange(i >= 6 ? 0.001 : -10000.0, 10000.0);
        spin->setDecimals(3);
        spin->setSingleStep(i >= 3 && i < 6 ? 15.0 : 0.25);
        spin->setKeyboardTracking(false);
        form->addRow(labels[i], spin);

        connect(spin, &QDoubleSpinBox::editingFinished, this, [this]
        {
            if (refreshing_ || selectedId_ == 0)
                return;

            emit TransformEditingFinished(selectedId_, CurrentTransformValue());
        });
    }

    form = group(Tr("Визуал", "Visuals"), &visualBox_);
    BuildAssetInspector(form);

    auto* lightGroup = new QGroupBox(Tr("Точечный свет", "Point Light"), this);
    lightInspector_ = lightGroup;

    auto* lightForm = new QFormLayout(lightGroup);

    lightColor_ = new QPushButton(Tr("Выбрать цвет…", "Choose Color…"), lightGroup);
    lightColor_->setObjectName(QStringLiteral("LightColor"));
    lightForm->addRow(lightColor_);

    const QString lightLabelsEn[] = { "Red", "Green", "Blue", "Intensity", "Radius" };
    const QString lightLabelsRu[] = { "Красный", "Зелёный", "Синий", "Интенсивность", "Радиус" };

    auto emitPointLightChanged = [this]
    {
        if (refreshing_ || selectedId_ == 0)
            return;

        emit PointLightEditingFinished(selectedId_, CurrentPointLightValue());
    };

    for (int i = 0; i < 5; ++i)
    {
        auto* spin = lightValues_[i] = new QDoubleSpinBox(lightGroup);
        spin->setObjectName(QStringLiteral("LightValue%1").arg(i));
        spin->setRange(i == 4 ? 0.01 : 0.0, i < 3 ? 1.0 : 10000.0);
        spin->setDecimals(3);
        spin->setSingleStep(i < 3 ? 0.05 : 0.5);
        spin->setKeyboardTracking(false);
        lightRowLabels_[i] = new QLabel(Tr(lightLabelsRu[i].toUtf8().constData(), lightLabelsEn[i].toUtf8().constData()), lightGroup);
        lightForm->addRow(lightRowLabels_[i], spin);

        connect(spin, &QDoubleSpinBox::editingFinished, this, emitPointLightChanged);
    }

    lightEnabled_ = new QCheckBox(Tr("Включён", "Enabled"), lightGroup);
    lightEnabled_->setObjectName(QStringLiteral("LightEnabled"));
    lightForm->addRow(lightEnabled_);

    connect(lightEnabled_, &QCheckBox::clicked, this, [this, emitPointLightChanged](bool)
    {
        emitPointLightChanged();
    });

    connect(lightColor_, &QPushButton::clicked, this, [this]
    {
        if (refreshing_ || selectedId_ == 0)
            return;

        const QColor current = QColor::fromRgbF(
            lightValues_[0]->value(),
            lightValues_[1]->value(),
            lightValues_[2]->value()
        );

        emit LightColorRequested(selectedId_, current);
    });

    inspectorLayout->addWidget(lightGroup);
    lightGroup->hide();

    auto* characterGroup = new QGroupBox(Tr("Персонаж", "Character"), content);
    characterInspector_ = characterGroup;
    auto* characterForm = new QFormLayout(characterGroup);
    characterSpeed_ = new QDoubleSpinBox(characterGroup);
    characterSpeed_->setObjectName("CharacterSpeed");
    characterSpeed_->setRange(0, 10000);
    characterSpeed_->setKeyboardTracking(false);
    characterSpeedLabel_ = new QLabel(Tr("Скорость, ед/с", "Speed, units/s"), characterGroup);
    characterForm->addRow(characterSpeedLabel_, characterSpeed_);
    inspectorLayout->addWidget(characterGroup);
    characterGroup->hide();
    connect(characterSpeed_, &QDoubleSpinBox::editingFinished, this, [this] {
        if (!refreshing_ && selectedId_) emit CharacterSpeedEditingFinished(selectedId_, characterSpeed_->value());
    });
    inspectorLayout->addStretch();

    connect(objectName_, &QLineEdit::editingFinished, this, [this]
    {
        if (refreshing_ || selectedId_ == 0)
            return;

        emit NameEditingFinished(selectedId_, objectName_->text().trimmed());
    });
}

void ObjectInspectorPanel::BuildAssetInspector(QFormLayout* form)
{
    auto* content = form->parentWidget();

    modelAsset_ = new QLineEdit(content);
    modelAsset_->setReadOnly(true);
    modelAsset_->setObjectName(QStringLiteral("ModelAsset"));
    modelAsset_->setMinimumWidth(0);
    modelAsset_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    modelRowLabel_ = new QLabel(Tr("Модель", "Model"), content);
    form->addRow(modelRowLabel_, modelAsset_);

    chooseModel_ = new QPushButton(Tr("Выбрать модель…", "Choose Model…"), content);
    chooseModel_->setObjectName("ChooseModel");
    chooseModel_->setToolTip(Tr("Заменить модель или превратить примитив в модель. Для C++ персонажей визуал задаётся их классом.",
                                "Replace model or convert primitive to model. For C++ characters visuals are set by their class."));
    form->addRow(chooseModel_);
    connect(chooseModel_, &QPushButton::clicked, this, &ObjectInspectorPanel::ChooseModelRequested);

    textureAsset_ = new QLineEdit(content);
    textureAsset_->setReadOnly(true);
    textureAsset_->setObjectName(QStringLiteral("TextureAsset"));
    textureAsset_->setMinimumWidth(0);
    textureAsset_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    textureRowLabel_ = new QLabel(Tr("Текстура", "Texture"), content);
    form->addRow(textureRowLabel_, textureAsset_);

    auto* rowWidget = new QWidget(content);
    auto* row = new QHBoxLayout(rowWidget);
    row->setContentsMargins(0, 0, 0, 0);

    chooseTextureButton_ = new QPushButton(Tr("Выбрать текстуру…", "Choose Texture…"), rowWidget);
    chooseTextureButton_->setObjectName(QStringLiteral("ChooseTexture"));

    clearTextureButton_ = new QPushButton(Tr("Убрать", "Clear"), rowWidget);
    clearTextureButton_->setObjectName(QStringLiteral("ClearTexture"));

    row->addWidget(chooseTextureButton_);
    row->addWidget(clearTextureButton_);

    form->addRow(rowWidget);

    connect(chooseTextureButton_, &QPushButton::clicked, this, [this]
    {
        emit ChooseTextureRequested();
    });

    connect(clearTextureButton_, &QPushButton::clicked, this, [this]
    {
        emit ClearTextureRequested();
    });

    materialAsset_ = new QLineEdit(content);
    materialAsset_->setReadOnly(true);
    materialAsset_->setObjectName(QStringLiteral("MaterialAsset"));
    materialAsset_->setMinimumWidth(0);
    materialAsset_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    materialAsset_->setPlaceholderText(Tr("по умолчанию", "default"));
    materialRowLabel_ = new QLabel(Tr("Материал", "Material"), content);
    form->addRow(materialRowLabel_, materialAsset_);

    auto* materialRowWidget = new QWidget(content);
    materialActions_ = materialRowWidget;
    auto* materialRow = new QHBoxLayout(materialRowWidget);
    materialRow->setContentsMargins(0, 0, 0, 0);

    chooseMaterialButton_ = new QPushButton(Tr("Выбрать…", "Choose…"), materialRowWidget);
    chooseMaterialButton_->setObjectName(QStringLiteral("ChooseMaterial"));
    editMaterialButton_ = new QPushButton(Tr("Редактировать", "Edit"), materialRowWidget);
    editMaterialButton_->setObjectName(QStringLiteral("EditMaterial"));
    clearMaterialButton_ = new QPushButton(Tr("Убрать", "Clear"), materialRowWidget);
    clearMaterialButton_->setObjectName(QStringLiteral("ClearMaterial"));

    materialRow->addWidget(chooseMaterialButton_);
    materialRow->addWidget(editMaterialButton_);
    materialRow->addWidget(clearMaterialButton_);
    form->addRow(materialRowWidget);

    connect(chooseMaterialButton_, &QPushButton::clicked, this, [this]
    {
        emit ChooseMaterialRequested();
    });
    connect(editMaterialButton_, &QPushButton::clicked, this, [this]
    {
        emit EditMaterialRequested(materialAsset_->text());
    });
    connect(clearMaterialButton_, &QPushButton::clicked, this, [this]
    {
        emit ClearMaterialRequested();
    });
}

void ObjectInspectorPanel::SetEnabled(bool enabled)
{
    setEnabled(enabled);
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

    for (int i = 0; i < 3; ++i)
    {
        value.position[i] = transform_[i]->value();
        value.euler[i] = transform_[i + 3]->value();
        value.scale[i] = transform_[i + 6]->value();
    }

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
    const QColor c = QColor::fromRgbF(color[0], color[1], color[2]);
    lightColor_->setStyleSheet(QStringLiteral(
        "background-color: %1; color: %2; font-weight: bold;"
    ).arg(c.name(), c.lightnessF() > 0.5 ? QStringLiteral("black") : QStringLiteral("white")));
}

void ObjectInspectorPanel::Refresh(
    const EditorQtHost::Snapshot& view,
    quint64 selectedId,
    bool selectionChanged
)
{
    refreshing_ = true;

    selectedId_ = selectedId;

    if (selectedId_ == 0)
    {
        selectionLabel_->setText(Tr("Объект не выбран", "No object selected"));
        objectName_->clear();

        for (auto* spin : transform_)
            spin->setValue(0.0);

        modelAsset_->clear();
        textureAsset_->clear();
        materialAsset_->clear();
        editMaterialButton_->setEnabled(false);

        lightInspector_->hide();
        characterInspector_->hide();

        refreshing_ = false;
        return;
    }

    const auto it = std::find_if(
        view.objects.begin(),
        view.objects.end(),
        [selectedId](const auto& o) { return o.id == selectedId; }
    );

    if (it == view.objects.end())
    {
        selectionLabel_->setText(Tr("Выбран несуществующий объект", "Selected object does not exist"));
        refreshing_ = false;
        return;
    }

    const auto& object = *it;

    selectionLabel_->setText(
        QStringLiteral("%1 [%2]")
            .arg(QString::fromStdString(object.name))
            .arg(QString::fromStdString(object.type))
    );

    if (selectionChanged || !objectName_->hasFocus())
        objectName_->setText(QString::fromStdString(object.name));

    const auto& t = object.transform;

    const QQuaternion q(
        t.rotation[3],
        t.rotation[0],
        t.rotation[1],
        t.rotation[2]
    );

    const auto euler = q.toEulerAngles();

    const double values[] =
    {
        t.position[0],
        t.position[1],
        t.position[2],
        euler.x(),
        euler.y(),
        euler.z(),
        t.scale[0],
        t.scale[1],
        t.scale[2]
    };

    for (int i = 0; i < 9; ++i)
    {
        const bool focused = transform_[i]->hasFocus() || transform_[i]->isAncestorOf(QApplication::focusWidget());
        if (selectionChanged || !focused)
            transform_[i]->setValue(values[i]);
    }

    const QString model = QString::fromStdString(object.modelAsset);
    const QString texture = QString::fromStdString(object.textureAsset);
    const QString material = QString::fromStdString(object.materialAsset);

    if (selectionChanged || !modelAsset_->hasFocus())
        modelAsset_->setText(model);

    if (selectionChanged || !textureAsset_->hasFocus())
        textureAsset_->setText(texture);

    if (selectionChanged || !materialAsset_->hasFocus())
        materialAsset_->setText(material);
    editMaterialButton_->setEnabled(!object.materialAsset.empty() || !object.hasPointLight);
    editMaterialButton_->setToolTip(object.materialAsset.empty()
        ? Tr("Создать материал в Content/Materials, назначить его объекту и открыть редактор",
             "Create material in Content/Materials, assign to object, and open editor")
        : QString());

    const bool supportsModel = object.type == "Engine.Model" || QString::fromStdString(object.type).startsWith("Primitive.");
    chooseModel_->setEnabled(supportsModel);

    if (object.hasPointLight)
    {
        lightInspector_->show();

        const auto& light = object.pointLight;

        const double lv[] =
        {
            light.color[0],
            light.color[1],
            light.color[2],
            light.intensity,
            light.radius
        };

        for (int i = 0; i < 5; ++i)
        {
            const bool focused = lightValues_[i]->hasFocus() || lightValues_[i]->isAncestorOf(QApplication::focusWidget());
            if (selectionChanged || !focused)
                lightValues_[i]->setValue(lv[i]);
        }

        lightEnabled_->setChecked(light.enabled != 0);
        UpdateLightColorButton(light.color);
    }
    else
    {
        lightInspector_->hide();
    }

    if (object.gameplayKind == 1)
    {
        characterInspector_->show();
        double speed = 5.0;
        if (!object.projectData.empty()) {
            const auto json = QJsonDocument::fromJson(QByteArray::fromStdString(object.projectData)).object();
            if (json.contains("moveSpeed")) speed = json["moveSpeed"].toDouble();
            else if (json.contains("speed")) speed = json["speed"].toDouble();
        }
        const bool speedFocused = characterSpeed_->hasFocus() || characterSpeed_->isAncestorOf(QApplication::focusWidget());
        if (selectionChanged || !speedFocused) characterSpeed_->setValue(speed);
    }
    else
    {
        characterInspector_->hide();
    }

    refreshing_ = false;
}
