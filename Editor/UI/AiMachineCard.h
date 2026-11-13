#pragma once

#include "CollapsibleCard.h"
#include "EditorLocale.h"
#include "VariableValuesEditor.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <functional>

// Автомат ИИ выделенного «ИИ-персонажа»: свойство stateMachine (путь к .fsm или имя автомата из C++)
// и значения памяти этого объекта (свойство machineMemory) поверх значений из файла.
class AiMachineCard : public CollapsibleCard
{
public:
    explicit AiMachineCard(QWidget* parent = nullptr)
        : CollapsibleCard(Tr("Автомат ИИ", "AI State Machine"), parent, true)
    {
        auto* layout = ContentLayout();
        label_ = new QLabel(Content());
        label_->setObjectName(QStringLiteral("CardSectionLabel"));
        layout->addWidget(label_);

        machine_ = new QLineEdit(Content());
        machine_->setObjectName(QStringLiteral("AiMachine"));
        machine_->setFixedHeight(22);
        machine_->setMaxLength(4096);
        QObject::connect(machine_, &QLineEdit::editingFinished, machine_, [this] {
            const QString text = machine_->text().trimmed();
            if (text != shown_ && onSetMachine) onSetMachine(text);
        });
        layout->addWidget(machine_);

        status_ = new QLabel(Content());
        status_->setObjectName(QStringLiteral("AiMachineStatus"));
        status_->setWordWrap(true);
        status_->hide();
        layout->addWidget(status_);

        auto* actions = new QWidget(Content());
        auto* row = new QHBoxLayout(actions);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        open_ = Button(actions, row, "AiMachineOpen", [this] {
            if (shown_.isEmpty()) { if (onCreate) onCreate(); }
            else if (onOpen) onOpen(shown_);
        });
        choose_ = Button(actions, row, "AiMachineChoose", [this] { if (onChoose) onChoose(); });
        clear_ = Button(actions, row, "AiMachineClear", [this] { if (onSetMachine) onSetMachine(QString()); });
        layout->addWidget(actions);

        memoryLabel_ = new QLabel(Content());
        memoryLabel_->setObjectName(QStringLiteral("CardSectionLabel"));
        layout->addWidget(memoryLabel_);
        memory_ = new VariableValuesEditor(QStringLiteral("AiMemory_"), QStringLiteral("AiMemoryReset_"),
            {"как в автомате (%1)", "as in the machine (%1)", "Пусто — значение из автомата: %1", "Empty — the machine's value: %1",
             "Вернуть значение из автомата", "Use the machine's value"}, Content());
        memory_->setObjectName(QStringLiteral("AiMemoryValues"));
        memory_->onChanged = [this](const QString& json) { if (onMemoryChanged) onMemoryChanged(json); };
        layout->addWidget(memory_);
        memoryLabel_->hide();
        memory_->hide();
        RetranslateUi();
    }

    static bool IsFile(const QString& machine) { return machine.endsWith(QStringLiteral(".fsm"), Qt::CaseInsensitive); }

    // machine — значение свойства stateMachine; memory — machineMemory («» — все как в файле).
    void SetData(const QString& machine, const QString& memory, bool editable)
    {
        shown_ = machine;
        memoryShown_ = memory;
        if (!machine_->hasFocus()) machine_->setText(machine);
        machine_->setEnabled(editable);
        const bool file = IsFile(machine);
        const bool exists = file && fileExists && fileExists(machine);
        open_->setText(machine.isEmpty() ? Tr("Создать", "Create") : Tr("Открыть", "Open"));
        open_->setToolTip(machine.isEmpty() ? Tr("Создать автомат в Content/AI, назначить его персонажу", "Create a machine in Content/AI and assign it to the character")
                                            : QString());
        open_->setEnabled(machine.isEmpty() ? editable : exists);
        choose_->setEnabled(editable);
        clear_->setEnabled(editable && !machine.isEmpty());

        QString status;
        if (file && !machine.startsWith(QStringLiteral("/Game/"))) status = Tr("Путь к файлу должен начинаться с /Game/.", "The file path must start with /Game/.");
        else if (file && !exists) status = Tr("Файл не найден: переименовали или удалили?", "File not found: renamed or deleted?");
        else if (!machine.isEmpty() && !file) status = Tr("Автомат из C++: значения памяти задаёт код.", "A machine from C++: its code sets the memory.");
        status_->setText(status);
        status_->setVisible(!status.isEmpty());

        const QJsonArray declared = exists && readVariables ? readVariables(machine) : QJsonArray();
        const bool shown = memory_->Set(declared, memory, editable);
        memoryLabel_->setVisible(shown);
        memory_->setVisible(shown);
    }

    void RetranslateUi()
    {
        SetTitle(Tr("Автомат ИИ", "AI State Machine"));
        label_->setText(Tr("Файл .fsm или имя автомата из C++", "A .fsm file or the name of a C++ machine"));
        machine_->setPlaceholderText(Tr("нет", "none"));
        choose_->setText(Tr("Выбрать…", "Browse…"));
        clear_->setText(Tr("Убрать", "Clear"));
        memoryLabel_->setText(Tr("Память у этого объекта", "Memory of this object"));
        SetData(shown_, memoryShown_, choose_->isEnabled());
    }

    std::function<void(const QString&)> onSetMachine; // новое значение stateMachine; пустое — убрать
    std::function<void()> onCreate;
    std::function<void(const QString&)> onOpen;
    std::function<void()> onChoose;
    std::function<void(const QString&)> onMemoryChanged; // JSON-объект; пустая строка — все как в файле
    std::function<QJsonArray(const QString&)> readVariables; // переменные файла по /Game пути: [{"name","type","value"}]
    std::function<bool(const QString&)> fileExists;

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

    QString shown_, memoryShown_;
    QLabel* label_ = nullptr;
    QLineEdit* machine_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* open_ = nullptr;
    QPushButton* choose_ = nullptr;
    QPushButton* clear_ = nullptr;
    QLabel* memoryLabel_ = nullptr;
    VariableValuesEditor* memory_ = nullptr;
};
