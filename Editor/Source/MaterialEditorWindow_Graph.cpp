#include "MaterialEditorWindow.h"

#include "MaterialGraphPresets.h"
#include "MaterialGraphTransform.h"
#include "MaterialGraphWidget.h"
#include "EditorLocale.h"
#include "EditorTheme.h"
#include "IconProvider.h"
#include "MaterialEvents.h"

#include <QMenu>
#include <QMessageBox>
#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QStatusBar>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

// Шаблоны материала (Simple/ORM) и команды графа, вынесенные из MaterialEditorWindow.cpp:
// работают с graph_ через публичный API, не меняя размер самого окна.

void MaterialEditorWindow::BuildGraphTools(QToolBar* tools)
{
    templates_ = new QToolButton(this);
    templates_->setObjectName(QStringLiteral("MaterialTemplates"));
    templates_->setIcon(IconProvider::TemplateIcon());
    templates_->setIconSize(QSize(16, 16));
    templates_->setToolTip(Tr("Шаблоны материала: Simple, ORM, базовые эффекты",
                              "Material templates: Simple, ORM, base effects"));
    templates_->setPopupMode(QToolButton::InstantPopup);
    templates_->setEnabled(false); // до загрузки HLSL

    auto* menu = new QMenu(templates_);
    menu->setObjectName(QStringLiteral("MaterialTemplateMenu"));
    auto apply = [this](int kind) {
        if (QMessageBox::question(this, Tr("Шаблон материала", "Material template"),
                Tr("Заменить текущий материал графом шаблона? До сохранения это отменяется кнопкой «Откатить».",
                   "Replace the current material with the template graph? Revert undoes this before saving.")) == QMessageBox::Yes)
            ApplyTemplate(kind);
    };
    menu->addAction(Tr("Simple — слоты текстур", "Simple — texture slots"), this, [this, apply] { apply(0); });
    menu->addAction(Tr("ORM — упакованная текстура AO/Roughness/Metallic", "ORM — packed texture AO/Roughness/Metallic"),
                    this, [this, apply] { apply(1); });
    menu->addSeparator();
    menu->addAction(Tr("Textured Color — базовая текстура", "Textured Color — base texture"), this, [this, apply] { apply(2); });
    menu->addAction(Tr("Animated Neon — неоновая пульсация", "Animated Neon — pulsing neon"), this, [this, apply] { apply(3); });
    menu->addAction(Tr("Gold — золотой металл с бликом", "Gold — golden metal with rim"), this, [this, apply] { apply(4); });
    menu->addAction(Tr("Scrolling Energy — бегущая текстура", "Scrolling Energy — scrolling energy"), this, [this, apply] { apply(5); });
    templates_->setMenu(menu);
    tools->addWidget(templates_);
}

void MaterialEditorWindow::ApplyTemplate(int kind)
{
    if (!loaded_ || !sourceLoaded_ || saving_) return;
    BackupGraphFiles();
    if (kind == 0)
        graph_->SetModel(EditorCore::MaterialGraphPresets::BuildSimple());
    else if (kind == 1)
        graph_->SetModel(EditorCore::MaterialGraphPresets::BuildOrm());
    else
        graph_->LoadPreset(kind - 2);
    if (!graphMode_) SetGraphMode(true);
    ApplyGraph();
}

void MaterialEditorWindow::ExposeSelectedAsParameter()
{
    if (!loaded_ || !sourceLoaded_ || saving_) return;
    if (!graphMode_)
    {
        statusBar()->showMessage(Tr("Сначала перейдите в нодовый граф", "Switch to the node graph first"));
        return;
    }
    const QList<int> ids = graph_->SelectedNodes();
    if (!EditorCore::MaterialGraphTransform::CanExpose(graph_->Model(), ids))
    {
        statusBar()->showMessage(Tr("Выделите ноду Constant, чтобы выставить её параметром",
                                    "Select a Constant node to expose it as a parameter"));
        return;
    }
    BackupGraphFiles();
    graph_->SetModel(EditorCore::MaterialGraphTransform::ExposeConstants(graph_->Model(), ids));
    ApplyGraph();
    statusBar()->showMessage(Tr("Константа стала параметром: правится без перекомпиляции",
                                "Constant exposed as a parameter: editable without recompile"));
}

QColor MaterialEditorWindow::ValueToColor(const std::array<float, 4>& value, uint32_t components)
{
    auto channel = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    return QColor::fromRgbF(channel(value[0]), channel(value[1]), channel(value[2]),
                            components == 4 ? channel(value[3]) : 1.0f);
}

void MaterialEditorWindow::LoadPublicParams()
{
    publicParams_.clear();
    hasExplicitPublicParams_ = false;
    const QString path = PhysicalPath(material_);
    QFile file(path);
    if (file.open(QIODevice::ReadOnly))
    {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject() && doc.object().contains(QStringLiteral("publicParams")))
        {
            hasExplicitPublicParams_ = true;
            const QJsonArray arr = doc.object().value(QStringLiteral("publicParams")).toArray();
            for (const auto& val : arr)
                publicParams_.insert(val.toString());
        }
    }
    if (!hasExplicitPublicParams_)
    {
        for (const auto& p : state_.parameters)
            publicParams_.insert(p.name);
    }
}

void MaterialEditorWindow::SavePublicParams()
{
    const QString path = PhysicalPath(material_);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();
    QJsonArray arr;
    for (const QString& name : publicParams_)
        arr.append(name);
    obj[QStringLiteral("publicParams")] = arr;
    doc.setObject(obj);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(doc.toJson(QJsonDocument::Indented));
}

void MaterialEditorWindow::SetParamPublic(const QString& name, bool isPublic)
{
    hasExplicitPublicParams_ = true;
    if (isPublic) publicParams_.insert(name);
    else publicParams_.remove(name);
    // Флаг — метаданные редактора, не состояние шейдера: пишется сразу, чтобы параметр
    // появился в инспекторе объекта без перевыбора; «Откатить» его не возвращает.
    SavePublicParams();
    EditorCore::MaterialEvents::Instance().NotifyChanged(material_);
}

void MaterialEditorWindow::BuildParameterRow(QWidget* row, QHBoxLayout* layout, int index)
{
    const auto& parameter = state_.parameters[index];
    const bool isPublic = publicParams_.contains(parameter.name);
    auto* eyeBtn = new QPushButton(IconProvider::EyeIcon(isPublic), QString(), row);
    eyeBtn->setObjectName(QStringLiteral("MaterialParamEye_%1").arg(parameter.name));
    eyeBtn->setFixedSize(24, 22);
    eyeBtn->setCheckable(true);
    eyeBtn->setChecked(isPublic);
    eyeBtn->setToolTip(Tr("Показывать в инспекторе объекта", "Show in the object inspector"));
    connect(eyeBtn, &QPushButton::clicked, this, [this, eyeBtn, name = parameter.name](bool checked) {
        SetParamPublic(name, checked);
        eyeBtn->setIcon(IconProvider::EyeIcon(checked));
    });
    layout->addWidget(eyeBtn);

    if (!parameter.components)
    {
        layout->addWidget(new QLabel(Tr("тип не поддерживается редактором", "type not supported by editor"), row));
        return;
    }

    std::vector<QDoubleSpinBox*> spins;
    for (uint32_t i = 0; i < parameter.components; ++i)
    {
        auto* spin = new QDoubleSpinBox(row);
        spin->setObjectName(QStringLiteral("MaterialParam_%1_%2").arg(parameter.name).arg(i));
        spin->setRange(-100000.0, 100000.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.05);
        spin->setKeyboardTracking(false);
        spin->setValue(parameter.value[i]);
        spin->setMinimumWidth(64);
        layout->addWidget(spin, 1);
        spins.push_back(spin);
    }
    auto values = [spins] {
        std::array<float, 4> value{};
        for (size_t i = 0; i < spins.size(); ++i) value[i] = static_cast<float>(spins[i]->value());
        return value;
    };
    auto setValues = [spins](const std::array<float, 4>& value) {
        for (size_t i = 0; i < spins.size(); ++i)
        {
            const QSignalBlocker block(spins[i]);
            spins[i]->setValue(value[i]);
        }
    };

    QPushButton* color = nullptr;
    if (parameter.components >= 3)
    {
        color = new QPushButton(row);
        color->setObjectName(QStringLiteral("MaterialColor_%1").arg(parameter.name));
        color->setFixedSize(28, 22);
        color->setToolTip(Tr("Выбрать цвет", "Choose color"));
        layout->addWidget(color);
    }
    const uint32_t components = parameter.components;
    auto paint = [color, components](const std::array<float, 4>& value) {
        if (color) color->setStyleSheet(QStringLiteral("background-color: %1;").arg(ValueToColor(value, components).name()));
    };
    paint(parameter.value);

    for (auto* spin : spins)
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, index, values, paint]
        {
            paint(values());
            SetParameter(index, values());
        });
    if (color)
        connect(color, &QPushButton::clicked, this, [this, index, values, setValues, paint, components]
        {
            QColorDialog::ColorDialogOptions options;
            if (components == 4) options |= QColorDialog::ShowAlphaChannel;
            const QColor picked = QColorDialog::getColor(ValueToColor(values(), components), this,
                                                         state_.parameters[index].name, options);
            if (!picked.isValid()) return;
            auto value = values();
            value[0] = static_cast<float>(picked.redF());
            value[1] = static_cast<float>(picked.greenF());
            value[2] = static_cast<float>(picked.blueF());
            if (components == 4) value[3] = static_cast<float>(picked.alphaF());
            setValues(value);
            paint(value);
            SetParameter(index, value);
        });

    auto* reset = new QPushButton(EditorTheme::CreateResetIcon(), QString(), row);
    reset->setFixedWidth(28);
    reset->setToolTip(Tr("Значение из HLSL", "Value from HLSL"));
    layout->addWidget(reset);
    connect(reset, &QPushButton::clicked, this, [this, index, setValues, paint] {
        const auto val = state_.parameters[index].defaultValue;
        setValues(val); paint(val); SetParameter(index, val);
    });
}
