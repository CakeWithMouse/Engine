#pragma once

#include "CollapsibleCard.h"
#include "EditorLocale.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <functional>

class GeometryCard : public CollapsibleCard
{
public:
    explicit GeometryCard(QWidget* parent = nullptr)
        : CollapsibleCard(Tr("Геометрия", "Geometry"), parent, true)
    {
        BuildCard();
    }

    void SetData(const QString& model, bool canChooseModel)
    {
        modelAsset_->setText(model);
        chooseModel_->setEnabled(canChooseModel);
        clearModel_->setEnabled(!model.isEmpty());
    }

    void SetTexture(const QString& texture) { textureAsset_->setText(texture); clearTexture_->setEnabled(!texture.isEmpty()); }

    void Clear()
    {
        modelAsset_->clear();
        SetTexture({});
        chooseModel_->setEnabled(false);
        clearModel_->setEnabled(false);
    }

    void RetranslateUi()
    {
        SetTitle(Tr("Геометрия", "Geometry"));
        if (labelModelTitle_) labelModelTitle_->setText(Tr("Модель (.obj / .fbx / .glb)", "Model (.obj / .fbx / .glb)"));
        if (modelAsset_) modelAsset_->setPlaceholderText(Tr("нет модели", "none"));
        if (chooseModel_) chooseModel_->setText(Tr("Выбрать модель…", "Choose Model…"));
        if (clearModel_) clearModel_->setText(Tr("Очистить", "Clear"));
        chooseTexture_->setText(Tr("Выбрать текстуру…", "Choose Texture…"));
        clearTexture_->setText(Tr("Убрать текстуру", "Clear Texture"));
    }

    std::function<void()> onChooseModel;
    std::function<void()> onClearModel;
    std::function<void()> onChooseTexture, onClearTexture;

private:
    void BuildCard()
    {
        auto* cl = ContentLayout();

        labelModelTitle_ = new QLabel(Tr("Модель (.obj / .fbx / .glb)", "Model (.obj / .fbx / .glb)"), Content());
        labelModelTitle_->setObjectName(QStringLiteral("CardSectionLabel"));
        cl->addWidget(labelModelTitle_);

        modelAsset_ = new QLineEdit(Content());
        modelAsset_->setReadOnly(true);
        modelAsset_->setObjectName(QStringLiteral("ModelAsset"));
        modelAsset_->setFixedHeight(22);
        modelAsset_->setPlaceholderText(Tr("нет модели", "none"));
        cl->addWidget(modelAsset_);

        auto* btnRow = new QWidget(Content());
        auto* lBtns = new QHBoxLayout(btnRow);
        lBtns->setContentsMargins(0, 0, 0, 0);
        lBtns->setSpacing(4);

        chooseModel_ = new QPushButton(Tr("Выбрать модель…", "Choose Model…"), btnRow);
        chooseModel_->setObjectName(QStringLiteral("ChooseModel"));
        chooseModel_->setFixedHeight(24);
        QObject::connect(chooseModel_, &QPushButton::clicked, [this] {
            if (onChooseModel) onChooseModel();
        });
        lBtns->addWidget(chooseModel_);

        clearModel_ = new QPushButton(Tr("Очистить", "Clear"), btnRow);
        clearModel_->setObjectName(QStringLiteral("ClearModel"));
        clearModel_->setFixedHeight(24);
        QObject::connect(clearModel_, &QPushButton::clicked, [this] {
            if (onClearModel) onClearModel();
        });
        lBtns->addWidget(clearModel_);

        cl->addWidget(btnRow);
        textureAsset_ = new QLineEdit(Content()); textureAsset_->setObjectName("TextureAsset"); textureAsset_->setReadOnly(true); cl->addWidget(textureAsset_);
        auto* textureRow = new QHBoxLayout;
        chooseTexture_ = new QPushButton(Tr("Выбрать текстуру…", "Choose Texture…"), Content()); chooseTexture_->setObjectName("ChooseTexture");
        clearTexture_ = new QPushButton(Tr("Убрать текстуру", "Clear Texture"), Content()); clearTexture_->setObjectName("ClearTexture");
        connect(chooseTexture_, &QPushButton::clicked, this, [this] { if(onChooseTexture) onChooseTexture(); });
        connect(clearTexture_, &QPushButton::clicked, this, [this] { if(onClearTexture) onClearTexture(); });
        textureRow->addWidget(chooseTexture_); textureRow->addWidget(clearTexture_); cl->addLayout(textureRow);
    }

    QLineEdit* textureAsset_ = nullptr;
    QPushButton* chooseTexture_ = nullptr;
    QPushButton* clearTexture_ = nullptr;
    QLabel* labelModelTitle_ = nullptr;
    QLineEdit* modelAsset_ = nullptr;
    QPushButton* chooseModel_ = nullptr;
    QPushButton* clearModel_ = nullptr;
};

