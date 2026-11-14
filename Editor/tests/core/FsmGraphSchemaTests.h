#pragma once

#include "../../Core/EditorLocale.h"
#include "../../Core/FsmGraphSchema.h"
#include "../../Core/Graph/GraphSchemaRegistry.h"

#include <QJsonArray>
#include <QJsonObject>
#include <cstdio>
#include <cstdlib>

namespace
{

int gFsmSchemaChecks = 0;

void CheckFsm(bool cond, const char* msg)
{
    ++gFsmSchemaChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (FsmGraphSchema): %s\n", msg);
        std::exit(1);
    }
}

void ExpectAiRu(const char* english, const char* russian)
{
    const QString actual = EditorCore::AiMessageRu(QString::fromUtf8(english));
    if (actual != QString::fromUtf8(russian)) std::fprintf(stderr, "  \"%s\" -> \"%s\"\n", english, actual.toUtf8().constData());
    CheckFsm(actual == QString::fromUtf8(russian), english);
}

// Каталог в формате Ai::MachineCatalogJson движка (часть блоков).
const char* const FsmCatalog = R"json({
  "format": "ITMOEngine.Graph", "schema": "ai.fsm", "schemaVersion": 1,
  "triggers": ["completed", "succeeded", "failed", "event", "tick"],
  "blocks": [
    {"type": "Test.Fire", "kind": "task", "nameRu": "Огонь", "nameEn": "Fire", "hintRu": "", "hintEn": "", "engine": false,
     "params": [{"name": "salvos", "type": "int", "labelRu": "Залпов", "labelEn": "Salvos", "required": false, "default": 3, "min": 1, "max": 10}]},
    {"type": "Ai.MoveTo", "kind": "task", "nameRu": "Идти к", "nameEn": "Move to", "hintRu": "Идёт", "hintEn": "Goes", "engine": true,
     "params": [{"name": "target", "type": "key:place", "labelRu": "Куда", "labelEn": "Where", "required": true},
                {"name": "radius", "type": "float", "labelRu": "Радиус", "labelEn": "Radius", "required": false, "default": 1.0, "min": 0.0}]},
    {"type": "Ai.Wait", "kind": "task", "nameRu": "Ждать", "nameEn": "Wait", "hintRu": "", "hintEn": "", "engine": true,
     "params": [{"name": "seconds", "type": "float", "labelRu": "Секунды", "labelEn": "Seconds", "required": false, "default": 1.0, "min": 0.0}]},
    {"type": "Ai.CanSee", "kind": "condition", "nameRu": "Видит", "nameEn": "Can see", "hintRu": "", "hintEn": "", "engine": true,
     "params": [{"name": "target", "type": "key:object", "labelRu": "Кого", "labelEn": "Whom", "required": true}]},
    {"type": "Ai.Compare", "kind": "condition", "nameRu": "Сравнить число", "nameEn": "Compare number", "hintRu": "", "hintEn": "", "engine": true,
     "params": [{"name": "key", "type": "key:number", "labelRu": "Ключ", "labelEn": "Key", "required": true}]},
    {"type": "Test.Watch", "kind": "service", "nameRu": "Следить", "nameEn": "Watch", "hintRu": "", "hintEn": "", "engine": false, "params": []}
  ]
})json";

int RunFsmGraphSchemaTests()
{
    using namespace EditorCore;
    gFsmSchemaChecks = 0;
    EditorLocale::Instance().SetLanguage(EditorLanguage::Ru);

    FsmGraphSchema schema;
    QString error;
    CheckFsm(!schema.LoadCatalog(QStringLiteral("{\"schema\":\"code\"}"), &error) && !error.isEmpty() && !schema.IsLoaded(), "a catalog of another schema is refused");
    CheckFsm(schema.LoadCatalog(QString::fromUtf8(FsmCatalog), &error) && schema.IsLoaded(), "the engine catalog loads");
    CheckFsm(schema.FindBlock(QStringLiteral("Ai.MoveTo")) && schema.FindBlock(QStringLiteral("Ai.MoveTo"))->params.size() == 2 &&
             schema.FindBlock(QStringLiteral("Ai.MoveTo"))->params[1].fallback.toDouble() == 1.0, "blocks keep their parameters");
    const auto tasks = schema.Blocks(QStringLiteral("task"));
    CheckFsm(tasks.size() == 3 && tasks[0]->engine && tasks[1]->engine && tasks[2]->type == QStringLiteral("Test.Fire"), "engine blocks come before game blocks");
    CheckFsm(schema.Blocks(QStringLiteral("service")).size() == 1, "blocks are listed by kind");

    // Память и ключи подходящего типа.
    schema.SetVariables(QJsonArray{QJsonObject{{"name", "Player"}, {"type", "object"}}, QJsonObject{{"name", "Home"}, {"type", "vec3"}},
                                   QJsonObject{{"name", "Lives"}, {"type", "int"}}, QJsonObject{{"name", "Speed"}, {"type", "float"}}});
    CheckFsm(schema.KeysFor(QStringLiteral("key:place")) == QStringList({QStringLiteral("Player"), QStringLiteral("Home")}), "a place key is an object or a vector");
    CheckFsm(schema.KeysFor(QStringLiteral("key:number")) == QStringList({QStringLiteral("Lives"), QStringLiteral("Speed")}), "a number key is int or float");
    CheckFsm(schema.KeysFor(QStringLiteral("key:object")) == QStringList({QStringLiteral("Player")}) && schema.KeysFor(QStringLiteral("key")).size() == 4,
             "keys of one type and of any type");

    // Ноды, пины и правила переходов.
    GraphDocument document;
    document.SetSchema(schema.Id(), 1);
    QVariantMap properties;
    QString title;
    schema.PrepareNewNode(document, QStringLiteral("Fsm.State"), properties, title);
    const int patrol = document.AddNode(QStringLiteral("Fsm.State"), title, QPointF(0, 0), properties);
    properties.clear();
    schema.PrepareNewNode(document, QStringLiteral("Fsm.State"), properties, title);
    CheckFsm(properties.value(QStringLiteral("name")) == QStringLiteral("State2") && title == QStringLiteral("State2"), "new states get free names");
    const int chase = document.AddNode(QStringLiteral("Fsm.State"), title, QPointF(0, 200), properties);
    const int finish = document.AddNode(QStringLiteral("Fsm.Finish"), {}, QPointF(300, 0));
    CheckFsm(schema.Pins(*document.FindNode(patrol)).size() == 2 && schema.Pins(*document.FindNode(finish)).size() == 1, "a state has in and out, Finish only in");
    CheckFsm(schema.ConnectionProblem(document, patrol, QStringLiteral("out"), chase, QStringLiteral("in")).isEmpty(), "state to state");
    document.AddConnection(patrol, QStringLiteral("out"), chase, QStringLiteral("in"));
    CheckFsm(schema.ConnectionProblem(document, chase, QStringLiteral("out"), patrol, QStringLiteral("in")).isEmpty(), "transitions may form cycles");
    CheckFsm(!schema.ConnectionProblem(document, patrol, QStringLiteral("out"), patrol, QStringLiteral("in")).isEmpty(), "no transition to itself");
    CheckFsm(!schema.ConnectionProblem(document, finish, QStringLiteral("in"), patrol, QStringLiteral("in")).isEmpty(), "Finish has no output");
    CheckFsm(schema.NodeTitle(*document.FindNode(finish)) == QStringLiteral("Закончить"), "targets are named by type");
    GraphNodeData renamed = *document.FindNode(patrol);
    schema.SetNodeName(renamed, QStringLiteral("Patrol"));
    CheckFsm(schema.NodeName(renamed) == QStringLiteral("Patrol") && renamed.title == QStringLiteral("Patrol"), "renaming sets name and title");
    CheckFsm(!schema.NameProblem(QStringLiteral("A/B")).isEmpty() && !schema.NameProblem(QStringLiteral(" ")).isEmpty() &&
             schema.NameProblem(QStringLiteral("Патруль")).isEmpty(), "state names: not empty, no '/'");

    // Подписи блоков и переходов.
    const QVariantMap move{{"type", "Ai.MoveTo"}, {"target", "Player"}, {"radius", 1.5}};
    CheckFsm(schema.BlockSummary(move) == QStringLiteral("Идти к: Player, 1.5"), "a block summary lists its values");
    CheckFsm(schema.BlockSummary({{"type", "Ai.CanSee"}, {"target", "Player"}, {"not", true}}) == QStringLiteral("НЕ Видит: Player"), "an inverted condition");
    CheckFsm(schema.BlockSummary({{"type", "Ai.Any"}, {"items", QVariantList{QVariantMap{{"type", "Ai.CanSee"}, {"target", "Player"}}}}}) ==
             QStringLiteral("Хотя бы одно из (Видит: Player)"), "a group of conditions");
    CheckFsm(schema.BlockSummary({{"type", "Test.Watch"}, {"interval", 0.5}}) == QStringLiteral("Следить · раз в 0.5 с"), "a service interval");
    CheckFsm(schema.BlockSummary({{"type", "Ai.Fly"}}) == QStringLiteral("Ai.Fly"), "an unknown block shows its type");
    GraphConnectionData link;
    link.properties = {{"trigger", "event"}, {"event", "Ai.Spotted"}, {"delay", 0.5},
                       {"conditions", QVariantList{QVariantMap{{"type", "Ai.CanSee"}, {"target", "Player"}}}}};
    CheckFsm(schema.LinkLabel(link) == QStringLiteral("Ai.Spotted · Видит: Player · через 0.5 с"), "a transition label");
    link.properties = {};
    CheckFsm(schema.LinkLabel(link).isEmpty() && !schema.LinkFields(document, link).isEmpty(), "a plain completed transition has no label but has properties");

    // Вложенность: дети по порядку, предки.
    const int walk = document.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Walk"), QPointF(40, 100), {{"name", "Walk"}, {"parent", patrol}});
    const int look = document.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Look"), QPointF(0, 100), {{"name", "Look"}, {"parent", patrol}});
    const int deep = document.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Deep"), QPointF(0, 300), {{"name", "Deep"}, {"parent", look}});
    CheckFsm(FsmGraphSchema::Children(document, patrol) == QList<int>({look, walk}), "children: top to bottom, then left to right");
    CheckFsm(FsmGraphSchema::IsAncestor(document, patrol, deep) && !FsmGraphSchema::IsAncestor(document, deep, patrol), "ancestors");
    GraphNodeData pasted = *document.FindNode(deep);
    pasted.properties[QStringLiteral("parent")] = 999;
    schema.PreparePastedNode(document, pasted);
    CheckFsm(!pasted.properties.contains(QStringLiteral("parent")) && schema.NodeName(pasted) == QStringLiteral("Deep2"), "a pasted state without its parent goes to the top");

    // Предупреждения.
    const auto warnings = schema.Warnings(document);
    CheckFsm(warnings.contains(finish), "a target nothing leads to");
    CheckFsm(warnings.value(chase).isEmpty() || !warnings.value(chase).contains(QStringLiteral("никто")), "a state with a transition into it runs");
    CheckFsm(warnings.value(walk).contains(QStringLiteral("никто")), "a second child without transitions never runs");
    CheckFsm(warnings.value(deep).contains(QStringLiteral("нет задач")), "a leaf without tasks does nothing");
    {
        // «Следующее» ведёт в следующего соседа: в него переходят, предупреждения нет.
        GraphDocument chain;
        chain.SetSchema(schema.Id(), 1);
        const QVariantList wait{QVariantMap{{"type", "Ai.Wait"}}};
        const int first = chain.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("First"), QPointF(0, 0), {{"name", "First"}, {"tasks", wait}});
        const int second = chain.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Second"), QPointF(0, 100), {{"name", "Second"}, {"tasks", wait}});
        const int next = chain.AddNode(QStringLiteral("Fsm.Next"), {}, QPointF(300, 0));
        chain.AddConnection(first, QStringLiteral("out"), next, QStringLiteral("in"));
        chain.AddConnection(second, QStringLiteral("out"), first, QStringLiteral("in"));
        CheckFsm(schema.Warnings(chain).isEmpty(), "a state reached through Next is not reported");
    }

    // Реестр схем знает автомат.
    const auto* entry = GraphSchemaRegistry::Instance().Find(QStringLiteral("ai.fsm"));
    CheckFsm(entry && entry->fileSuffix == QStringLiteral(".fsm") && entry->create && entry->create()->Id() == QStringLiteral("ai.fsm"), "ai.fsm is registered");
    CheckFsm(GraphSchemaRegistry::Instance().ForFile(document.ExportToJson().toUtf8()) == entry, "a .fsm file is recognized by its header");

    // Сообщения загрузчика по-русски (тексты из Engine/Source/AI/MachineFile.cpp и MachineBlocks.cpp).
    ExpectAiRu("task 2 \"Ai.Wait\": \"seconds\" must be 0 or more", "задача 2 \"Ai.Wait\": «seconds» должно быть не меньше 0");
    ExpectAiRu("task 1: unknown block \"Ai.Fly\"", "задача 1: неизвестный блок «Ai.Fly»: возможно, игровой модуль проекта устарел");
    ExpectAiRu("enter condition 1 \"Ai.IsTrue\": \"key\": no variable \"Alert\"; declare it in \"variables\"",
               "условие входа 1 \"Ai.IsTrue\": «key»: нет переменной «Alert» — объявите её в памяти автомата");
    ExpectAiRu("task 4 \"Ai.MoveTo\": \"target\": variable \"flag\" is bool, the block needs object or vec3",
               "задача 4 \"Ai.MoveTo\": «target»: переменная «flag» типа bool, а блоку нужен тип object or vec3");
    ExpectAiRu("task 5: \"Ai.CanSee\" is a condition, not a task", "задача 5: «Ai.CanSee» — блок вида «condition», а здесь нужен «task»");
    ExpectAiRu("transition to \"Rest\": unknown trigger \"sometimes\"; use completed, succeeded, failed, event or tick",
               "переход в «Rest»: неизвестный триггер «sometimes»");
    ExpectAiRu("transition to Next: \"Only\" is the last state of its parent", "переход к «Следующее»: «Only» — последнее состояние у своего родителя, следующего нет");
    ExpectAiRu("transition to \"B\": name the event", "переход в «B»: задайте имя события");
    ExpectAiRu("two states are named \"A\"", "два состояния с именем «A»");
    ExpectAiRu("parent 99 is not a node", "родителя 99 нет в графе");
    ExpectAiRu("settings: \"targetKey\" needs a variable of type object, \"where\" is vec3", "настройки: «targetKey»: нужна переменная типа object, а «where» — vec3");
    ExpectAiRu("variable \"who\": an object has no starting value", "переменная «who»: у объекта нет начального значения");
    ExpectAiRu("cannot read the file", "не удаётся прочитать файл: возможно, его переименовали или удалили");
    ExpectAiRu("the machine has no states", "в автомате нет состояний");
    ExpectAiRu("State machine \"X\": something new", "State machine \"X\": something new"); // незнакомое остаётся
    return gFsmSchemaChecks;
}

} // namespace
