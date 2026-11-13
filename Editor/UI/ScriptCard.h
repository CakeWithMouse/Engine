#pragma once

#include "CollapsibleCard.h"
#include "EditorLocale.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QRegularExpression>
#include <QToolButton>
#include <QLineEdit>
#include <QPushButton>
#include <functional>

// Визуальный скрипт выделенного объекта: путь .vscript, открыть (или создать и назначить), выбрать, убрать.
class ScriptCard : public CollapsibleCard
{
public:
    explicit ScriptCard(QWidget* parent = nullptr)
        : CollapsibleCard(Tr("Скрипт", "Script"), parent, true)
    {
        auto* layout = ContentLayout();
        label_ = new QLabel(Tr("Визуальный скрипт (.vscript)", "Visual script (.vscript)"), Content());
        label_->setObjectName(QStringLiteral("CardSectionLabel"));
        layout->addWidget(label_);

        asset_ = new QLineEdit(Content());
        asset_->setReadOnly(true);
        asset_->setObjectName(QStringLiteral("ScriptAsset"));
        asset_->setFixedHeight(22);
        asset_->setPlaceholderText(Tr("нет", "none"));
        layout->addWidget(asset_);

        auto* actions = new QWidget(Content());
        auto* row = new QHBoxLayout(actions);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        open_ = Button(actions, row, "OpenScript", [this] { if (onOpenScript) onOpenScript(asset_->text()); });
        choose_ = Button(actions, row, "ChooseScript", [this] { if (onChooseScript) onChooseScript(); });
        clear_ = Button(actions, row, "ClearScript", [this] { if (onClearScript) onClearScript(); });
        layout->addWidget(actions);

        // Значения переменных скрипта для этого объекта: пустое поле — значение из скрипта (в подсказке).
        variablesLabel_ = new QLabel(Content());
        variablesLabel_->setObjectName(QStringLiteral("CardSectionLabel"));
        layout->addWidget(variablesLabel_);
        variablesBox_ = new QWidget(Content());
        variablesBox_->setObjectName(QStringLiteral("ScriptVariableValues"));
        variablesForm_ = new QFormLayout(variablesBox_);
        variablesForm_->setContentsMargins(0, 0, 0, 0);
        variablesForm_->setSpacing(3);
        layout->addWidget(variablesBox_);
        variablesLabel_->hide();
        variablesBox_->hide();
        RetranslateUi();
    }

    // script — /Game путь; values — JSON-объект значений этого объекта («» — все как в скрипте).
    void SetVariables(const QString& script, const QString& values, bool editable)
    {
        const QJsonArray declared = readVariables && !script.isEmpty() ? readVariables(script) : QJsonArray();
        const QString key = script + QChar(1) + QString::fromUtf8(QJsonDocument(declared).toJson(QJsonDocument::Compact)) + QChar(1) + values +
                            QChar(1) + (editable ? QStringLiteral("1") : QStringLiteral("0"));
        if (key == shownKey_) return; // не перестраивать поля, пока в них печатают
        shownKey_ = key;
        values_ = QJsonDocument::fromJson(values.toUtf8()).object();
        while (variablesForm_->rowCount() > 0) variablesForm_->removeRow(0);
        rows_.clear();
        for (const auto& item : declared)
        {
            const QJsonObject variable = item.toObject();
            const QString name = variable.value(QStringLiteral("name")).toString();
            const QString type = variable.value(QStringLiteral("type")).toString(QStringLiteral("float"));
            if (name.isEmpty()) continue;
            AddRow(name, type, variable.value(QStringLiteral("value")), editable);
        }
        variablesLabel_->setVisible(!rows_.isEmpty());
        variablesBox_->setVisible(!rows_.isEmpty());
    }

    // Текст значения в поле: число, «x, y, z», строка, true/false.
    static QString ValueText(const QJsonValue& value)
    {
        if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        if (value.isDouble()) return QString::number(value.toDouble());
        if (value.isArray())
        {
            QStringList parts;
            for (const auto& part : value.toArray()) parts << QString::number(part.toDouble());
            return parts.join(QStringLiteral(", "));
        }
        return value.toString();
    }

    // Значение из текста поля по типу переменной; Undefined — текст не подходит.
    static QJsonValue ParseValue(const QString& text, const QString& type)
    {
        const QString trimmed = text.trimmed();
        if (type == QStringLiteral("string")) return text;
        if (type == QStringLiteral("float"))
        {
            bool ok = false;
            const double number = QString(trimmed).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
            return ok ? QJsonValue(number) : QJsonValue(QJsonValue::Undefined);
        }
        if (type == QStringLiteral("vec3"))
        {
            const QStringList parts = trimmed.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
            if (parts.size() != 3) return QJsonValue(QJsonValue::Undefined);
            QJsonArray vector;
            for (const auto& part : parts)
            {
                bool ok = false;
                const double number = part.toDouble(&ok);
                if (!ok) return QJsonValue(QJsonValue::Undefined);
                vector.append(number);
            }
            return vector;
        }
        return QJsonValue(QJsonValue::Undefined);
    }

    void SetData(const QString& script, bool editable)
    {
        asset_->setText(script);
        open_->setText(script.isEmpty() ? Tr("Создать", "Create") : Tr("Открыть", "Open"));
        open_->setToolTip(script.isEmpty() ? Tr("Создать скрипт в Content/Scripts, назначить его объекту и открыть",
                                                "Create a script in Content/Scripts, assign it to the object and open it")
                                           : QString());
        open_->setEnabled(!script.isEmpty() || editable);
        choose_->setEnabled(editable);
        clear_->setEnabled(editable && !script.isEmpty());
    }

    void RetranslateUi()
    {
        SetTitle(Tr("Скрипт", "Script"));
        label_->setText(Tr("Визуальный скрипт (.vscript)", "Visual script (.vscript)"));
        asset_->setPlaceholderText(Tr("нет", "none"));
        choose_->setText(Tr("Выбрать…", "Browse…"));
        clear_->setText(Tr("Убрать", "Clear"));
        variablesLabel_->setText(Tr("Переменные у этого объекта", "Variables of this object"));
        SetData(asset_->text(), choose_->isEnabled());
    }

    std::function<void(const QString&)> onOpenScript; // пустой путь: создать
    std::function<void()> onChooseScript;
    std::function<void()> onClearScript;
    // Новые значения переменных объекта (JSON-объект; пустая строка — все как в скрипте).
    std::function<void(const QString&)> onVariablesChanged;
    // Список переменных скрипта по /Game пути: [{"name","type","value"}].
    std::function<QJsonArray(const QString&)> readVariables;

private:
    template <class F>
    static QPushButton* Button(QWidget* parent, QHBoxLayout* row, const char* name, F&& clicked)
    {
        auto* button = new QPushButton(parent);
        button->setObjectName(QString::fromLatin1(name));
        button->setFixedHeight(24);
        QObject::connect(button, &QPushButton::clicked, std::forward<F>(clicked));
        row->addWidget(button);
        return button;
    }

    void AddRow(const QString& name, const QString& type, const QJsonValue& fallback, bool editable)
    {
        auto* row = new QWidget(variablesBox_);
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(0, 0, 0, 0);
        line->setSpacing(3);
        const bool overridden = values_.contains(name);
        QWidget* editor = nullptr;
        if (type == QStringLiteral("bool"))
        {
            auto* combo = new QComboBox(row);
            combo->addItem(Tr("как в скрипте (%1)", "as in the script (%1)").arg(ValueText(fallback)), QString());
            combo->addItem(QStringLiteral("true"), QStringLiteral("true"));
            combo->addItem(QStringLiteral("false"), QStringLiteral("false"));
            combo->setCurrentIndex(overridden ? (values_.value(name).toBool() ? 1 : 2) : 0);
            QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), row, [this, name, combo](int) {
                const QString value = combo->currentData().toString();
                Commit(name, value.isEmpty() ? QJsonValue(QJsonValue::Undefined) : QJsonValue(value == QStringLiteral("true")));
            });
            editor = combo;
        }
        else
        {
            auto* field = new QLineEdit(row);
            field->setFixedHeight(22);
            field->setPlaceholderText(ValueText(fallback));
            field->setText(overridden ? ValueText(values_.value(name)) : QString());
            field->setToolTip(Tr("Пусто — значение из скрипта: %1", "Empty — the script's value: %1").arg(ValueText(fallback)));
            QObject::connect(field, &QLineEdit::editingFinished, row, [this, name, type, field] {
                if (field->text().isEmpty() || (type != QStringLiteral("string") && field->text().trimmed().isEmpty()))
                    return Commit(name, QJsonValue(QJsonValue::Undefined));
                const QJsonValue value = ParseValue(field->text(), type);
                if (value.isUndefined())
                {
                    field->setText(values_.contains(name) ? ValueText(values_.value(name)) : QString());
                    return;
                }
                Commit(name, value);
            });
            editor = field;
        }
        editor->setObjectName(QStringLiteral("ScriptVariable_") + name);
        editor->setEnabled(editable);
        line->addWidget(editor, 1);
        auto* reset = new QToolButton(row);
        reset->setObjectName(QStringLiteral("ScriptVariableReset_") + name);
        reset->setText(QString(QChar(0x21BA)));
        reset->setToolTip(Tr("Вернуть значение из скрипта", "Use the script's value"));
        reset->setVisible(overridden);
        reset->setEnabled(editable);
        QObject::connect(reset, &QToolButton::clicked, row, [this, name] { Commit(name, QJsonValue(QJsonValue::Undefined)); });
        line->addWidget(reset);
        variablesForm_->addRow(name, row);
        rows_ << name;
    }

    void Commit(const QString& name, const QJsonValue& value)
    {
        QJsonObject next = values_;
        if (value.isUndefined()) next.remove(name);
        else next.insert(name, value);
        if (next == values_) return;
        values_ = next;
        if (onVariablesChanged)
            onVariablesChanged(next.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(next).toJson(QJsonDocument::Compact)));
    }

    QLabel* variablesLabel_ = nullptr;
    QWidget* variablesBox_ = nullptr;
    QFormLayout* variablesForm_ = nullptr;
    QStringList rows_;
    QJsonObject values_;
    QString shownKey_;
    QLabel* label_ = nullptr;
    QLineEdit* asset_ = nullptr;
    QPushButton* open_ = nullptr;
    QPushButton* choose_ = nullptr;
    QPushButton* clear_ = nullptr;
};
