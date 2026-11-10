#pragma once

#include "CollapsibleCard.h"
#include "EditorLocale.h"

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

    void SetData(const QString& model, bool canChooseModel, const QString& contentRoot = QString(), bool forceCheck = false)
    {
        modelAsset_->setText(model);
        chooseModel_->setEnabled(canChooseModel);
        clearModel_->setEnabled(!model.isEmpty());

        if (!model.isEmpty() && !contentRoot.isEmpty())
        {
            if (forceCheck || model != cachedModelPath_)
            {
                cachedModelPath_ = model;
                const QString rel = model.startsWith(QStringLiteral("/Game/")) ? model.mid(6) : model;
                cachedModelExists_ = QFileInfo::exists(QDir(contentRoot).filePath(rel));
            }
            if (!cachedModelExists_)
            {
                modelAsset_->setStyleSheet(QStringLiteral("color: #E06C75;"));
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

    void SetTexture(const QString& texture, const QString& contentRoot = QString(), bool forceCheck = false)
    {
        textureAsset_->setText(texture);
        clearTexture_->setEnabled(!texture.isEmpty());

        if (!texture.isEmpty() && !contentRoot.isEmpty())
        {
            if (forceCheck || texture != cachedTexturePath_)
            {
                cachedTexturePath_ = texture;
                const QString rel = texture.startsWith(QStringLiteral("/Game/")) ? texture.mid(6) : texture;
                cachedTextureExists_ = QFileInfo::exists(QDir(contentRoot).filePath(rel));
            }
            if (!cachedTextureExists_)
            {
                textureAsset_->setStyleSheet(QStringLiteral("color: #E06C75;"));
                textureAsset_->setToolTip(Tr("Файл текстуры не найден на диске", "Texture file not found on disk"));
            }
            else
            {
                textureAsset_->setStyleSheet(QString());
                textureAsset_->setToolTip(QString());
            }
        }
        else
        {
            cachedTexturePath_.clear();
            cachedTextureExists_ = true;
            textureAsset_->setStyleSheet(QString());
            textureAsset_->setToolTip(QString());
        }
    }

    void Clear()
    {
        modelAsset_->clear();
        SetTexture({});
        chooseModel_->setEnabled(false);
        clearModel_->setEnabled(false);
        cachedModelPath_.clear();
        cachedModelExists_ = true;
        cachedTexturePath_.clear();
        cachedTextureExists_ = true;
        modelAsset_->setStyleSheet(QString());
        modelAsset_->setToolTip(QString());
        textureAsset_->setStyleSheet(QString());
        textureAsset_->setToolTip(QString());
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
    QString cachedModelPath_;
    bool cachedModelExists_ = true;
    QString cachedTexturePath_;
    bool cachedTextureExists_ = true;
};

