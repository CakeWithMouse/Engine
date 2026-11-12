#include "ScriptMessages.h"
#include "EditorLocale.h"

#include <QRegularExpression>
#include <QVector>

namespace
{
// Имена типов из ScriptCatalog (TypeName).
QString TypeRu(const QString& type)
{
    if (type == QStringLiteral("exec")) return QStringLiteral("поток выполнения");
    if (type == QStringLiteral("bool")) return QStringLiteral("логическое");
    if (type == QStringLiteral("float")) return QStringLiteral("число");
    if (type == QStringLiteral("vec3")) return QStringLiteral("вектор");
    if (type == QStringLiteral("string")) return QStringLiteral("строка");
    if (type == QStringLiteral("object")) return QStringLiteral("объект");
    if (type == QStringLiteral("any")) return QStringLiteral("любое");
    if (type == QStringLiteral("variable")) return QStringLiteral("переменная");
    return type;
}

struct Rule
{
    QRegularExpression pattern;
    QString russian;  // %1, %2… — группы шаблона
    QVector<int> types; // номера групп с именем типа
};

// Шаблоны повторяют тексты Engine/Source/Script (ScriptProgram.cpp, ScriptInstance.cpp)
// и Game::GetScriptProblems. Новое сообщение движка без шаблона показывается по-английски.
const QVector<Rule>& Rules()
{
    static const QVector<Rule> rules = [] {
        QVector<Rule> list;
        auto add = [&](const char* pattern, const char* russian, QVector<int> types = {}) {
            list.push_back({QRegularExpression(QStringLiteral("^") + QString::fromUtf8(pattern) + QStringLiteral("$")),
                            QString::fromUtf8(russian), std::move(types)});
        };
        // Чтение файла и заголовок.
        add("cannot read the file", "не удаётся прочитать файл: возможно, его переименовали или удалили");
        add("invalid JSON: (.*)", "файл повреждён, ошибка JSON: %1");
        add("the root must be a JSON object", "файл повреждён: в корне должен быть JSON-объект");
        add("not a visual script: \"format\" must be \"(.*)\"", "это не визуальный скрипт: поле «format» должно быть «%1»");
        add("unsupported graph format version (\\d+); update the engine", "формат графа версии %1 не поддерживается, обновите движок");
        add("not a code graph: \"schema\" must be \"(.*)\"", "это не граф кода: поле «schema» должно быть «%1»");
        add("unsupported script version (\\d+); update the engine", "скрипт версии %1 не поддерживается, обновите движок");
        add("\"(variables|nodes|connections)\" must be an array", "файл повреждён: «%1» должно быть массивом");
        add("invalid graph: (.*)", "граф повреждён: %1");
        // Переменные.
        add("a variable without a name", "переменная без имени");
        add("variable \"(.*)\" is declared twice", "переменная «%1» объявлена дважды");
        add("variable \"(.*)\" has unknown type \"(.*)\"", "у переменной «%1» неизвестный тип «%2»");
        add("variable \"(.*)\": the value is not a (\\w+)", "переменная «%1»: значение не подходит к типу «%2»", {2});
        // Ноды.
        add("a node without a positive integer id", "нода без целого положительного id");
        add("node id (\\d+) is used twice", "id ноды %1 встречается дважды");
        add("unknown node type \"(.*)\"", "неизвестный тип ноды «%1»: возможно, игровой модуль проекта устарел");
        add("\"outputs\" must be a whole number from 1 to (\\d+)", "число выходов должно быть целым от 1 до %1");
        add("choose a key", "выберите клавишу");
        add("unknown key \"(.*)\"", "неизвестная клавиша «%1»");
        add("choose a variable", "выберите переменную");
        add("no variable \"(.*)\"", "нет переменной «%1»");
        add("the value is not a (\\w+)", "значение не подходит к типу «%1»", {1});
        add("default of input (.*) is not a (\\w+)", "значение входа %1 не подходит к типу «%2»", {2});
        // Связи.
        add("a link must be an object", "файл повреждён: связь должна быть JSON-объектом");
        add("a link to a missing node (\\d+)", "связь ведёт к несуществующей ноде %1");
        add("no output \"(.*)\"", "нет выхода «%1»");
        add("no input \"(.*)\"", "нет входа «%1»");
        add("input (.*) takes the execution flow, not a value", "вход %1 принимает поток выполнения, а не значение");
        add("input (.*) takes a value, not the execution flow", "вход %1 принимает значение, а не поток выполнения");
        add("output (.*) has more than one link", "у выхода %1 больше одной связи");
        add("input (.*) takes (\\w+), got (\\w+)", "вход %1 принимает тип «%2», а подключён «%3»", {2, 3});
        add("input (.*) has more than one link", "у входа %1 больше одной связи");
        add("the value depends on itself \\(a data cycle\\)", "значение зависит само от себя (цикл по данным)");
        // Выполнение в Play.
        add("more than (\\d+) steps in one event: probably an endless loop", "больше %1 шагов за одно событие: вероятно, бесконечный цикл");
        add("no object \\(the Target input is empty or the object was not found\\)",
            "нет объекта: вход Target пуст или объект с таким именем не найден");
        add("object #(\\d+) is no longer in the scene", "объекта #%1 больше нет в сцене");
        add("division by zero", "деление на ноль");
        add("Min is greater than Max", "Min больше Max");
        add("unexpected error: (.*)", "непредвиденная ошибка: %1");
        add("internal: (.*)", "внутренняя ошибка: %1");
        return list;
    }();
    return rules;
}
} // namespace

QString EditorCore::ScriptMessageRu(const QString& message)
{
    for (const auto& rule : Rules())
    {
        const auto match = rule.pattern.match(message);
        if (!match.hasMatch()) continue;
        // Один проход по шаблону: «%» внутри подставленного текста не разворачивается.
        QString text;
        for (int i = 0; i < rule.russian.size(); ++i)
        {
            const QChar c = rule.russian[i];
            if (c == QLatin1Char('%') && i + 1 < rule.russian.size() && rule.russian[i + 1].isDigit())
            {
                const int group = rule.russian[++i].digitValue();
                text += rule.types.contains(group) ? TypeRu(match.captured(group)) : match.captured(group);
            }
            else text += c;
        }
        return text;
    }
    return message;
}

QString EditorCore::LocalizeScriptMessage(const QString& message)
{
    return EditorLocale::Instance().Language() == EditorLanguage::Ru ? ScriptMessageRu(message) : message;
}
