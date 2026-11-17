#pragma once

#include "AssetPathDisplay.h"
#include "CollapsibleCard.h"
#include "EditorLocale.h"
#include "EditorTheme.h"
#include "InspectorCardStyle.h"
#include "MaterialSlotList.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <functional>

// Поле общего материала: принимает .mat перетаскиванием из контент-браузера или со слота модели.
class MaterialDropField : public QLineEdit
{
public:
    std::function<void(const QString&)> dropped;
    std::function<QString()> contentRoot;
    std::function<QString()> current; // назначенный /Game путь: в поле он показан сокращённым

    explicit MaterialDropField(QWidget* parent = nullptr) : QLineEdit(parent) { setAcceptDrops(true); }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (!isEnabled() || Dropped(event->mimeData()).isEmpty()) return;
        event->setDropAction(Qt::CopyAction);
        event->accept();
        setStyleSheet(QStringLiteral("border: 1px solid %1;").arg(EditorTheme::Instance().AccentColor().name()));
    }
    void dragMoveEvent(QDragMoveEvent* event) override
    {
        if (Dropped(event->mimeData()).isEmpty()) return;
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
    void dragLeaveEvent(QDragLeaveEvent*) override { setStyleSheet(baseStyle); }
    void dropEvent(QDropEvent* event) override
    {
        setStyleSheet(baseStyle);
        const QString path = Dropped(event->mimeData());
        if (path.isEmpty()) return;
        event->setDropAction(Qt::CopyAction);
        event->accept();
        if ((!current || path != current()) && dropped) dropped(path);
    }

public:
    QString baseStyle;

private:
    QString Dropped(const QMimeData* mime) const
    {
        return MaterialSlotDrag::DroppedMaterial(mime, contentRoot ? contentRoot() : QString());
    }
};

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
        contentRoot_ = contentRoot;
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
                SetFieldStyle(QStringLiteral("color: %1;").arg(EditorTheme::Instance().AccentColor().name()));
                materialAsset_->setToolTip(Tr("Файл материала не найден на диске", "Material file not found on disk"));
                editMaterialButton_->setEnabled(false);
                editMaterialButton_->setToolTip(Tr("Файл материала не найден на диске", "Material file not found on disk"));
            }
            else
            {
                SetFieldStyle(QString());
                editMaterialButton_->setEnabled(true);
                editMaterialButton_->setToolTip(QString());
            }
        }
        else
        {
            cachedMaterialPath_.clear();
            cachedMaterialExists_ = true;
            SetFieldStyle(QString());
            editMaterialButton_->setEnabled(!hasPointLight);
            editMaterialButton_->setToolTip(material.isEmpty()
                ? Tr("Создать материал в Content/Materials, назначить его объекту и открыть редактор",
                     "Create material in Content/Materials, assign to object and open editor")
                : QString());
        }
    }

    // Слоты частей модели (пусто — у объекта один общий материал). Общий материал, если задан, перекрывает их.
    void SetSlots(const QVector<MaterialSlotInfo>& items, bool forceCheck = false)
    {
        hasSlots_ = !items.isEmpty();
        UpdateSharedLabel();
        slotList_->SetSlots(items, contentRoot_, !rawMaterialPath_.isEmpty(), forceCheck);
    }

    MaterialSlotList* SlotList() const { return slotList_; }
    void SetIsolation(int slot, bool available) { slotList_->SetIsolation(slot, available); }

    void InvalidateCache()
    {
        cachedMaterialPath_.clear();
        slotList_->InvalidateCache();
    }

    void Clear()
    {
        rawMaterialPath_.clear();
        materialAsset_->clear();
        materialAsset_->setToolTip(QString());
        slotList_->SetSlots({}, contentRoot_, false, true);
        hasSlots_ = false;
        UpdateSharedLabel();
        editMaterialButton_->setText(Tr("Создать", "Create"));
        editMaterialButton_->setObjectName(QStringLiteral("CreateMaterial"));
        editMaterialButton_->setEnabled(true);
        clearMaterialButton_->setEnabled(false);
        cachedMaterialPath_.clear();
        cachedMaterialExists_ = true;
        SetFieldStyle(QString());
    }

    void RetranslateUi()
    {
        SetTitle(Tr("Материал", "Material"));
        SetInfo(Tr("Материал", "Material"),
                Tr("Материал объекта [.mat].\nПозволяет настроить шейдеры, текстуры и параметры отражения поверхности.",
                   "Object material [.mat].\nConfigures shaders, textures, and surface reflection parameters."));
        UpdateSharedLabel();
        const bool hasMat = !rawMaterialPath_.isEmpty();
        if (editMaterialButton_)
        {
            editMaterialButton_->setText(hasMat ? Tr("Редактировать", "Edit Material") : Tr("Создать", "Create"));
        }
        if (chooseMaterialButton_) chooseMaterialButton_->setText(Tr("Выбрать…", "Browse…"));
        if (clearMaterialButton_) clearMaterialButton_->setText(Tr("Убрать", "Clear"));
        if (slotList_) { slotList_->ApplyStyle(); slotList_->Retranslate(); }
    }

    // Callbacks
    std::function<void(const QString&)> onEditMaterial;
    std::function<void()> onChooseMaterial;
    std::function<void()> onClearMaterial;
    std::function<void(const QString&)> onDropMaterial;            // .mat брошен на общее поле
    std::function<void(int, const QString&)> onAssignSlot;         // слот, /Game путь или пусто
    std::function<void(int)> onChooseSlot;
    std::function<void(int)> onIsolateSlot;                        // -1 — показать все части

private:
    void BuildCard()
    {
        SetInfo(Tr("Материал", "Material"),
                Tr("Материал объекта [.mat].\nПозволяет настроить шейдеры, текстуры и параметры отражения поверхности.",
                   "Object material [.mat].\nConfigures shaders, textures, and surface reflection parameters."));

        auto* cl = ContentLayout();

        labelMatTitle_ = new QLabel(Tr("Слот материала [.mat]", "Material Slot [.mat]"), Content());
        labelMatTitle_->setObjectName(QStringLiteral("CardSectionLabel"));
        cl->addWidget(labelMatTitle_);

        auto* field = new MaterialDropField(Content());
        field->contentRoot = [this] { return contentRoot_; };
        field->current = [this] { return rawMaterialPath_; };
        field->dropped = [this](const QString& path) { if (onDropMaterial) onDropMaterial(path); };
        materialAsset_ = field;
        materialAsset_->setReadOnly(true);
        materialAsset_->setObjectName(QStringLiteral("MaterialAsset"));
        materialAsset_->setFixedHeight(InspectorCardStyle::kFieldHeight);
        materialAsset_->setMinimumWidth(0);
        materialAsset_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        materialAsset_->setPlaceholderText(Tr("по умолчанию", "default"));
        cl->addWidget(materialAsset_);

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

        slotList_ = new MaterialSlotList(Content());
        slotList_->onAssign = [this](int index, const QString& path) { if (onAssignSlot) onAssignSlot(index, path); };
        slotList_->onChoose = [this](int index) { if (onChooseSlot) onChooseSlot(index); };
        slotList_->onEdit = [this](const QString& path) { if (onEditMaterial) onEditMaterial(path); };
        slotList_->onIsolate = [this](int slot) { if (onIsolateSlot) onIsolateSlot(slot); };
        cl->addWidget(slotList_);
    }

    void UpdateSharedLabel()
    {
        if (!labelMatTitle_) return;
        labelMatTitle_->setText(hasSlots_ ? Tr("Общий материал всей модели [.mat]", "Model-wide material [.mat]")
                                          : Tr("Слот материала [.mat]", "Material Slot [.mat]"));
        if (materialAsset_)
            materialAsset_->setPlaceholderText(hasSlots_ ? Tr("не задан — действуют слоты ниже", "not set — slots below apply")
                                                         : Tr("по умолчанию", "default"));
    }

    void SetFieldStyle(const QString& style)
    {
        static_cast<MaterialDropField*>(materialAsset_)->baseStyle = style;
        materialAsset_->setStyleSheet(style);
    }

    QLineEdit* materialAsset_ = nullptr;
    QWidget* materialActions_ = nullptr;
    QPushButton* editMaterialButton_ = nullptr;
    QPushButton* chooseMaterialButton_ = nullptr;
    QPushButton* clearMaterialButton_ = nullptr;
    QLabel* labelMatTitle_ = nullptr;
    MaterialSlotList* slotList_ = nullptr;
    QString contentRoot_;
    bool hasSlots_ = false;
    QString cachedMaterialPath_;
    QString rawMaterialPath_;
    bool cachedMaterialExists_ = true;
};
