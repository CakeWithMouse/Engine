#pragma once

#include "CollapsibleCard.h"
#include "EditorLocale.h"
#include "EditorTheme.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <functional>

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
        materialAsset_->setText(material);
        clearMaterialButton_->setEnabled(!material.isEmpty());

        if (!material.isEmpty() && !contentRoot.isEmpty())
        {
            if (forceCheck || material != cachedMaterialPath_)
            {
                cachedMaterialPath_ = material;
                const QString rel = material.startsWith(QStringLiteral("/Game/")) ? material.mid(6) : material;
                cachedMaterialExists_ = QFileInfo::exists(QDir(contentRoot).filePath(rel));
            }
            if (!cachedMaterialExists_)
            {
                materialAsset_->setStyleSheet(QStringLiteral("color: %1;").arg(EditorTheme::Instance().AccentColor().name()));
                materialAsset_->setToolTip(Tr("Файл материала не найден на диске", "Material file not found on disk"));
                editMaterialButton_->setEnabled(false);
                editMaterialButton_->setToolTip(Tr("Файл материала не найден на диске", "Material file not found on disk"));
            }
            else
            {
                materialAsset_->setStyleSheet(QString());
                materialAsset_->setToolTip(QString());
                editMaterialButton_->setEnabled(true);
                editMaterialButton_->setToolTip(QString());
            }
        }
        else
        {
            cachedMaterialPath_.clear();
            cachedMaterialExists_ = true;
            materialAsset_->setStyleSheet(QString());
            materialAsset_->setToolTip(QString());
            editMaterialButton_->setEnabled(!hasPointLight);
            editMaterialButton_->setToolTip(material.isEmpty()
                ? Tr("Создать материал в Content/Materials, назначить его объекту и открыть редактор",
                     "Create material in Content/Materials, assign to object and open editor")
                : QString());
        }
    }

    void InvalidateCache()
    {
        cachedMaterialPath_.clear();
    }

    void Clear()
    {
        materialAsset_->clear();
        editMaterialButton_->setEnabled(false);
        clearMaterialButton_->setEnabled(false);
        cachedMaterialPath_.clear();
        cachedMaterialExists_ = true;
        materialAsset_->setStyleSheet(QString());
        materialAsset_->setToolTip(QString());
    }

    void RetranslateUi()
    {
        SetTitle(Tr("Материал", "Material"));
        if (labelMatTitle_) labelMatTitle_->setText(Tr("Слот материала (.mat)", "Material Slot (.mat)"));
        if (materialAsset_) materialAsset_->setPlaceholderText(Tr("по умолчанию", "default"));
        if (editMaterialButton_) editMaterialButton_->setText(Tr("Редактировать", "Edit Material"));
        if (chooseMaterialButton_) chooseMaterialButton_->setText(Tr("Выбрать…", "Browse…"));
        if (clearMaterialButton_) clearMaterialButton_->setText(Tr("Убрать", "Clear"));
    }

    // Callbacks
    std::function<void(const QString&)> onEditMaterial;
    std::function<void()> onChooseMaterial;
    std::function<void()> onClearMaterial;

private:
    void BuildCard()
    {
        auto* cl = ContentLayout();

        labelMatTitle_ = new QLabel(Tr("Слот материала (.mat)", "Material Slot (.mat)"), Content());
        labelMatTitle_->setObjectName(QStringLiteral("CardSectionLabel"));
        cl->addWidget(labelMatTitle_);

        materialAsset_ = new QLineEdit(Content());
        materialAsset_->setReadOnly(true);
        materialAsset_->setObjectName(QStringLiteral("MaterialAsset"));
        materialAsset_->setFixedHeight(22);
        materialAsset_->setPlaceholderText(Tr("по умолчанию", "default"));
        cl->addWidget(materialAsset_);

        materialActions_ = new QWidget(Content());
        auto* lMatBtns = new QHBoxLayout(materialActions_);
        lMatBtns->setContentsMargins(0, 0, 0, 0);
        lMatBtns->setSpacing(4);

        editMaterialButton_ = new QPushButton(Tr("Редактировать", "Edit Material"), materialActions_);
        editMaterialButton_->setObjectName(QStringLiteral("EditMaterial"));
        editMaterialButton_->setFixedHeight(24);
        QObject::connect(editMaterialButton_, &QPushButton::clicked, [this] {
            if (onEditMaterial) onEditMaterial(materialAsset_->text());
        });
        lMatBtns->addWidget(editMaterialButton_);

        chooseMaterialButton_ = new QPushButton(Tr("Выбрать…", "Browse…"), materialActions_);
        chooseMaterialButton_->setObjectName(QStringLiteral("ChooseMaterial"));
        chooseMaterialButton_->setFixedHeight(24);
        QObject::connect(chooseMaterialButton_, &QPushButton::clicked, [this] {
            if (onChooseMaterial) onChooseMaterial();
        });
        lMatBtns->addWidget(chooseMaterialButton_);

        clearMaterialButton_ = new QPushButton(Tr("Убрать", "Clear"), materialActions_);
        clearMaterialButton_->setObjectName(QStringLiteral("ClearMaterial"));
        clearMaterialButton_->setFixedHeight(24);
        QObject::connect(clearMaterialButton_, &QPushButton::clicked, [this] {
            if (onClearMaterial) onClearMaterial();
        });
        lMatBtns->addWidget(clearMaterialButton_);
        cl->addWidget(materialActions_);
    }

    QLineEdit* materialAsset_ = nullptr;
    QWidget* materialActions_ = nullptr;
    QPushButton* editMaterialButton_ = nullptr;
    QPushButton* chooseMaterialButton_ = nullptr;
    QPushButton* clearMaterialButton_ = nullptr;
    QLabel* labelMatTitle_ = nullptr;
    QString cachedMaterialPath_;
    bool cachedMaterialExists_ = true;
};
