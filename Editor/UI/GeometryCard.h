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

    void Clear()
    {
        modelAsset_->clear();
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
    }

    std::function<void()> onChooseModel;
    std::function<void()> onClearModel;

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
        modelAsset_->setPlaceholderText(Tr("нет модели", "none"));
        cl->addWidget(modelAsset_);

        auto* btnRow = new QWidget(Content());
        auto* lBtns = new QHBoxLayout(btnRow);
        lBtns->setContentsMargins(0, 0, 0, 0);
        lBtns->setSpacing(4);

        chooseModel_ = new QPushButton(Tr("Выбрать модель…", "Choose Model…"), btnRow);
        chooseModel_->setObjectName(QStringLiteral("ChooseModel"));
        QObject::connect(chooseModel_, &QPushButton::clicked, [this] {
            if (onChooseModel) onChooseModel();
        });
        lBtns->addWidget(chooseModel_);

        clearModel_ = new QPushButton(Tr("Очистить", "Clear"), btnRow);
        clearModel_->setObjectName(QStringLiteral("ClearModel"));
        QObject::connect(clearModel_, &QPushButton::clicked, [this] {
            if (onClearModel) onClearModel();
        });
        lBtns->addWidget(clearModel_);

        cl->addWidget(btnRow);
    }

    QLabel* labelModelTitle_ = nullptr;
    QLineEdit* modelAsset_ = nullptr;
    QPushButton* chooseModel_ = nullptr;
    QPushButton* clearModel_ = nullptr;
};

