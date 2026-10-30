#pragma once

#include "CollapsibleCard.h"
#include "EditorLocale.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <functional>

class MaterialCard : public CollapsibleCard
{
public:
    explicit MaterialCard(QWidget* parent = nullptr)
        : CollapsibleCard(Tr("Материал и Визуал", "Material & Visual"), parent, true)
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

    void SetData(const QString& material, const QString& model, const QString& texture, bool canChooseModel, bool hasPointLight)
    {
        materialAsset_->setText(material);
        modelAsset_->setText(model);
        textureAsset_->setText(texture);
        chooseModel_->setEnabled(canChooseModel);
        editMaterialButton_->setEnabled(!material.isEmpty() || !hasPointLight);
        editMaterialButton_->setToolTip(material.isEmpty()
            ? Tr("Создать материал в Content/Materials, назначить его объекту и открыть редактор",
                 "Create material in Content/Materials, assign to object and open editor")
            : QString());
    }

    void Clear()
    {
        materialAsset_->clear();
        modelAsset_->clear();
        textureAsset_->clear();
        editMaterialButton_->setEnabled(false);
        chooseModel_->setEnabled(false);
    }

    void RetranslateUi()
    {
        SetTitle(Tr("Материал и Визуал", "Material & Visual"));
        if (labelMatTitle_) labelMatTitle_->setText(Tr("Материал (.mat)", "Material (.mat)"));
        if (labelModelTitle_) labelModelTitle_->setText(Tr("Модель", "Model"));
        if (labelTexTitle_) labelTexTitle_->setText(Tr("Текстура", "Texture"));
        if (materialAsset_) materialAsset_->setPlaceholderText(Tr("по умолчанию", "default"));
        if (textureAsset_) textureAsset_->setPlaceholderText(Tr("нет", "none"));
        if (editMaterialButton_) editMaterialButton_->setText(Tr("Редактировать", "Edit Material"));
        if (chooseMaterialButton_) chooseMaterialButton_->setText(Tr("Выбрать…", "Browse…"));
        if (clearMaterialButton_) clearMaterialButton_->setText(Tr("Убрать", "Clear"));
        if (chooseModel_) chooseModel_->setText(Tr("Выбрать модель…", "Choose Model…"));
        if (chooseTextureButton_) chooseTextureButton_->setText(Tr("Выбрать…", "Browse…"));
        if (clearTextureButton_) clearTextureButton_->setText(Tr("Убрать", "Clear"));
    }

    // Callbacks
    std::function<void(const QString&)> onEditMaterial;
    std::function<void()> onChooseMaterial;
    std::function<void()> onClearMaterial;
    std::function<void()> onChooseModel;
    std::function<void()> onChooseTexture;
    std::function<void()> onClearTexture;

private:
    void BuildCard()
    {
        auto* cl = ContentLayout();

        // 1. Material
        labelMatTitle_ = new QLabel(Tr("Материал (.mat)", "Material (.mat)"), Content());
        labelMatTitle_->setObjectName(QStringLiteral("CardSectionLabel"));
        cl->addWidget(labelMatTitle_);

        materialAsset_ = new QLineEdit(Content());
        materialAsset_->setReadOnly(true);
        materialAsset_->setObjectName(QStringLiteral("MaterialAsset"));
        materialAsset_->setPlaceholderText(Tr("по умолчанию", "default"));
        cl->addWidget(materialAsset_);

        materialActions_ = new QWidget(Content());
        auto* lMatBtns = new QHBoxLayout(materialActions_);
        lMatBtns->setContentsMargins(0, 0, 0, 0);
        lMatBtns->setSpacing(4);

        editMaterialButton_ = new QPushButton(Tr("Редактировать", "Edit Material"), materialActions_);
        editMaterialButton_->setObjectName(QStringLiteral("PrimaryActionButton"));
        QObject::connect(editMaterialButton_, &QPushButton::clicked, [this] {
            if (onEditMaterial) onEditMaterial(materialAsset_->text());
        });
        lMatBtns->addWidget(editMaterialButton_);

        chooseMaterialButton_ = new QPushButton(Tr("Выбрать…", "Browse…"), materialActions_);
        chooseMaterialButton_->setObjectName(QStringLiteral("ChooseMaterial"));
        QObject::connect(chooseMaterialButton_, &QPushButton::clicked, [this] {
            if (onChooseMaterial) onChooseMaterial();
        });
        lMatBtns->addWidget(chooseMaterialButton_);

        clearMaterialButton_ = new QPushButton(Tr("Убрать", "Clear"), materialActions_);
        clearMaterialButton_->setObjectName(QStringLiteral("ClearMaterial"));
        QObject::connect(clearMaterialButton_, &QPushButton::clicked, [this] {
            if (onClearMaterial) onClearMaterial();
        });
        lMatBtns->addWidget(clearMaterialButton_);
        cl->addWidget(materialActions_);

        // 2. Model
        labelModelTitle_ = new QLabel(Tr("Модель", "Model"), Content());
        labelModelTitle_->setObjectName(QStringLiteral("CardSectionLabel"));
        cl->addWidget(labelModelTitle_);

        modelAsset_ = new QLineEdit(Content());
        modelAsset_->setReadOnly(true);
        modelAsset_->setObjectName(QStringLiteral("ModelAsset"));
        cl->addWidget(modelAsset_);

        chooseModel_ = new QPushButton(Tr("Выбрать модель…", "Choose Model…"), Content());
        chooseModel_->setObjectName(QStringLiteral("ChooseModel"));
        QObject::connect(chooseModel_, &QPushButton::clicked, [this] {
            if (onChooseModel) onChooseModel();
        });
        cl->addWidget(chooseModel_);

        // 3. Texture
        labelTexTitle_ = new QLabel(Tr("Текстура", "Texture"), Content());
        labelTexTitle_->setObjectName(QStringLiteral("CardSectionLabel"));
        cl->addWidget(labelTexTitle_);

        textureAsset_ = new QLineEdit(Content());
        textureAsset_->setReadOnly(true);
        textureAsset_->setObjectName(QStringLiteral("TextureAsset"));
        textureAsset_->setPlaceholderText(Tr("нет", "none"));
        cl->addWidget(textureAsset_);

        auto* rowTexBtns = new QWidget(Content());
        auto* lTexBtns = new QHBoxLayout(rowTexBtns);
        lTexBtns->setContentsMargins(0, 0, 0, 0);
        lTexBtns->setSpacing(4);

        chooseTextureButton_ = new QPushButton(Tr("Выбрать…", "Browse…"), rowTexBtns);
        chooseTextureButton_->setObjectName(QStringLiteral("ChooseTexture"));
        QObject::connect(chooseTextureButton_, &QPushButton::clicked, [this] {
            if (onChooseTexture) onChooseTexture();
        });
        lTexBtns->addWidget(chooseTextureButton_);

        clearTextureButton_ = new QPushButton(Tr("Убрать", "Clear"), rowTexBtns);
        clearTextureButton_->setObjectName(QStringLiteral("ClearTexture"));
        QObject::connect(clearTextureButton_, &QPushButton::clicked, [this] {
            if (onClearTexture) onClearTexture();
        });
        lTexBtns->addWidget(clearTextureButton_);
        cl->addWidget(rowTexBtns);
    }

    QLineEdit* materialAsset_ = nullptr;
    QWidget* materialActions_ = nullptr;
    QPushButton* editMaterialButton_ = nullptr;
    QPushButton* chooseMaterialButton_ = nullptr;
    QPushButton* clearMaterialButton_ = nullptr;

    QLineEdit* modelAsset_ = nullptr;
    QPushButton* chooseModel_ = nullptr;

    QLineEdit* textureAsset_ = nullptr;
    QPushButton* chooseTextureButton_ = nullptr;
    QPushButton* clearTextureButton_ = nullptr;

    QLabel* labelMatTitle_ = nullptr;
    QLabel* labelModelTitle_ = nullptr;
    QLabel* labelTexTitle_ = nullptr;
};
