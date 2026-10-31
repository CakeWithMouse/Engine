#include "ObjectInspectorPanel.h"
#include "QtHost.h"
#include "PropertySection.h"
#include <QMap>

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
    BuildUi();
}

void ObjectInspectorPanel::SetMaterialsAvailable(bool available)
{
    materialActions_->setEnabled(available);
    materialAsset_->setEnabled(available);
    materialActions_->setToolTip(available ? QString() : QStringLiteral("Материалы требуют обновления игрового модуля."));
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
    scrollArea->setMinimumHeight(0);

    auto* content = new QWidget(scrollArea);

    auto* inspectorLayout = new QVBoxLayout(content);
    inspectorLayout->setContentsMargins(6, 6, 6, 6);
    QMap<QString,PropertySection*> groups;
    auto group = [content, inspectorLayout,&groups](const QString& title) {
        auto* layout=inspectorLayout;QString path;
        const auto parts=title.split('/');
        for(int i=0;i<parts.size();++i) {
            path+=(path.isEmpty()?QString():QString("/"))+parts[i];
            if(!groups.contains(path)) {auto* section=new PropertySection(parts[i],path,content);groups[path]=section;layout->addWidget(section);
                if(i+1<parts.size())new QVBoxLayout(section->body);}
            if(i+1<parts.size())layout=qobject_cast<QVBoxLayout*>(groups[path]->body->layout());
        }
        auto* form = new QFormLayout(groups[title]->body);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        return form;
    };
    auto* form = group(QStringLiteral("Общие"));
    selectionLabel_ = new QLabel(QStringLiteral("Объект не выбран"), content);
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
    form->addRow(QStringLiteral("Имя"), objectName_);

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
        if (i % 3 == 0) form = group(i == 0 ? QStringLiteral("Трансформация/Позиция") : i == 3 ? QStringLiteral("Трансформация/Поворот") : QStringLiteral("Трансформация/Масштаб"));
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

    form = group(QStringLiteral("Визуал"));
    visualInspector_ = groups[QStringLiteral("Визуал")];
    BuildAssetInspector(form);

    auto* lightGroup = new PropertySection(QStringLiteral("Точечный свет"),"Light",this);
    lightInspector_ = lightGroup;

    auto* lightForm = new QFormLayout(lightGroup->body);

    lightColor_ = new QPushButton(QStringLiteral("Выбрать цвет…"), lightGroup);
    lightColor_->setObjectName(QStringLiteral("LightColor"));
    lightForm->addRow(lightColor_);

    const QString lightLabels[] =
    {
        QStringLiteral("Красный"),
        QStringLiteral("Зелёный"),
        QStringLiteral("Синий"),
        QStringLiteral("Интенсивность"),
        QStringLiteral("Радиус")
    };

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
        lightForm->addRow(lightLabels[i], spin);

        connect(spin, &QDoubleSpinBox::editingFinished, this, emitPointLightChanged);
    }

    lightEnabled_ = new QCheckBox(QStringLiteral("Включён"), lightGroup);
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

    auto* characterGroup = new PropertySection(QStringLiteral("Персонаж"),"Character",content);
    characterInspector_ = characterGroup;
    auto* characterForm = new QFormLayout(characterGroup->body);
    characterSpeed_ = new QDoubleSpinBox(characterGroup);
    characterSpeed_->setObjectName("CharacterSpeed");
    characterSpeed_->setRange(0, 10000);
    characterSpeed_->setKeyboardTracking(false);
    characterForm->addRow(QStringLiteral("Скорость, ед/с"), characterSpeed_);
    inspectorLayout->addWidget(characterGroup);
    characterGroup->hide();
    connect(characterSpeed_, &QDoubleSpinBox::editingFinished, this, [this] {
        if (!refreshing_ && selectedId_) emit CharacterSpeedEditingFinished(selectedId_, characterSpeed_->value());
    });
    properties_=new QWidget(content);propertiesLayout_=new QVBoxLayout(properties_);propertiesLayout_->setContentsMargins(0,0,0,0);
    inspectorLayout->addWidget(properties_);
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
    form->addRow(QStringLiteral("Модель"), modelAsset_);

    chooseModel_ = new QPushButton(QStringLiteral("Выбрать модель…"), content);
    chooseModel_->setObjectName("ChooseModel");
    chooseModel_->setToolTip(QStringLiteral("Заменить модель или превратить примитив в модель. Для C++ персонажей визуал задаётся их классом."));
    form->addRow(chooseModel_);
    connect(chooseModel_, &QPushButton::clicked, this, &ObjectInspectorPanel::ChooseModelRequested);

    textureAsset_ = new QLineEdit(content);
    textureAsset_->setReadOnly(true);
    textureAsset_->setObjectName(QStringLiteral("TextureAsset"));
    textureAsset_->setMinimumWidth(0);
    textureAsset_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    form->addRow(QStringLiteral("Текстура"), textureAsset_);

    auto* rowWidget = new QWidget(content);
    auto* row = new QHBoxLayout(rowWidget);
    row->setContentsMargins(0, 0, 0, 0);

    auto* chooseTextureButton = new QPushButton(QStringLiteral("Выбрать текстуру…"), rowWidget);
    chooseTextureButton->setObjectName(QStringLiteral("ChooseTexture"));

    auto* clearTextureButton = new QPushButton(QStringLiteral("Убрать"), rowWidget);
    clearTextureButton->setObjectName(QStringLiteral("ClearTexture"));

    row->addWidget(chooseTextureButton);
    row->addWidget(clearTextureButton);

    form->addRow(rowWidget);

    connect(chooseTextureButton, &QPushButton::clicked, this, [this]
    {
        emit ChooseTextureRequested();
    });

    connect(clearTextureButton, &QPushButton::clicked, this, [this]
    {
        emit ClearTextureRequested();
    });

    materialAsset_ = new QLineEdit(content);
    materialAsset_->setReadOnly(true);
    materialAsset_->setObjectName(QStringLiteral("MaterialAsset"));
    materialAsset_->setMinimumWidth(0);
    materialAsset_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    materialAsset_->setPlaceholderText(QStringLiteral("по умолчанию"));
    form->addRow(QStringLiteral("Материал"), materialAsset_);

    auto* materialRowWidget = new QWidget(content);
    materialActions_ = materialRowWidget;
    auto* materialRow = new QHBoxLayout(materialRowWidget);
    materialRow->setContentsMargins(0, 0, 0, 0);

    auto* chooseMaterialButton = new QPushButton(QStringLiteral("Выбрать…"), materialRowWidget);
    chooseMaterialButton->setObjectName(QStringLiteral("ChooseMaterial"));
    editMaterialButton_ = new QPushButton(QStringLiteral("Редактировать"), materialRowWidget);
    editMaterialButton_->setObjectName(QStringLiteral("EditMaterial"));
    auto* clearMaterialButton = new QPushButton(QStringLiteral("Убрать"), materialRowWidget);
    clearMaterialButton->setObjectName(QStringLiteral("ClearMaterial"));

    materialRow->addWidget(chooseMaterialButton);
    materialRow->addWidget(editMaterialButton_);
    materialRow->addWidget(clearMaterialButton);
    form->addRow(materialRowWidget);

    connect(chooseMaterialButton, &QPushButton::clicked, this, [this]
    {
        emit ChooseMaterialRequested();
    });
    connect(editMaterialButton_, &QPushButton::clicked, this, [this]
    {
        emit EditMaterialRequested(materialAsset_->text());
    });
    connect(clearMaterialButton, &QPushButton::clicked, this, [this]
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
    const QColor backgroundColor = QColor::fromRgbF(color[0], color[1], color[2]);

    const double luminance =
        color[0] * 0.299 +
        color[1] * 0.587 +
        color[2] * 0.114;

    lightColor_->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                               .arg(backgroundColor.name())
                               .arg(luminance > 0.5 ? QStringLiteral("black") : QStringLiteral("white"))
    );
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

    const bool canReplaceWholeInspector =
        selectionChanged ||
        !focus ||
        !isAncestorOf(focus);

    auto canReplaceField = [&](QWidget* field)
    {
        return selectionChanged ||
            !focus ||
            (field != focus && !field->isAncestorOf(focus));
    };

    selectionLabel_->setText(QStringLiteral("Объект не выбран"));
    chooseModel_->setEnabled(false);

    if (selectedId_ == 0)
    {
        objectName_->clear();
        modelAsset_->clear();
        textureAsset_->clear();
        materialAsset_->clear();
        editMaterialButton_->setEnabled(false);
        lightInspector_->setVisible(false);
        characterInspector_->hide();
        properties_->hide();
        refreshing_ = false;
        return;
    }

    modelAsset_->clear();
    textureAsset_->clear();
    materialAsset_->clear();
    editMaterialButton_->setEnabled(false);

    bool selectedLight = false;

    for (const auto& object : view.objects)
    {
        if (object.id != selectedId_)
            continue;

        RefreshProjectProperties(view,object);
        visualInspector_->setVisible(object.type != "Light.Directional");
        selectedLight = object.hasPointLight;
        selectionLabel_->setText(QStringLiteral("Выбран: %1\n%2 · ID %3").arg(QString::fromStdString(object.name), QString::fromStdString(object.type)).arg(object.id));
        chooseModel_->setEnabled(object.type == "Engine.Model" || object.type.rfind("Primitive.", 0) == 0);
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

        modelAsset_->setText(QString::fromStdString(object.modelAsset));
        textureAsset_->setText(QString::fromStdString(object.textureAsset));
        materialAsset_->setText(QString::fromStdString(object.materialAsset));
        // С материалом по умолчанию кнопка создаёт новый материал; у света материала нет.
        editMaterialButton_->setEnabled(object.type != "Light.Directional" && (!object.materialAsset.empty() || !object.hasPointLight));
        if (object.type == "Light.Directional")
            selectionLabel_->setText(selectionLabel_->text() + QStringLiteral("\nОдин на всю сцену. Поворот задаёт направление.\nПозиция и масштаб не влияют на освещение."));
        editMaterialButton_->setToolTip(object.materialAsset.empty()
            ? QStringLiteral("Создать материал в Content/Materials, назначить его объекту и открыть редактор")
            : QString());

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

            for (int i = 0; i < 3; ++i)
            {
                transform_[i]->setValue(transform.position[i]);
                transform_[i + 6]->setValue(transform.scale[i]);
            }

            transform_[3]->setValue(euler.x());
            transform_[4]->setValue(euler.y());
            transform_[5]->setValue(euler.z());
        }

        break;
    }

    lightInspector_->setVisible(selectedLight);

    refreshing_ = false;
}
