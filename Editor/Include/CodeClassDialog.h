#pragma once
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

// No engine objects are modified until the SDK accepts the complete request.
class CodeClassDialog final : public QDialog
{
public:
    explicit CodeClassDialog(const QString& projectRoot, QWidget* parent = nullptr) : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("Добавить C++ класс"));
        setMinimumWidth(560);
        auto* layout = new QVBoxLayout(this);
        auto* form = new QFormLayout;
        kind_ = new QComboBox(this); kind_->setObjectName("ClassKind");
        kind_->addItems({"Component", "Character", "Game", "Object"});
        name_ = new QLineEdit(this); name_->setObjectName("ClassName");
        name_->setPlaceholderText(QStringLiteral("Например: MyCharacter (без .h и .cpp)"));
        base_ = new QLineEdit(this); base_->setObjectName("ClassBase");
        header_ = new QLineEdit(this); header_->setObjectName("ClassBaseHeader");
        form->addRow(QStringLiteral("Назначение"), kind_);
        form->addRow(QStringLiteral("Имя класса"), name_);
        form->addRow(QStringLiteral("Родитель"), base_);
        form->addRow(QStringLiteral("Заголовок родителя"), header_);
        layout->addLayout(form);
        auto* hint = new QLabel(QStringLiteral("Имя: латинские буквы, цифры и _. Первая буква — не цифра.\n"
            "Для Object родитель необязателен. Заголовок — относительно Source или SDK Include."), this);
        hint->setWordWrap(true); layout->addWidget(hint);
        auto* error = new QLabel(this); error->setObjectName("ClassError"); error->setWordWrap(true);
        layout->addWidget(error);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Создать")); layout->addWidget(buttons);
        auto validate = [=] {
            const auto name = name_->text().trimmed(), base = base_->text().trimmed(), header = header_->text().trimmed();
            QString message;
            if (!QRegularExpression("^[A-Za-z_][A-Za-z_0-9]*$").match(name).hasMatch())
                message = QStringLiteral("Введите имя вроде MyCharacter: без пробелов, кириллицы и расширения файла.");
            else if (ReservedNames().contains(name, Qt::CaseSensitive) || QRegularExpression("^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])$", QRegularExpression::CaseInsensitiveOption).match(name).hasMatch())
                message = QStringLiteral("Это имя зарезервировано. Выберите другое имя класса.");
            else if (name == base)
                message = QStringLiteral("Имя нового класса должно отличаться от имени родителя.");
            else if (QFileInfo::exists(projectRoot + "/Source/" + name + ".h") || QFileInfo::exists(projectRoot + "/Source/" + name + ".cpp"))
                message = QStringLiteral("Файлы этого класса уже существуют. Выберите другое имя.");
            else if ((!base.isEmpty() && !QRegularExpression("^[A-Za-z_][A-Za-z_0-9]*(::[A-Za-z_][A-Za-z_0-9]*)*$").match(base).hasMatch()) || (base.isEmpty() && kind_->currentText() != "Object"))
                message = QStringLiteral("Укажите имя родительского C++ класса.");
            else if ((!base.isEmpty() && header.isEmpty()) || header.contains("..") || header.contains(QRegularExpression("[\"<>\\r\\n]")))
                message = QStringLiteral("Укажите заголовок родителя без кавычек, <>, переноса строки и ..");
            error->setText(message); buttons->button(QDialogButtonBox::Ok)->setEnabled(message.isEmpty());
        };
        connect(kind_, &QComboBox::currentTextChanged, this, [=](const QString& kind) {
            base_->setText(kind == "Character" ? "Character" : kind == "Component" ? "GameComponent" : kind == "Game" ? "Game" : "");
            header_->setText(kind == "Character" ? "Engine/Components/Character.h" : kind == "Component" ? "Engine/Components/GameComponents.h" : kind == "Game" ? "Engine/MainGame/BaseGameClass/Game.h" : "Engine/Components/ProjectObject.h");
            validate();
        });
        for (auto* edit : {name_, base_, header_}) connect(edit, &QLineEdit::textChanged, this, validate);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        base_->setText("GameComponent"); header_->setText("Engine/Components/GameComponents.h"); validate();
        name_->setFocus();
    }

    QStringList Arguments() const
    {
        QStringList result{"-Name", name_->text().trimmed(), "-Kind", kind_->currentText()};
        if (!base_->text().trimmed().isEmpty()) result << "-Base" << base_->text().trimmed();
        if (!header_->text().trimmed().isEmpty()) result << "-BaseHeader" << header_->text().trimmed();
        return result;
    }

    static QStringList ReservedNames()
    {
        return QStringLiteral("alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq Game GameComponent Character SceneCamera PlaySettings Module").split(' ');
    }
private:
    QComboBox* kind_{};
    QLineEdit *name_{}, *base_{}, *header_{};
};
