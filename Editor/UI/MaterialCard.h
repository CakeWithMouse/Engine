#pragma once

#include "CollapsibleCard.h"
#include "EditorLocale.h"
#include "EditorTheme.h"
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
        if (path != text() && dropped) dropped(path);
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
                SetFieldStyle(QStringLiteral("color: %1;").arg(EditorTheme::Instance().AccentColor().name()));
                materialAsset_->setToolTip(Tr("Файл материала не найден на диске", "Material file not found on disk"));
                editMaterialButton_->setEnabled(false);
                editMaterialButton_->setToolTip(Tr("Файл материала не найден на диске", "Material file not found on disk"));
            }
            else
            {
                SetFieldStyle(QString());
                materialAsset_->setToolTip(QString());
                editMaterialButton_->setEnabled(true);
                editMaterialButton_->setToolTip(QString());
            }
        }
        else
        {
            cachedMaterialPath_.clear();
            cachedMaterialExists_ = true;
            SetFieldStyle(QString());
            materialAsset_->setToolTip(QString());
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
        slotList_->SetSlots(items, contentRoot_, !materialAsset_->text().isEmpty(), forceCheck);
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
        materialAsset_->clear();
        slotList_->SetSlots({}, contentRoot_, false, true);
        hasSlots_ = false;
        UpdateSharedLabel();
        editMaterialButton_->setEnabled(false);
        clearMaterialButton_->setEnabled(false);
        cachedMaterialPath_.clear();
        cachedMaterialExists_ = true;
        SetFieldStyle(QString());
        materialAsset_->setToolTip(QString());
    }

    void RetranslateUi()
    {
        SetTitle(Tr("Материал", "Material"));
        UpdateSharedLabel();
        if (editMaterialButton_) editMaterialButton_->setText(Tr("Редактировать", "Edit Material"));
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
        auto* cl = ContentLayout();

        labelMatTitle_ = new QLabel(Tr("Слот материала (.mat)", "Material Slot (.mat)"), Content());
        labelMatTitle_->setObjectName(QStringLiteral("CardSectionLabel"));
        cl->addWidget(labelMatTitle_);

        auto* field = new MaterialDropField(Content());
        field->contentRoot = [this] { return contentRoot_; };
        field->dropped = [this](const QString& path) { if (onDropMaterial) onDropMaterial(path); };
        materialAsset_ = field;
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
        labelMatTitle_->setText(hasSlots_ ? Tr("Общий материал всей модели (.mat)", "Model-wide material (.mat)")
                                          : Tr("Слот материала (.mat)", "Material Slot (.mat)"));
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
    bool cachedMaterialExists_ = true;
};
