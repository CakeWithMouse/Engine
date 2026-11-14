#include "FsmBlockListEditor.h"
#include "EditorLocale.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using EditorCore::FsmGraphSchema;

namespace
{
bool Ru() { return EditorLocale::Instance().Language() == EditorLanguage::Ru; }

QDoubleSpinBox* Number(QWidget* parent, double minimum, double maximum, double value)
{
    auto* box = new QDoubleSpinBox(parent);
    box->setRange(std::max(minimum, -1e9), std::min(maximum, 1e9));
    box->setDecimals(3);
    box->setSingleStep(0.1);
    box->setValue(value);
    box->setKeyboardTracking(false);
    return box;
}
} // namespace

FsmBlockListEditor::FsmBlockListEditor(const FsmGraphSchema& schema, const QString& kind, const QString& prefix, QWidget* parent)
    : QWidget(parent), schema_(schema), kind_(kind), prefix_(prefix)
{
    setObjectName(prefix);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    list_ = new QListWidget(this);
    list_->setObjectName(prefix + QStringLiteral("List"));
    list_->setMinimumHeight(70);
    connect(list_, &QListWidget::currentRowChanged, this, [this] { if (!refreshing_) ShowParams(); });
    layout->addWidget(list_, 1);

    auto* buttons = new QHBoxLayout;
    add_ = new QToolButton(this);
    add_->setObjectName(prefix + QStringLiteral("Add"));
    add_->setText(Tr("+ Добавить", "+ Add"));
    add_->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(add_);
    connect(menu, &QMenu::aboutToShow, this, [this, menu] {
        menu->clear();
        bool separated = false;
        for (const auto* block : schema_.Blocks(kind_))
        {
            if (!block->engine && !separated)
            {
                menu->addSection(Tr("Блоки игры", "Game blocks"));
                separated = true;
            }
            QAction* action = menu->addAction(Ru() ? block->nameRu : block->nameEn);
            action->setToolTip(Ru() ? block->hintRu : block->hintEn);
            const QString type = block->type;
            connect(action, &QAction::triggered, this, [this, type] { AddBlock(type); });
        }
    });
    menu->setToolTipsVisible(true);
    add_->setMenu(menu);
    remove_ = new QPushButton(Tr("Удалить", "Remove"), this);
    remove_->setObjectName(prefix + QStringLiteral("Remove"));
    connect(remove_, &QPushButton::clicked, this, [this] {
        const int row = CurrentRow();
        if (row < 0) return;
        blocks_.removeAt(row);
        RefreshList();
        SelectBlock(std::min(row, static_cast<int>(blocks_.size()) - 1));
        if (changed) changed();
    });
    up_ = new QPushButton(QStringLiteral("↑"), this);
    up_->setObjectName(prefix + QStringLiteral("Up"));
    up_->setToolTip(Tr("Выше: раньше по порядку", "Up: earlier in order"));
    connect(up_, &QPushButton::clicked, this, [this] { Move(-1); });
    down_ = new QPushButton(QStringLiteral("↓"), this);
    down_->setObjectName(prefix + QStringLiteral("Down"));
    connect(down_, &QPushButton::clicked, this, [this] { Move(1); });
    for (auto* button : {up_, down_}) button->setFixedWidth(28);
    buttons->addWidget(add_);
    buttons->addWidget(remove_);
    buttons->addStretch(1);
    buttons->addWidget(up_);
    buttons->addWidget(down_);
    layout->addLayout(buttons);

    params_ = new QWidget(this);
    params_->setObjectName(prefix + QStringLiteral("Params"));
    form_ = new QFormLayout(params_);
    form_->setContentsMargins(0, 4, 0, 0);
    layout->addWidget(params_);
    RefreshList();
}

void FsmBlockListEditor::SetBlocks(const QVariantList& blocks)
{
    blocks_ = blocks;
    RefreshList();
    SelectBlock(blocks_.isEmpty() ? -1 : 0);
}

int FsmBlockListEditor::CurrentRow() const { return list_->currentRow(); }

void FsmBlockListEditor::SelectBlock(int row)
{
    list_->setCurrentRow(row);
    ShowParams();
}

bool FsmBlockListEditor::AddBlock(const QString& type)
{
    const FsmGraphSchema::Block* block = schema_.FindBlock(type);
    if (!block || block->kind != kind_) return false;
    QVariantMap item{{QStringLiteral("type"), type}};
    // Ключ подставляется сразу, если подходит единственная переменная.
    for (const auto& param : block->params)
        if (param.type.startsWith(QStringLiteral("key")) && param.required)
        {
            const QStringList keys = schema_.KeysFor(param.type);
            if (!keys.isEmpty()) item.insert(param.name, keys.first());
        }
    blocks_.append(item);
    RefreshList();
    SelectBlock(static_cast<int>(blocks_.size()) - 1);
    if (changed) changed();
    return true;
}

void FsmBlockListEditor::RefreshList()
{
    refreshing_ = true;
    const int row = list_->currentRow();
    list_->clear();
    for (const auto& block : blocks_)
    {
        auto* item = new QListWidgetItem(schema_.BlockSummary(block.toMap()), list_);
        const auto* info = schema_.FindBlock(block.toMap().value(QStringLiteral("type")).toString());
        if (info) item->setToolTip(Ru() ? info->hintRu : info->hintEn);
    }
    list_->setCurrentRow(std::min(row, list_->count() - 1));
    refreshing_ = false;
    remove_->setEnabled(!blocks_.isEmpty());
    up_->setEnabled(blocks_.size() > 1);
    down_->setEnabled(blocks_.size() > 1);
}

void FsmBlockListEditor::Move(int delta)
{
    const int row = CurrentRow(), target = row + delta;
    if (row < 0 || target < 0 || target >= blocks_.size()) return;
    blocks_.swapItemsAt(row, target);
    RefreshList();
    SelectBlock(target);
    if (changed) changed();
}

void FsmBlockListEditor::SetParam(int row, const QString& name, const QVariant& value)
{
    if (row < 0 || row >= blocks_.size()) return;
    QVariantMap block = blocks_[row].toMap();
    if (value.isValid()) block.insert(name, value);
    else block.remove(name);
    if (block == blocks_[row].toMap()) return;
    blocks_[row] = block;
    if (auto* item = list_->item(row)) item->setText(schema_.BlockSummary(block));
    if (changed) changed();
}

void FsmBlockListEditor::ShowParams()
{
    while (form_->rowCount() > 0) form_->removeRow(0);
    const int row = CurrentRow();
    if (row < 0 || row >= blocks_.size()) return;
    const QVariantMap block = blocks_[row].toMap();
    const QString type = block.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("Ai.Any") || type == QStringLiteral("Ai.All"))
    {
        form_->addRow(new QLabel(Tr("Группа условий: её состав правится в файле .fsm.", "A group of conditions: edit its items in the .fsm file."), params_));
    }
    const FsmGraphSchema::Block* info = schema_.FindBlock(type);
    if (!info && type != QStringLiteral("Ai.Any") && type != QStringLiteral("Ai.All"))
    {
        form_->addRow(new QLabel(Tr("Блока «%1» нет в каталоге игрового модуля.", "Block \"%1\" is not in the game module's catalog.").arg(type), params_));
        return;
    }
    if (info && !(Ru() ? info->hintRu : info->hintEn).isEmpty())
    {
        auto* hint = new QLabel(Ru() ? info->hintRu : info->hintEn, params_);
        hint->setWordWrap(true);
        form_->addRow(hint);
    }
    auto objectName = [this](const QString& param) { return prefix_ + QStringLiteral("Param_") + param; };

    if (info)
        for (const auto& param : info->params)
        {
            const QString label = Ru() ? param.labelRu : param.labelEn;
            const QVariant current = block.contains(param.name) ? block.value(param.name) : param.fallback.toVariant();
            QWidget* editor = nullptr;
            if (param.type == QStringLiteral("bool"))
            {
                auto* box = new QCheckBox(params_);
                box->setChecked(current.toBool());
                connect(box, &QCheckBox::toggled, this, [this, row, n = param.name](bool on) { SetParam(row, n, on); });
                editor = box;
            }
            else if (param.type == QStringLiteral("int"))
            {
                auto* box = new QSpinBox(params_);
                box->setRange(static_cast<int>(std::max(param.minimum, -1e9)), static_cast<int>(std::min(param.maximum, 1e9)));
                box->setValue(current.toInt());
                box->setKeyboardTracking(false);
                connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, row, n = param.name](int v) { SetParam(row, n, v); });
                editor = box;
            }
            else if (param.type == QStringLiteral("float"))
            {
                auto* box = Number(params_, param.minimum, param.maximum, current.toDouble());
                connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, row, n = param.name](double v) { SetParam(row, n, v); });
                editor = box;
            }
            else if (param.type == QStringLiteral("vec3"))
            {
                auto* line = new QWidget(params_);
                auto* h = new QHBoxLayout(line);
                h->setContentsMargins(0, 0, 0, 0);
                const QVariantList list = current.toList();
                QVector<QDoubleSpinBox*> boxes;
                for (int i = 0; i < 3; ++i)
                {
                    boxes << Number(line, -1e9, 1e9, i < list.size() ? list[i].toDouble() : 0.0);
                    boxes.last()->setObjectName(objectName(param.name) + QString::number(i));
                    h->addWidget(boxes.last());
                }
                for (auto* box : boxes)
                    connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, row, n = param.name, boxes] {
                        SetParam(row, n, QVariantList{boxes[0]->value(), boxes[1]->value(), boxes[2]->value()});
                    });
                editor = line;
            }
            else if (param.type.startsWith(QStringLiteral("key")))
            {
                auto* combo = new QComboBox(params_);
                const QStringList keys = schema_.KeysFor(param.type);
                // Необязательный ключ (чей скрипт): пусто — сам персонаж.
                if (!param.required) combo->addItem(Tr("— сам персонаж —", "— the character itself —"), QString());
                for (const QString& name : keys) combo->addItem(name, name);
                const QString chosen = current.toString();
                if (!chosen.isEmpty() && !keys.contains(chosen)) combo->addItem(chosen, chosen); // ключа нет в памяти: загрузчик скажет
                combo->setCurrentIndex(chosen.isEmpty() ? (param.required ? -1 : 0) : combo->findData(chosen));
                combo->setToolTip(Tr("Ключи памяти подходящего типа. Новый ключ добавляется на панели «Память».",
                                     "Memory keys of a fitting type. Add a key on the Memory panel."));
                connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, row, combo, n = param.name](int index) {
                    const QString key = combo->itemData(index).toString();
                    SetParam(row, n, key.isEmpty() ? QVariant() : QVariant(key));
                    QTimer::singleShot(0, this, [this] { ShowParams(); }); // значение (value) зависит от типа ключа; не из сигнала поля
                });
                editor = combo;
            }
            else if (param.type == QStringLiteral("value"))
            {
                const QString keyType = schema_.VariableType(block.value(QStringLiteral("key")).toString());
                if (keyType == QStringLiteral("bool"))
                {
                    auto* box = new QCheckBox(params_);
                    box->setChecked(current.toBool());
                    connect(box, &QCheckBox::toggled, this, [this, row, n = param.name](bool on) { SetParam(row, n, on); });
                    editor = box;
                }
                else
                {
                    auto* line = new QLineEdit(params_);
                    if (current.metaType().id() == QMetaType::QVariantList)
                    {
                        QStringList parts;
                        for (const auto& v : current.toList()) parts << QString::number(v.toDouble());
                        line->setText(parts.join(QStringLiteral(", ")));
                    }
                    else line->setText(current.toString());
                    line->setPlaceholderText(keyType == QStringLiteral("vec3") ? QStringLiteral("x, y, z") : FsmGraphSchema::TypeLabel(keyType));
                    connect(line, &QLineEdit::editingFinished, this, [this, row, line, keyType, n = param.name] {
                        const QString text = line->text().trimmed();
                        QVariant value;
                        bool ok = true;
                        if (keyType == QStringLiteral("int")) value = text.toInt(&ok);
                        else if (keyType == QStringLiteral("float")) value = QString(text).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
                        else if (keyType == QStringLiteral("vec3"))
                        {
                            const QStringList parts = text.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
                            QVariantList vector;
                            for (const auto& part : parts) vector << part.toDouble(&ok);
                            ok = ok && vector.size() == 3;
                            value = vector;
                        }
                        else value = line->text();
                        if (ok) SetParam(row, n, value);
                    });
                    editor = line;
                }
            }
            else
            {
                auto* line = new QLineEdit(current.toString(), params_);
                connect(line, &QLineEdit::editingFinished, this, [this, row, line, n = param.name] { SetParam(row, n, line->text()); });
                editor = line;
            }
            editor->setObjectName(objectName(param.name));
            form_->addRow(label + (param.required ? QStringLiteral(" *") : QString()), editor);
        }

    if (kind_ == QStringLiteral("condition"))
    {
        auto* negate = new QCheckBox(Tr("Наоборот (НЕ)", "Inverted (NOT)"), params_);
        negate->setObjectName(objectName(QStringLiteral("not")));
        negate->setChecked(block.value(QStringLiteral("not")).toBool());
        connect(negate, &QCheckBox::toggled, this, [this, row](bool on) { SetParam(row, QStringLiteral("not"), on ? QVariant(true) : QVariant()); });
        form_->addRow(negate);
    }
    if (kind_ == QStringLiteral("service"))
    {
        auto* interval = Number(params_, 0, 1e6, block.value(QStringLiteral("interval")).toDouble());
        interval->setObjectName(objectName(QStringLiteral("interval")));
        interval->setToolTip(Tr("0 — при каждом решении персонажа", "0 — on every think of the character"));
        connect(interval, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, row](double v) {
            SetParam(row, QStringLiteral("interval"), v > 0 ? QVariant(v) : QVariant());
        });
        form_->addRow(Tr("Интервал, с", "Interval, s"), interval);
    }
}
