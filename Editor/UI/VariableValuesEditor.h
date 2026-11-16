#pragma once

#include "EditorLocale.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QRegularExpression>
#include <QToolButton>
#include <QWidget>
#include <functional>

// Значения переменных у одного объекта поверх значений из файла (скрипта, автомата ИИ).
// Пустое поле — значение из файла (оно в подсказке), заполненное — своё, ↺ возвращает значение файла.
// Поддерживаются bool, int, float, vec3, string; переменные других типов (object) не показываются.
class VariableValuesEditor : public QWidget
{
public:
    // Подписи с источником значения: «как в скрипте (%1)», «Пусто — значение из скрипта: %1», «Вернуть значение из скрипта».
    struct Texts
    {
        const char* asInRu; const char* asInEn; const char* emptyRu; const char* emptyEn; const char* resetRu; const char* resetEn;
    };

    // namePrefix / resetPrefix — objectName полей и кнопок сброса, к ним добавляется имя переменной.
    VariableValuesEditor(QString namePrefix, QString resetPrefix, Texts texts, QWidget* parent = nullptr)
        : QWidget(parent), namePrefix_(std::move(namePrefix)), resetPrefix_(std::move(resetPrefix)), texts_(std::move(texts))
    {
        form_ = new QFormLayout(this);
        form_->setContentsMargins(0, 0, 0, 0);
        form_->setSpacing(3);
    }

    // declared — [{"name","type","value"}] из файла; values — JSON-объект значений объекта («» — все как в файле).
    // Возвращает true, если есть что показать. Поля не перестраиваются, пока данные те же: в них могут печатать.
    bool Set(const QJsonArray& declared, const QString& values, bool editable)
    {
        const QString key = QString::fromUtf8(QJsonDocument(declared).toJson(QJsonDocument::Compact)) + QChar(1) + values + QChar(1) +
                            (editable ? QStringLiteral("1") : QStringLiteral("0"));
        if (key == shownKey_) return !rows_.isEmpty();
        shownKey_ = key;
        values_ = QJsonDocument::fromJson(values.toUtf8()).object();
        while (form_->rowCount() > 0) form_->removeRow(0);
        rows_.clear();
        for (const auto& item : declared)
        {
            const QJsonObject variable = item.toObject();
            const QString name = variable.value(QStringLiteral("name")).toString();
            const QString type = variable.value(QStringLiteral("type")).toString(QStringLiteral("float"));
            if (name.isEmpty() || !Supported(type)) continue;
            AddRow(name, type, variable.value(QStringLiteral("value")), editable);
        }
        return !rows_.isEmpty();
    }

    static bool Supported(const QString& type)
    {
        return type == QStringLiteral("bool") || type == QStringLiteral("int") || type == QStringLiteral("float") ||
               type == QStringLiteral("vec3") || type == QStringLiteral("string");
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
        if (type == QStringLiteral("int"))
        {
            bool ok = false;
            const int number = trimmed.toInt(&ok);
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

    // Новые значения объекта (JSON-объект; пустая строка — все как в файле).
    std::function<void(const QString&)> onChanged;

private:
    void AddRow(const QString& name, const QString& type, const QJsonValue& fallback, bool editable)
    {
        auto* row = new QWidget(this);
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(0, 0, 0, 0);
        line->setSpacing(3);
        const bool overridden = values_.contains(name);
        QWidget* editor = nullptr;
        if (type == QStringLiteral("bool"))
        {
            auto* combo = new QComboBox(row);
            combo->addItem(Tr(texts_.asInRu, texts_.asInEn).arg(ValueText(fallback)), QString());
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
            field->setToolTip(Tr(texts_.emptyRu, texts_.emptyEn).arg(ValueText(fallback)));
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
        editor->setObjectName(namePrefix_ + name);
        editor->setEnabled(editable);
        line->addWidget(editor, 1);
        auto* reset = new QToolButton(row);
        reset->setObjectName(resetPrefix_ + name);
        reset->setText(QString(QChar(0x21BA)));
        reset->setToolTip(Tr(texts_.resetRu, texts_.resetEn));
        reset->setVisible(overridden);
        reset->setEnabled(editable);
        QObject::connect(reset, &QToolButton::clicked, row, [this, name] { Commit(name, QJsonValue(QJsonValue::Undefined)); });
        line->addWidget(reset);
        form_->addRow(name, row);
        rows_ << name;
    }

    void Commit(const QString& name, const QJsonValue& value)
    {
        QJsonObject next = values_;
        if (value.isUndefined()) next.remove(name);
        else next.insert(name, value);
        if (next == values_) return;
        values_ = next;
        if (onChanged) onChanged(next.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(next).toJson(QJsonDocument::Compact)));
    }

    QString namePrefix_, resetPrefix_;
    Texts texts_;
    QFormLayout* form_ = nullptr;
    QStringList rows_;
    QJsonObject values_;
    QString shownKey_;
};
