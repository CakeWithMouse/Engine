#pragma once

#include "MaterialSlotRow.h"

#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QVector>
#include <algorithm>
#include <functional>

// Список слотов материалов модели внутри карточки «Материал».
class MaterialSlotList : public QWidget
{
public:
    // Фильтр по имени показывается, только когда слотов больше этого числа.
    static constexpr int kFilterThreshold = 8;

    std::function<void(int, const QString&)> onAssign; // индекс слота, /Game путь или пусто (стандартный)
    std::function<void(int)> onChoose;
    std::function<void(const QString&)> onEdit;
    std::function<void(int)> onIsolate; // слот, части которого показать; -1 — все части

    explicit MaterialSlotList(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("MaterialSlotList"));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 4, 0, 0);
        layout->setSpacing(2);

        header_ = new QLabel(this);
        header_->setObjectName(QStringLiteral("CardSectionLabel"));
        layout->addWidget(header_);

        overrideHint_ = new QLabel(this);
        overrideHint_->setObjectName(QStringLiteral("MaterialSlotOverrideHint"));
        overrideHint_->setWordWrap(true);
        layout->addWidget(overrideHint_);

        filter_ = new QLineEdit(this);
        filter_->setObjectName(QStringLiteral("MaterialSlotFilter"));
        filter_->setClearButtonEnabled(true);
        filter_->setFixedHeight(22);
        QObject::connect(filter_, &QLineEdit::textChanged, this, [this] { ApplyFilter(); });
        layout->addWidget(filter_);

        isolationHint_ = new QLabel(this);
        isolationHint_->setObjectName(QStringLiteral("MaterialSlotIsolationHint"));
        isolationHint_->setWordWrap(true);
        isolationHint_->hide();
        layout->addWidget(isolationHint_);

        rows_ = new QWidget(this);
        rowsLayout_ = new QVBoxLayout(rows_);
        rowsLayout_->setContentsMargins(0, 0, 0, 0);
        rowsLayout_->setSpacing(1);
        layout->addWidget(rows_);

        hint_ = new QLabel(this);
        hint_->setObjectName(QStringLiteral("MaterialSlotHint"));
        hint_->setWordWrap(true);
        layout->addWidget(hint_);

        ApplyStyle();
        Retranslate();
        hide();
    }

    void SetSlots(const QVector<MaterialSlotInfo>& items, const QString& contentRoot, bool overridden, bool forceCheck)
    {
        if (forceCheck || contentRoot != contentRoot_) exists_.clear();
        contentRoot_ = contentRoot;
        const bool layoutChanged = items.size() != slots_.size()
            || !std::equal(items.begin(), items.end(), slots_.begin(), [](const auto& a, const auto& b) { return a.index == b.index && a.name == b.name; });
        if (!layoutChanged && !forceCheck && items == slots_ && overridden == overridden_) return;
        if (layoutChanged) { Rebuild(items); SetIsolation(-1, isolationAvailable_); }
        slots_ = items;
        overridden_ = overridden;
        for (int i = 0; i < rowWidgets_.size(); ++i)
            rowWidgets_[i]->SetInfo(items[i], Exists(items[i].path), overridden);
        header_->setText(Tr("Материалы частей модели: %1", "Model part materials: %1").arg(items.size()));
        overrideHint_->setVisible(overridden);
        filter_->setVisible(items.size() > kFilterThreshold);
        setVisible(!items.isEmpty());
    }

    void InvalidateCache() { exists_.clear(); }

    // Активный слот изоляции из снимка сцены (-1 — показаны все части); available — модуль умеет изолировать.
    void SetIsolation(int slot, bool available)
    {
        isolationAvailable_ = available;
        isolatedSlot_ = available ? slot : -1;
        QString name;
        for (auto* row : rowWidgets_)
        {
            const bool active = row->Info().index == isolatedSlot_;
            row->SetActive(active);
            row->SetIsolationAvailable(available);
            if (active) name = row->Info().name.isEmpty() ? QString::number(row->Info().index) : row->Info().name;
        }
        isolationHint_->setVisible(isolatedSlot_ >= 0);
        if (isolatedSlot_ >= 0)
            isolationHint_->setText(Tr("Во вьюпорте показаны только части «%1». Нажмите глаз ещё раз, чтобы показать всю модель.",
                                       "The viewport shows only the \"%1\" parts. Press the eye again to show the whole model.").arg(name));
    }
    int IsolatedSlot() const { return isolatedSlot_; }

    void Retranslate()
    {
        header_->setText(Tr("Материалы частей модели: %1", "Model part materials: %1").arg(slots_.size()));
        overrideHint_->setText(Tr("Общий материал выше перекрывает все слоты. Уберите его, чтобы части модели получили свои материалы.",
                                  "The model-wide material above overrides every slot. Clear it to give each part its own material."));
        filter_->setPlaceholderText(Tr("Фильтр по имени или материалу…", "Filter by name or material…"));
        hint_->setText(Tr("Перетащите .mat из контент-браузера на строку, или строку на другую строку, чтобы скопировать материал.",
                          "Drop a .mat from the Content Browser onto a row, or drag a row onto another to copy its material."));
        for (auto* row : rowWidgets_) row->Retranslate();
        if (!slots_.isEmpty()) SetSlots(slots_, contentRoot_, overridden_, true);
    }

    void ApplyStyle()
    {
        const auto pal = EditorTheme::Instance().Palette();
        QColor active = EditorTheme::Instance().AccentColor();
        active.setAlpha(48);
        setStyleSheet(QStringLiteral(
            "QFrame[materialSlotRow=\"true\"] { background: %1; border: 1px solid transparent; border-radius: 3px; }"
            "QFrame[materialSlotRow=\"true\"]:hover { border-color: %2; }"
            "QFrame[materialSlotRow=\"true\"][activeSlot=\"true\"] { border-color: %3; background: %5; }"
            "QFrame[materialSlotRow=\"true\"][dropTarget=\"true\"] { border-color: %3; }"
            "QLabel#MaterialSlotIsolationHint { color: %3; }"
            "QLabel#MaterialSlotOverrideHint { color: %3; }"
            "QLabel#MaterialSlotHint { color: %4; }")
            .arg(pal.bgDarkest.name(), pal.borderMid.name(), EditorTheme::Instance().AccentColor().name(), pal.textDisabled.name(), active.name(QColor::HexArgb)));
    }

private:
    bool Exists(const QString& path)
    {
        if (path.isEmpty() || contentRoot_.isEmpty()) return true;
        auto found = exists_.find(path);
        if (found == exists_.end()) found = exists_.insert(path, QFileInfo::exists(MaterialSlotDrag::DiskPath(contentRoot_, path)));
        return *found;
    }

    void Rebuild(const QVector<MaterialSlotInfo>& items)
    {
        qDeleteAll(rowWidgets_);
        rowWidgets_.clear();
        for (const auto& slot : items)
        {
            auto* row = new MaterialSlotRow(slot, rows_);
            const int index = slot.index;
            row->contentRoot = [this] { return contentRoot_; };
            row->dropped = [this, index](const QString& path) { if (onAssign) onAssign(index, path); };
            row->clear = [this, index] { if (onAssign) onAssign(index, QString()); };
            row->choose = [this, index] { if (onChoose) onChoose(index); };
            row->edit = [this, row] { if (onEdit) onEdit(row->Info().path); };
            row->activate = [this, index] {
                if (isolationAvailable_ && onIsolate) onIsolate(isolatedSlot_ == index ? -1 : index);
            };
            rowsLayout_->addWidget(row);
            rowWidgets_.push_back(row);
        }
        ApplyFilter();
    }

    void ApplyFilter()
    {
        const QString text = filter_->text().trimmed();
        for (auto* row : rowWidgets_)
            row->setVisible(text.isEmpty() || row->SearchText().contains(text, Qt::CaseInsensitive));
    }

    QLabel* header_ = nullptr;
    QLabel* overrideHint_ = nullptr;
    QLineEdit* filter_ = nullptr;
    QWidget* rows_ = nullptr;
    QVBoxLayout* rowsLayout_ = nullptr;
    QLabel* hint_ = nullptr;
    QVector<MaterialSlotRow*> rowWidgets_;
    QVector<MaterialSlotInfo> slots_;
    QHash<QString, bool> exists_;
    QString contentRoot_;
    bool overridden_ = false;
    QLabel* isolationHint_ = nullptr;
    bool isolationAvailable_ = false;
    int isolatedSlot_ = -1;
};
