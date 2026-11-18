#pragma once

#include "EditorLocale.h"
#include "IconProvider.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <functional>

class VariablePanelWidget : public QWidget
{
public:
    VariablePanelWidget(QTableWidget* table, const QString& prefix, const QStringList& types,
                        const QString& nameColTitle, const QString& valColTitle,
                        std::function<QString(const QString&)> typeLabel = nullptr,
                        QWidget* parent = nullptr)
        : QWidget(parent), table_(table), prefix_(prefix), types_(types),
          nameColTitle_(nameColTitle), valColTitle_(valColTitle),
          typeLabel_(std::move(typeLabel))
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        auto* bar = new QHBoxLayout;
        bar->setContentsMargins(0, 0, 0, 0);
        bar->setSpacing(4);

        addBtn_ = new QPushButton(IconProvider::PlusIcon(), QString(), this);
        addBtn_->setObjectName(prefix_ + QStringLiteral("AddVariable"));
        addBtn_->setFixedSize(28, 24);
        addBtn_->setToolTip(Tr("Добавить переменную", "Add variable"));
        connect(addBtn_, &QPushButton::clicked, this, [this] { AddVariable(); });
        bar->addWidget(addBtn_);

        dupBtn_ = new QPushButton(IconProvider::CopyIcon(), QString(), this);
        dupBtn_->setObjectName(prefix_ + QStringLiteral("DuplicateVariable"));
        dupBtn_->setFixedSize(28, 24);
        dupBtn_->setToolTip(Tr("Дублировать переменную", "Duplicate variable"));
        connect(dupBtn_, &QPushButton::clicked, this, [this] { DuplicateVariable(); });
        bar->addWidget(dupBtn_);

        removeBtn_ = new QPushButton(IconProvider::TrashIcon(), QString(), this);
        removeBtn_->setObjectName(prefix_ + QStringLiteral("RemoveVariable"));
        removeBtn_->setFixedSize(28, 24);
        removeBtn_->setToolTip(Tr("Удалить переменную", "Remove variable"));
        connect(removeBtn_, &QPushButton::clicked, this, [this] { RemoveVariable(table_->currentRow()); });
        bar->addWidget(removeBtn_);

        bar->addStretch(1);
        layout->addLayout(bar);

        table_->setColumnCount(5);
        table_->setHorizontalHeaderLabels({nameColTitle_, Tr("Тип", "Type"), valColTitle_, QString(), QString()});
        table_->verticalHeader()->hide();
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
        table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
        table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
        table_->setColumnWidth(3, 26);
        table_->setColumnWidth(4, 26);

        layout->addWidget(table_, 1);
    }

    std::function<void(const QJsonArray& list, const QHash<QString, QString>& renames)> onVariablesChanged;

    void SetVariables(const QJsonArray& list)
    {
        refreshing_ = true;
        lastSavedList_ = list;
        table_->setRowCount(0);
        table_->setRowCount(list.size());
        for (int row = 0; row < list.size(); ++row)
        {
            const QJsonObject var = list[row].toObject();
            const QString name = var.value(QStringLiteral("name")).toString();
            const QString type = var.value(QStringLiteral("type")).toString(QStringLiteral("float"));
            const bool isPub = var.value(QStringLiteral("public")).toBool(true);

            auto* nameField = new QLineEdit(name);
            nameField->setObjectName(QStringLiteral("%1VariableName_%2").arg(prefix_).arg(row));
            nameField->setProperty("previous", name);
            connect(nameField, &QLineEdit::editingFinished, this, [this, row] { OnRowEdited(row); });
            table_->setCellWidget(row, 0, nameField);

            auto* combo = new QComboBox;
            combo->setObjectName(QStringLiteral("%1VariableType_%2").arg(prefix_).arg(row));
            for (const QString& t : types_) combo->addItem(typeLabel_ ? typeLabel_(t) : t, t);
            combo->setCurrentIndex(std::max(0, combo->findData(type)));
            connect(combo, &QComboBox::currentIndexChanged, this, [this, row] { OnRowEdited(row); });
            table_->setCellWidget(row, 1, combo);

            auto* valField = new QLineEdit(FormatValue(var.value(QStringLiteral("value")), type));
            valField->setObjectName(QStringLiteral("%1VariableValue_%2").arg(prefix_).arg(row));
            valField->setPlaceholderText(type == QStringLiteral("vec3") ? QStringLiteral("x, y, z") : (prefix_ == QStringLiteral("Fsm") ? Tr("пусто", "empty") : QString()));
            if (type == QStringLiteral("object"))
            {
                valField->setEnabled(false);
                valField->setPlaceholderText(QString());
                valField->setToolTip(Tr("У объекта нет начального значения", "Object has no initial value"));
            }
            connect(valField, &QLineEdit::editingFinished, this, [this, row] { OnRowEdited(row); });
            table_->setCellWidget(row, 2, valField);

            auto* eyeBtn = new QPushButton(IconProvider::EyeIcon(isPub), QString());
            eyeBtn->setObjectName(QStringLiteral("%1VariableEye_%2").arg(prefix_).arg(row));
            eyeBtn->setFixedSize(24, 20);
            eyeBtn->setCheckable(true);
            eyeBtn->setChecked(isPub);
            eyeBtn->setToolTip(isPub ? Tr("Публичная переменная: видна в инспекторе", "Public variable: visible in inspector")
                                     : Tr("Приватная переменная: скрыта из инспектора", "Private variable: hidden in inspector"));
            connect(eyeBtn, &QPushButton::clicked, this, [this, row, eyeBtn](bool checked) {
                eyeBtn->setIcon(IconProvider::EyeIcon(checked));
                eyeBtn->setToolTip(checked ? Tr("Публичная переменная: видна в инспекторе", "Public variable: visible in inspector")
                                           : Tr("Приватная переменная: скрыта из инспектора", "Private variable: hidden in inspector"));
                OnRowEdited(row);
            });
            table_->setCellWidget(row, 3, eyeBtn);

            auto* trashBtn = new QPushButton(IconProvider::TrashIcon(), QString());
            trashBtn->setObjectName(QStringLiteral("%1VariableTrash_%2").arg(prefix_).arg(row));
            trashBtn->setFixedSize(24, 20);
            trashBtn->setToolTip(Tr("Удалить строку", "Delete row"));
            connect(trashBtn, &QPushButton::clicked, this, [this, row] { RemoveVariable(row); });
            table_->setCellWidget(row, 4, trashBtn);
        }
        refreshing_ = false;
    }

    void AddVariable()
    {
        QJsonArray list = CollectVariables();
        QStringList taken;
        for (const auto& v : list) taken << v.toObject().value(QStringLiteral("name")).toString();
        const QString base = prefix_ == QStringLiteral("Fsm") ? QStringLiteral("Key") : QStringLiteral("Var");
        QString name = base;
        for (int i = 2; taken.contains(name); ++i) name = QStringLiteral("%1%2").arg(base).arg(i);
        QJsonObject var{
            {QStringLiteral("name"), name},
            {QStringLiteral("type"), QStringLiteral("float")},
            {QStringLiteral("public"), true}
        };
        if (prefix_ != QStringLiteral("Fsm"))
        {
            var.insert(QStringLiteral("value"), 0);
        }
        list.append(var);
        if (onVariablesChanged) onVariablesChanged(list, {});
    }

    void DuplicateVariable()
    {
        const int row = table_->currentRow();
        if (row < 0 || row >= table_->rowCount()) return;
        QJsonArray list = CollectVariables();
        if (row >= list.size()) return;
        QJsonObject dup = list[row].toObject();
        QString origName = dup.value(QStringLiteral("name")).toString();
        QStringList taken;
        for (const auto& v : list) taken << v.toObject().value(QStringLiteral("name")).toString();
        QString name = origName + QStringLiteral("_Copy");
        for (int i = 2; taken.contains(name); ++i) name = QStringLiteral("%1_Copy%2").arg(origName).arg(i);
        dup[QStringLiteral("name")] = name;
        list.append(dup);
        if (onVariablesChanged) onVariablesChanged(list, {});
    }

    void RemoveVariable(int row)
    {
        if (row < 0 || row >= table_->rowCount()) return;
        QJsonArray list = CollectVariables();
        if (row >= list.size()) return;
        list.removeAt(row);
        if (onVariablesChanged) onVariablesChanged(list, {});
    }

    QJsonArray CollectVariables() const
    {
        QJsonArray list;
        for (int row = 0; row < table_->rowCount(); ++row)
        {
            const auto* nameField = qobject_cast<QLineEdit*>(table_->cellWidget(row, 0));
            const auto* combo = qobject_cast<QComboBox*>(table_->cellWidget(row, 1));
            const auto* valField = qobject_cast<QLineEdit*>(table_->cellWidget(row, 2));
            const auto* eyeBtn = qobject_cast<QPushButton*>(table_->cellWidget(row, 3));
            if (!nameField || !combo) continue;
            const QString name = nameField->text().trimmed();
            const QString type = combo->currentData().isValid() ? combo->currentData().toString() : combo->currentText();
            const bool isPub = eyeBtn ? eyeBtn->isChecked() : true;
            QJsonObject var{
                {QStringLiteral("name"), name},
                {QStringLiteral("type"), type},
                {QStringLiteral("public"), isPub}
            };
            const QJsonValue val = ParseValue(valField ? valField->text() : QString(), type);
            if (!val.isUndefined()) var.insert(QStringLiteral("value"), val);
            list.append(var);
        }
        return list;
    }

    static QString FormatValue(const QJsonValue& value, const QString& type)
    {
        if (value.isUndefined()) return QString();
        if (type == QStringLiteral("bool")) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        if (type == QStringLiteral("vec3"))
        {
            const QJsonArray list = value.toArray();
            QStringList parts;
            for (int i = 0; i < 3; ++i) parts << QString::number(i < list.size() ? list[i].toDouble() : 0.0, 'g', 6);
            return parts.join(QStringLiteral(", "));
        }
        if (type == QStringLiteral("float")) return QString::number(value.toDouble(), 'g', 6);
        if (type == QStringLiteral("int")) return QString::number(value.toInt());
        return value.toString();
    }

    QJsonValue ParseValue(const QString& text, const QString& type) const
    {
        const QString trimmed = text.trimmed();
        if (prefix_ == QStringLiteral("Fsm") && trimmed.isEmpty()) return QJsonValue(QJsonValue::Undefined);
        if (type == QStringLiteral("bool"))
            return QStringList{QStringLiteral("true"), QStringLiteral("1"), QStringLiteral("yes")}.contains(trimmed.toLower());
        if (type == QStringLiteral("vec3"))
        {
            const QStringList parts = trimmed.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
            QJsonArray list;
            for (int i = 0; i < 3; ++i) list.append(i < parts.size() ? parts[i].toDouble() : 0.0);
            return list;
        }
        if (type == QStringLiteral("float")) return QString(trimmed).replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
        if (type == QStringLiteral("int")) return trimmed.toInt();
        return text;
    }

private:
    void OnRowEdited(int row)
    {
        if (refreshing_) return;
        QHash<QString, QString> renames;
        if (const auto* nameField = qobject_cast<QLineEdit*>(table_->cellWidget(row, 0)))
        {
            const QString name = nameField->text().trimmed();
            const QString old = nameField->property("previous").toString();
            if (!old.isEmpty() && old != name) renames.insert(old, name);
        }
        const QJsonArray list = CollectVariables();
        if (renames.isEmpty() && list == lastSavedList_) return;
        QTimer::singleShot(0, this, [this, list, renames] {
            if (onVariablesChanged) onVariablesChanged(list, renames);
        });
    }

    QTableWidget* table_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    QPushButton* dupBtn_ = nullptr;
    QPushButton* removeBtn_ = nullptr;
    QString prefix_;
    QStringList types_;
    QString nameColTitle_;
    QString valColTitle_;
    std::function<QString(const QString&)> typeLabel_;
    QJsonArray lastSavedList_;
    bool refreshing_ = false;
};
