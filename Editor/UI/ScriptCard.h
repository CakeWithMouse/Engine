#pragma once

#include "CollapsibleCard.h"
#include "EditorLocale.h"

#include <QHBoxLayout>
#include <QLabel>
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
        RetranslateUi();
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
        SetData(asset_->text(), choose_->isEnabled());
    }

    std::function<void(const QString&)> onOpenScript; // пустой путь: создать
    std::function<void()> onChooseScript;
    std::function<void()> onClearScript;

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

    QLabel* label_ = nullptr;
    QLineEdit* asset_ = nullptr;
    QPushButton* open_ = nullptr;
    QPushButton* choose_ = nullptr;
    QPushButton* clear_ = nullptr;
};
