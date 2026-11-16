#pragma once

#include "CollapsibleCard.h"
#include "EditorLocale.h"
#include "VariableValuesEditor.h"

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
        variablesBox_ = new VariableValuesEditor(QStringLiteral("ScriptVariable_"), QStringLiteral("ScriptVariableReset_"),
            {"как в скрипте (%1)", "as in the script (%1)", "Пусто — значение из скрипта: %1", "Empty — the script's value: %1",
             "Вернуть значение из скрипта", "Use the script's value"}, Content());
        variablesBox_->setObjectName(QStringLiteral("ScriptVariableValues"));
        variablesBox_->onChanged = [this](const QString& json) { if (onVariablesChanged) onVariablesChanged(json); };
        layout->addWidget(variablesBox_);
        variablesLabel_->hide();
        variablesBox_->hide();
        RetranslateUi();
    }

    // script — /Game путь; values — JSON-объект значений этого объекта («» — все как в скрипте).
    void SetVariables(const QString& script, const QString& values, bool editable)
    {
        const QJsonArray declared = readVariables && !script.isEmpty() ? readVariables(script) : QJsonArray();
        const bool shown = variablesBox_->Set(declared, values, editable);
        variablesLabel_->setVisible(shown);
        variablesBox_->setVisible(shown);
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

    QLabel* variablesLabel_ = nullptr;
    VariableValuesEditor* variablesBox_ = nullptr;
    QLabel* label_ = nullptr;
    QLineEdit* asset_ = nullptr;
    QPushButton* open_ = nullptr;
    QPushButton* choose_ = nullptr;
    QPushButton* clear_ = nullptr;
};
