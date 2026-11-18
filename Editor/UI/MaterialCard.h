#pragma once

#include "AssetPathDisplay.h"
#include "AssetSlotWidget.h"
#include "CollapsibleCard.h"
#include "EditorLocale.h"
#include "EditorTheme.h"
#include "InspectorCardStyle.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <functional>
#include <array>
#include <vector>
#include <algorithm>

class MaterialCard : public CollapsibleCard
{
public:
    explicit MaterialCard(QWidget* parent = nullptr)
        : CollapsibleCard(Tr("Материал", "Material"), parent, true)
    {
        BuildCard();
    }

    void SetMaterialsAvailable(bool available)
    {
        if (materialActions_)
        {
            materialActions_->setEnabled(available);
            materialActions_->setToolTip(available ? QString() : Tr("Материалы требуют обновления игрового модуля.", "Materials require updating the game module."));
        }
        if (materialAsset_)
            materialAsset_->setEnabled(available);
    }

    void SetData(const QString& material, bool hasPointLight, const QString& contentRoot = QString(), bool forceCheck = false)
    {
        rawMaterialPath_ = material;
        EditorUI::FormatAssetPath(materialAsset_, material, material);
        clearMaterialButton_->setEnabled(!material.isEmpty());

        const bool hasMat = !material.isEmpty();
        editMaterialButton_->setText(hasMat ? Tr("Редактировать", "Edit Material") : Tr("Создать", "Create"));
        editMaterialButton_->setObjectName(hasMat ? QStringLiteral("EditMaterial") : QStringLiteral("CreateMaterial"));

        if (!material.isEmpty() && !contentRoot.isEmpty())
        {
            const QString rel = material.startsWith(QStringLiteral("/Game/")) ? material.mid(6) : material;
            if (forceCheck || material != cachedMaterialPath_)
            {
                cachedMaterialPath_ = material;
                cachedMaterialExists_ = QFileInfo::exists(QDir(contentRoot).filePath(rel));
            }
            if (!cachedMaterialExists_)
            {
                materialAsset_->setStyleSheet(QStringLiteral("color: %1;").arg(EditorTheme::Instance().AccentColor().name()));
                materialAsset_->setToolTip(Tr("Файл материала не найден на диске", "Material file not found on disk"));
                editMaterialButton_->setEnabled(false);
                editMaterialButton_->setToolTip(Tr("Файл материала не найден на диске", "Material file not found on disk"));
                ClearParams();
            }
            else
            {
                materialAsset_->setStyleSheet(QString());
                editMaterialButton_->setEnabled(true);
                editMaterialButton_->setToolTip(QString());
                RebuildPublicParams(QDir(contentRoot).filePath(rel));
            }
        }
        else
        {
            cachedMaterialPath_.clear();
            cachedMaterialExists_ = true;
            materialAsset_->setStyleSheet(QString());
            editMaterialButton_->setEnabled(!hasPointLight);
            editMaterialButton_->setToolTip(material.isEmpty()
                ? Tr("Создать материал в Content/Materials, назначить его объекту и открыть редактор",
                     "Create material in Content/Materials, assign to object and open editor")
                : QString());
            ClearParams();
        }
    }

    void InvalidateCache()
    {
        cachedMaterialPath_.clear();
    }

    void Clear()
    {
        rawMaterialPath_.clear();
        materialAsset_->clear();
        materialAsset_->setToolTip(QString());
        editMaterialButton_->setText(Tr("Создать", "Create"));
        editMaterialButton_->setObjectName(QStringLiteral("CreateMaterial"));
        editMaterialButton_->setEnabled(true);
        clearMaterialButton_->setEnabled(false);
        cachedMaterialPath_.clear();
        cachedMaterialExists_ = true;
        materialAsset_->setStyleSheet(QString());
        ClearParams();
    }

    void RetranslateUi()
    {
        SetTitle(Tr("Материал", "Material"));
        SetInfo(Tr("Материал", "Material"),
                Tr("Материал объекта [.mat]:\n• Шейдер, слоты текстур и параметры\n• Публичные параметры редактируются ниже\n• Перетаскивание файла из браузера",
                   "Object material [.mat]:\n• Shader, texture slots, and parameters\n• Public parameters editable below\n• Drag-and-drop file from browser"));
        if (materialSlot_) materialSlot_->RetranslateUi();
        if (labelMatTitle_) labelMatTitle_->setText(Tr("Слот материала [.mat]", "Material Slot [.mat]"));
        if (labelParamsTitle_) labelParamsTitle_->setText(Tr("Параметры", "Parameters"));
        if (materialAsset_) materialAsset_->setPlaceholderText(Tr("по умолчанию", "default"));
        const bool hasMat = !rawMaterialPath_.isEmpty();
        if (editMaterialButton_)
        {
            editMaterialButton_->setText(hasMat ? Tr("Редактировать", "Edit Material") : Tr("Создать", "Create"));
        }
        if (chooseMaterialButton_) chooseMaterialButton_->setText(Tr("Выбрать…", "Browse…"));
        if (clearMaterialButton_) clearMaterialButton_->setText(Tr("Убрать", "Clear"));
    }

    // Callbacks
    std::function<void(const QString&)> onEditMaterial;
    std::function<void()> onChooseMaterial;
    std::function<void()> onClearMaterial;
    std::function<void(const QString&)> onMaterialDropped;
    std::function<void(const QString&, const QString&, const std::array<float, 4>&)> onParameterChanged;

private:
    void ClearParams()
    {
        if (!paramsRowsLayout_) return;
        QLayoutItem* item = nullptr;
        while ((item = paramsRowsLayout_->takeAt(0)) != nullptr)
        {
            if (QWidget* w = item->widget()) w->deleteLater();
            delete item;
        }
        if (labelParamsTitle_) labelParamsTitle_->hide();
    }

    void UpdateFileParam(const QString& path, const QString& key, const std::array<float, 4>& v, int compCount)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (!doc.isObject()) return;
        QJsonObject root = doc.object();
        QJsonObject params = root.value(QStringLiteral("params")).toObject();
        if (compCount == 1)
        {
            params[key] = v[0];
        }
        else
        {
            QJsonArray arr;
            for (int i = 0; i < compCount; ++i) arr.append(v[i]);
            params[key] = arr;
        }
        root[QStringLiteral("params")] = params;
        doc.setObject(root);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write(doc.toJson(QJsonDocument::Indented));
    }

    void BuildParamItem(const QString& key, const QJsonValue& val, const QString& path)
    {
        std::array<float, 4> initVals{};
        int compCount = 1;
        if (val.isDouble())
        {
            initVals[0] = static_cast<float>(val.toDouble());
        }
        else if (val.isArray())
        {
            const QJsonArray arr = val.toArray();
            compCount = std::min(static_cast<int>(arr.size()), 4);
            for (int i = 0; i < compCount; ++i) initVals[i] = static_cast<float>(arr[i].toDouble());
        }
        else return;

        auto* row = new QWidget(paramsContainer_);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        auto* nameLabel = new QLabel(key, row);
        nameLabel->setMinimumWidth(55);
        layout->addWidget(nameLabel);

        std::vector<QDoubleSpinBox*> spins;
        for (int i = 0; i < compCount; ++i)
        {
            auto* spin = new QDoubleSpinBox(row);
            spin->setObjectName(QStringLiteral("InspectorMatParam_%1_%2").arg(key).arg(i));
            spin->setRange(-100000.0, 100000.0);
            spin->setDecimals(3);
            spin->setSingleStep(0.05);
            spin->setKeyboardTracking(false);
            spin->setValue(initVals[i]);
            spin->setMinimumWidth(50);
            layout->addWidget(spin, 1);
            spins.push_back(spin);
        }

        QPushButton* colorBtn = nullptr;
        auto updateColor = [compCount](QPushButton* btn, const std::array<float, 4>& v) {
            auto ch = [](float x) { return std::clamp(x, 0.0f, 1.0f); };
            const QColor c = QColor::fromRgbF(ch(v[0]), ch(v[1]), ch(v[2]), compCount == 4 ? ch(v[3]) : 1.0f);
            btn->setStyleSheet(QStringLiteral("background-color: %1;").arg(c.name()));
        };
        if (compCount >= 3)
        {
            colorBtn = new QPushButton(row);
            colorBtn->setObjectName(QStringLiteral("InspectorMatColor_%1").arg(key));
            colorBtn->setFixedSize(24, 20);
            colorBtn->setToolTip(Tr("Выбрать цвет", "Choose color"));
            updateColor(colorBtn, initVals);
            layout->addWidget(colorBtn);
        }

        auto applyChange = [this, spins, compCount, key, path, colorBtn, updateColor] {
            std::array<float, 4> v{};
            for (size_t i = 0; i < spins.size(); ++i) v[i] = static_cast<float>(spins[i]->value());
            if (colorBtn) updateColor(colorBtn, v);
            if (onParameterChanged) onParameterChanged(rawMaterialPath_, key, v);
            UpdateFileParam(path, key, v, compCount);
        };

        for (auto* spin : spins)
            QObject::connect(spin, &QDoubleSpinBox::valueChanged, [applyChange] { applyChange(); });

        if (colorBtn)
        {
            QObject::connect(colorBtn, &QPushButton::clicked, [spins, compCount, key, colorBtn, applyChange] {
                std::array<float, 4> curr{};
                for (size_t i = 0; i < spins.size(); ++i) curr[i] = static_cast<float>(spins[i]->value());
                auto ch = [](float x) { return std::clamp(x, 0.0f, 1.0f); };
                const QColor initial = QColor::fromRgbF(ch(curr[0]), ch(curr[1]), ch(curr[2]), compCount == 4 ? ch(curr[3]) : 1.0f);
                QColorDialog::ColorDialogOptions opts;
                if (compCount == 4) opts |= QColorDialog::ShowAlphaChannel;
                const QColor picked = QColorDialog::getColor(initial, colorBtn, key, opts);
                if (!picked.isValid()) return;
                if (spins.size() > 0) spins[0]->setValue(picked.redF());
                if (spins.size() > 1) spins[1]->setValue(picked.greenF());
                if (spins.size() > 2) spins[2]->setValue(picked.blueF());
                if (spins.size() > 3) spins[3]->setValue(picked.alphaF());
                applyChange();
            });
        }
        paramsRowsLayout_->addWidget(row);
    }

    void RebuildPublicParams(const QString& path)
    {
        ClearParams();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (!doc.isObject()) return;
        const QJsonObject root = doc.object();
        const QJsonObject paramsObj = root.value(QStringLiteral("params")).toObject();
        if (paramsObj.isEmpty()) return;

        QSet<QString> publicSet;
        const bool hasFilter = root.contains(QStringLiteral("publicParams"));
        if (hasFilter)
        {
            const QJsonArray arr = root.value(QStringLiteral("publicParams")).toArray();
            for (const auto& v : arr) publicSet.insert(v.toString());
        }

        bool hasAny = false;
        const QStringList keys = paramsObj.keys();
        for (const QString& key : keys)
        {
            if (hasFilter && !publicSet.contains(key)) continue;
            hasAny = true;
            BuildParamItem(key, paramsObj.value(key), path);
        }
        if (labelParamsTitle_) labelParamsTitle_->setVisible(hasAny);
    }

    void BuildCard()
    {
        SetInfo(Tr("Материал", "Material"),
                Tr("Материал объекта [.mat]:\n• Шейдер, слоты текстур и параметры\n• Публичные параметры редактируются ниже\n• Перетаскивание файла из браузера",
                   "Object material [.mat]:\n• Shader, texture slots, and parameters\n• Public parameters editable below\n• Drag-and-drop file from browser"));

        auto* cl = ContentLayout();

        labelMatTitle_ = new QLabel(Tr("Слот материала [.mat]", "Material Slot [.mat]"), Content());
        labelMatTitle_->setObjectName(QStringLiteral("CardSectionLabel"));
        cl->addWidget(labelMatTitle_);

        materialSlot_ = new AssetSlotWidget(EditorCore::AssetKind::Material, Content());
        materialAsset_ = materialSlot_->LineEdit();
        materialAsset_->setObjectName(QStringLiteral("MaterialAsset"));
        materialSlot_->onAssetDropped = [this](const QString& path) { if (onMaterialDropped) onMaterialDropped(path); };
        materialSlot_->onClearRequested = [this] { if (onClearMaterial) onClearMaterial(); };
        cl->addWidget(materialSlot_);

        materialActions_ = new QWidget(Content());
        auto* lMatBtns = new QHBoxLayout(materialActions_);
        InspectorCardStyle::ApplyRowLayout(lMatBtns);

        editMaterialButton_ = new QPushButton(Tr("Создать", "Create"), materialActions_);
        editMaterialButton_->setObjectName(QStringLiteral("CreateMaterial"));
        editMaterialButton_->setFixedHeight(InspectorCardStyle::kButtonHeight);
        QObject::connect(editMaterialButton_, &QPushButton::clicked, [this] {
            if (onEditMaterial) onEditMaterial(rawMaterialPath_);
        });
        lMatBtns->addWidget(editMaterialButton_);

        chooseMaterialButton_ = new QPushButton(Tr("Выбрать…", "Browse…"), materialActions_);
        chooseMaterialButton_->setObjectName(QStringLiteral("ChooseMaterial"));
        chooseMaterialButton_->setFixedHeight(InspectorCardStyle::kButtonHeight);
        QObject::connect(chooseMaterialButton_, &QPushButton::clicked, [this] {
            if (onChooseMaterial) onChooseMaterial();
        });
        lMatBtns->addWidget(chooseMaterialButton_);

        clearMaterialButton_ = new QPushButton(Tr("Убрать", "Clear"), materialActions_);
        clearMaterialButton_->setObjectName(QStringLiteral("ClearMaterial"));
        clearMaterialButton_->setFixedHeight(InspectorCardStyle::kButtonHeight);
        QObject::connect(clearMaterialButton_, &QPushButton::clicked, [this] {
            if (onClearMaterial) onClearMaterial();
        });
        lMatBtns->addWidget(clearMaterialButton_);
        cl->addWidget(materialActions_);

        labelParamsTitle_ = new QLabel(Tr("Параметры", "Parameters"), Content());
        labelParamsTitle_->setObjectName(QStringLiteral("CardSectionLabel"));
        labelParamsTitle_->hide();
        cl->addWidget(labelParamsTitle_);

        paramsContainer_ = new QWidget(Content());
        paramsRowsLayout_ = new QVBoxLayout(paramsContainer_);
        paramsRowsLayout_->setContentsMargins(0, 0, 0, 0);
        paramsRowsLayout_->setSpacing(4);
        cl->addWidget(paramsContainer_);
    }

    AssetSlotWidget* materialSlot_ = nullptr;
    QLineEdit* materialAsset_ = nullptr;
    QWidget* materialActions_ = nullptr;
    QPushButton* editMaterialButton_ = nullptr;
    QPushButton* chooseMaterialButton_ = nullptr;
    QPushButton* clearMaterialButton_ = nullptr;
    QLabel* labelMatTitle_ = nullptr;
    QLabel* labelParamsTitle_ = nullptr;
    QWidget* paramsContainer_ = nullptr;
    QVBoxLayout* paramsRowsLayout_ = nullptr;
    QString cachedMaterialPath_;
    QString rawMaterialPath_;
    bool cachedMaterialExists_ = true;
};
