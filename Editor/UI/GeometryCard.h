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

class GeometryCard : public CollapsibleCard
{
public:
    explicit GeometryCard(QWidget* parent = nullptr)
        : CollapsibleCard(Tr("Геометрия", "Geometry"), parent, true)
    {
        BuildCard();
    }

    void SetModelData(const QString& model, bool canChooseModel, const QString& contentRoot = QString(), bool forceCheck = false)
    {
        isPrimitive_ = false;
        primitiveType_.clear();

        const QString rel = model.startsWith(QStringLiteral("/Game/")) ? model.mid(6) : model;
        modelAsset_->setText(rel);
        chooseModel_->setEnabled(canChooseModel);
        chooseModel_->setToolTip(canChooseModel ? QString() : Tr("Замена модели недоступна", "Model replacement unavailable"));

        if (!model.isEmpty() && !contentRoot.isEmpty())
        {
            if (forceCheck || model != cachedModelPath_)
            {
                cachedModelPath_ = model;
                cachedModelExists_ = QFileInfo::exists(QDir(contentRoot).filePath(rel));
            }
            if (!cachedModelExists_)
            {
                modelAsset_->setStyleSheet(QStringLiteral("color: %1;").arg(EditorTheme::Instance().AccentColor().name()));
                modelAsset_->setToolTip(Tr("Файл модели не найден на диске", "Model file not found on disk"));
            }
            else
            {
                modelAsset_->setStyleSheet(QString());
                modelAsset_->setToolTip(QString());
            }
        }
        else
        {
            cachedModelPath_.clear();
            cachedModelExists_ = true;
            modelAsset_->setStyleSheet(QString());
            modelAsset_->setToolTip(QString());
        }
    }

    void SetPrimitiveData(const QString& primitiveType)
    {
        isPrimitive_ = true;
        primitiveType_ = primitiveType;
        cachedModelPath_.clear();
        cachedModelExists_ = true;

        modelAsset_->setText(Tr("Примитив: %1", "Primitive: %1").arg(primitiveType));
        modelAsset_->setStyleSheet(QString());
        modelAsset_->setToolTip(QString());
        chooseModel_->setEnabled(false);
        chooseModel_->setToolTip(Tr("Замена геометрии примитива на модель заблокирована (требуется C-ABI)",
                                   "Replacing primitive geometry with a model is blocked (requires C-ABI)"));
    }

    void UpdateChooseModelButton(bool isModel, bool isPrimitive)
    {
        if (isModel)
        {
            chooseModel_->setEnabled(true);
            chooseModel_->setToolTip(QString());
        }
        else if (isPrimitive)
        {
            chooseModel_->setEnabled(false);
            chooseModel_->setToolTip(Tr("Замена геометрии примитива на модель заблокирована (требуется C-ABI)",
                                       "Replacing primitive geometry with a model is blocked (requires C-ABI)"));
        }
        else
        {
            chooseModel_->setEnabled(false);
            chooseModel_->setToolTip(Tr("Замена модели недоступна", "Model replacement unavailable"));
        }
    }

    void SetData(const QString& model, bool canChooseModel, const QString& contentRoot = QString(), bool forceCheck = false)
    {
        SetModelData(model, canChooseModel, contentRoot, forceCheck);
    }

    void InvalidateCache()
    {
        cachedModelPath_.clear();
    }

    void Clear()
    {
        modelAsset_->clear();
        chooseModel_->setEnabled(false);
        chooseModel_->setToolTip(QString());
        cachedModelPath_.clear();
        cachedModelExists_ = true;
        isPrimitive_ = false;
        primitiveType_.clear();
        modelAsset_->setStyleSheet(QString());
        modelAsset_->setToolTip(QString());
    }

    void RetranslateUi()
    {
        SetTitle(Tr("Геометрия", "Geometry"));
        if (labelModelTitle_) labelModelTitle_->setText(Tr("Модель (.obj / .fbx / .glb)", "Model (.obj / .fbx / .glb)"));
        if (modelAsset_)
        {
            if (isPrimitive_)
                modelAsset_->setText(Tr("Примитив: %1", "Primitive: %1").arg(primitiveType_));
            else if (modelAsset_->text().isEmpty())
                modelAsset_->setPlaceholderText(Tr("нет модели", "none"));
        }
        if (chooseModel_) chooseModel_->setText(Tr("Заменить модель…", "Replace Model…"));
    }

    std::function<void()> onChooseModel;

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

        chooseModel_ = new QPushButton(Tr("Заменить модель…", "Replace Model…"), btnRow);
        chooseModel_->setObjectName(QStringLiteral("ChooseModel"));
        chooseModel_->setFixedHeight(24);
        QObject::connect(chooseModel_, &QPushButton::clicked, [this] {
            if (onChooseModel) onChooseModel();
        });
        lBtns->addWidget(chooseModel_);

        cl->addWidget(btnRow);
    }

    QLabel* labelModelTitle_ = nullptr;
    QLineEdit* modelAsset_ = nullptr;
    QPushButton* chooseModel_ = nullptr;
    QString cachedModelPath_;
    bool cachedModelExists_ = true;
    bool isPrimitive_ = false;
    QString primitiveType_;
};
