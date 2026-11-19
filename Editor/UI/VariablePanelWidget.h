#pragma once

#include "EditorLocale.h"
#include "EditorWindowSetup.h"
#include "IconProvider.h"
#include "InfoButton.h"

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
                        const QString& panelTitle = QString(), QWidget* parent = nullptr)
        : QWidget(parent), table_(table), prefix_(prefix), types_(types),
          nameColTitle_(nameColTitle), valColTitle_(valColTitle),
          typeLabel_(std::move(typeLabel))
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        const QColor iconColor = EditorTheme::Instance().Palette().textSecondary;
        addBtn_ = new QPushButton(IconProvider::PlusIcon(iconColor), QString(), this);
        addBtn_->setObjectName(prefix_ + QStringLiteral("AddVariable"));
        addBtn_->setProperty("panelBtn", true);
        addBtn_->setFixedSize(EditorWindowSetup::kPanelBtnWidth, EditorWindowSetup::kPanelBtnHeight);
        addBtn_->setIconSize(QSize(EditorWindowSetup::kPanelIconSize, EditorWindowSetup::kPanelIconSize));
        addBtn_->setToolTip(Tr("Добавить переменную", "Add variable"));
        connect(addBtn_, &QPushButton::clicked, this, [this] { AddVariable(); });

        dupBtn_ = new QPushButton(IconProvider::CopyIcon(iconColor), QString(), this);
        dupBtn_->setObjectName(prefix_ + QStringLiteral("DuplicateVariable"));
        dupBtn_->setProperty("panelBtn", true);
        dupBtn_->setFixedSize(EditorWindowSetup::kPanelBtnWidth, EditorWindowSetup::kPanelBtnHeight);
        dupBtn_->setIconSize(QSize(EditorWindowSetup::kPanelIconSize, EditorWindowSetup::kPanelIconSize));
        dupBtn_->setToolTip(Tr("Дублировать переменную", "Duplicate variable"));
        connect(dupBtn_, &QPushButton::clicked, this, [this] { DuplicateVariable(); });

        removeBtn_ = new QPushButton(IconProvider::TrashIcon(iconColor), QString(), this);
        removeBtn_->setObjectName(prefix_ + QStringLiteral("RemoveVariable"));
        removeBtn_->setProperty("panelBtn", true);
        removeBtn_->setFixedSize(EditorWindowSetup::kPanelBtnWidth, EditorWindowSetup::kPanelBtnHeight);
        removeBtn_->setIconSize(QSize(EditorWindowSetup::kPanelIconSize, EditorWindowSetup::kPanelIconSize));
        removeBtn_->setToolTip(Tr("Удалить переменную", "Remove variable"));
        connect(removeBtn_, &QPushButton::clicked, this, [this] { RemoveVariable(table_->currentRow()); });

        if (!panelTitle.isEmpty())
        {
            auto* headerLabel = EditorWindowSetup::MakePanelHeader(panelTitle, this);
            layout->addWidget(headerLabel);

            auto* btnRow = new QWidget(this);
            auto* btnBox = new QHBoxLayout(btnRow);
            btnBox->setContentsMargins(4, 3, 4, 3);
            btnBox->setSpacing(4);
            btnBox->addWidget(addBtn_);
            btnBox->addWidget(dupBtn_);
            btnBox->addWidget(removeBtn_);
            btnBox->addStretch(1);
            layout->addWidget(btnRow);
        }

        auto* bodyWidget = new QWidget(this);
        auto* bodyLayout = new QVBoxLayout(bodyWidget);
        const int m = panelTitle.isEmpty() ? 0 : 10;
        bodyLayout->setContentsMargins(m, 0, m, m);
        bodyLayout->setSpacing(8);
        bodyLayout->addWidget(table_, 1);
        layout->addWidget(bodyWidget, 1);

        table_->setColumnCount(4);
        table_->setHorizontalHeaderLabels({nameColTitle_, Tr("Тип", "Type"), valColTitle_, Tr("Показ.", "Show")});
        if (auto* item = table_->horizontalHeaderItem(3))
            item->setToolTip(Tr("Публичная — видна в инспекторе", "Public — visible in inspector"));
        table_->verticalHeader()->hide();
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->horizontalHeader()->setMinimumSectionSize(40);
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);

        QFontMetrics fm = table_->fontMetrics();
        const QString showTitle = Tr("Показ.", "Show");
        const int headerTextWidth = table_->horizontalHeader()->fontMetrics().horizontalAdvance(showTitle);
        const int showColWidth = std::max(headerTextWidth + 12, 34);

        int maxTypeWidth = fm.horizontalAdvance(Tr("Тип", "Type")) + 24;
        for (const QString& t : types_)
        {
            const QString label = typeLabel_ ? typeLabel_(t) : t;
            const int w = fm.horizontalAdvance(label) + 32;
            if (w > maxTypeWidth) maxTypeWidth = w;
        }
        table_->setColumnWidth(1, maxTypeWidth);
        table_->horizontalHeader()->resizeSection(1, maxTypeWidth);
        table_->setColumnWidth(3, showColWidth);
        table_->horizontalHeader()->resizeSection(3, showColWidth);

        bodyLayout->addWidget(table_, 1);
        layout->addWidget(bodyWidget, 1);
    }

    QPushButton* AddButton() const { return addBtn_; }
    QPushButton* DuplicateButton() const { return dupBtn_; }
    QPushButton* RemoveButton() const { return removeBtn_; }
    QList<QPushButton*> ToolbarButtons() const { return {addBtn_, dupBtn_, removeBtn_}; }

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

            auto* actions = new QWidget;
            auto* actionsLayout = new QHBoxLayout(actions);
            actionsLayout->setContentsMargins(2, 0, 2, 0);
            actionsLayout->setSpacing(2);

            actionsLayout->setAlignment(Qt::AlignCenter);

            auto* eyeBtn = new QPushButton(IconProvider::EyeIcon(isPub), QString(), actions);
            eyeBtn->setObjectName(QStringLiteral("%1VariableEye_%2").arg(prefix_).arg(row));
            eyeBtn->setFixedSize(24, 20);
            eyeBtn->setIconSize(QSize(14, 14));
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
            actionsLayout->addWidget(eyeBtn);
            table_->setCellWidget(row, 3, actions);
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
            const auto* eyeBtn = table_->cellWidget(row, 3)
                ? table_->cellWidget(row, 3)->findChild<QPushButton*>(QStringLiteral("%1VariableEye_%2").arg(prefix_).arg(row))
                : nullptr;
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
