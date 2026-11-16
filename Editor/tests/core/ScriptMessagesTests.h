#pragma once

#include "../../Core/ScriptMessages.h"

#include <cstdio>
#include <cstdlib>

namespace
{

int gScriptMessageChecks = 0;

void CheckScriptMessage(bool cond, const char* msg)
{
    ++gScriptMessageChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (ScriptMessages): %s\n", msg);
        std::exit(1);
    }
}

void ExpectRu(const char* english, const char* russian)
{
    const QString actual = EditorCore::ScriptMessageRu(QString::fromUtf8(english));
    if (actual != QString::fromUtf8(russian))
        std::fprintf(stderr, "  \"%s\" -> \"%s\"\n", english, actual.toUtf8().constData());
    CheckScriptMessage(actual == QString::fromUtf8(russian), english);
}

int RunScriptMessagesTests()
{
    gScriptMessageChecks = 0;

    // Каждый текст из Engine/Source/Script и Game::GetScriptProblems переводится.
    const char* const engineMessages[] = {
        "cannot read the file",
        "invalid JSON: [json.exception.parse_error.101] parse error",
        "the root must be a JSON object",
        "not a visual script: \"format\" must be \"ITMOEngine.Graph\"",
        "unsupported graph format version 2; update the engine",
        "not a code graph: \"schema\" must be \"code\"",
        "unsupported script version 3; update the engine",
        "\"variables\" must be an array",
        "\"nodes\" must be an array",
        "\"connections\" must be an array",
        "invalid graph: type must be string",
        "a variable without a name",
        "variable \"Speed\" is declared twice",
        "variable \"Speed\" has unknown type \"int\"",
        "variable \"Speed\": the value is not a float",
        "a node without a positive integer id",
        "node id 4 is used twice",
        "unknown node type \"Action.Fly\"",
        "\"outputs\" must be a whole number from 1 to 8",
        "choose a key",
        "name the AI memory key",
        "the object is not an AI character",
        "unknown key \"Hyper\"",
        "choose a variable",
        "no variable \"Speed\"",
        "the value is not a vec3",
        "default of input Degrees is not a vec3",
        "a link must be an object",
        "a link to a missing node 12",
        "no output \"then\"",
        "no input \"exec\"",
        "input exec takes the execution flow, not a value",
        "input Target takes a value, not the execution flow",
        "output then has more than one link",
        "input Degrees takes vec3, got float",
        "input Target has more than one link",
        "the value depends on itself (a data cycle)",
        "more than 10000 steps in one event: probably an endless loop",
        "no object (the Target input is empty or the object was not found)",
        "object #42 is no longer in the scene",
        "division by zero",
        "Min is greater than Max",
        "unexpected error: bad_alloc",
        "internal: unexpected value type",
    };
    for (const char* message : engineMessages)
        CheckScriptMessage(EditorCore::ScriptMessageRu(QString::fromUtf8(message)) != QString::fromUtf8(message), message);

    // Подстановка групп и имён типов.
    ExpectRu("choose a variable", "выберите переменную");
    ExpectRu("no variable \"Скорость\"", "нет переменной «Скорость»");
    ExpectRu("input Degrees takes vec3, got float", "вход Degrees принимает тип «вектор», а подключён «число»");
    ExpectRu("variable \"Speed\": the value is not a bool", "переменная «Speed»: значение не подходит к типу «логическое»");
    ExpectRu("more than 10000 steps in one event: probably an endless loop",
             "больше 10000 шагов за одно событие: вероятно, бесконечный цикл");
    ExpectRu("no object (the Target input is empty or the object was not found)",
             "нет объекта: вход Target пуст или объект с таким именем не найден");
    // «%» в подставленном тексте не разворачивается.
    ExpectRu("no variable \"%2\"", "нет переменной «%2»");

    // Неизвестный текст (новая Game.dll) остаётся английским, частичное совпадение не переводится.
    ExpectRu("a brand new engine message", "a brand new engine message");
    ExpectRu("division by zero in node 3", "division by zero in node 3");

    return gScriptMessageChecks;
}

} // namespace
