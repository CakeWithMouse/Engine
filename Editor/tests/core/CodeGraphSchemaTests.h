#pragma once

#include "../../Core/CodeGraphSchema.h"
#include "../../Core/Graph/GraphClipboard.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cstdlib>

namespace
{

int gCodeSchemaChecks = 0;

void CheckCodeSchema(bool cond, const char* msg)
{
    ++gCodeSchemaChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (CodeGraphSchema): %s\n", msg);
        std::exit(1);
    }
}

// Каталог в формате Script::CatalogJson движка (часть нод).
const char* const CodeCatalog = R"json({
  "format": "ITMOEngine.Graph", "formatVersion": 1, "schema": "code", "schemaVersion": 1, "maxSequenceOutputs": 8,
  "keys": ["A", "W", "Space"],
  "nodes": [
    {"type": "Event.Tick", "group": "Events", "name": {"ru": "Каждый кадр", "en": "Every frame"}, "hint": {"ru": "", "en": ""}, "event": true,
     "pins": [{"name": "then", "input": false, "type": "exec"}, {"name": "DeltaTime", "input": false, "type": "float"}], "properties": []},
    {"type": "Event.KeyPressed", "group": "Events", "name": {"ru": "Клавиша нажата", "en": "Key pressed"}, "hint": {"ru": "", "en": ""}, "event": true,
     "pins": [{"name": "then", "input": false, "type": "exec"}], "properties": [{"name": "key", "kind": "key"}]},
    {"type": "Flow.Sequence", "group": "Flow", "name": {"ru": "Последовательность", "en": "Sequence"}, "hint": {"ru": "", "en": ""}, "event": false,
     "pins": [{"name": "exec", "input": true, "type": "exec"}], "properties": [{"name": "outputs", "kind": "outputs"}]},
    {"type": "Action.Print", "group": "Actions", "name": {"ru": "Вывести в консоль", "en": "Print"}, "hint": {"ru": "", "en": ""}, "event": false,
     "pins": [{"name": "exec", "input": true, "type": "exec"}, {"name": "Text", "input": true, "type": "any"}, {"name": "then", "input": false, "type": "exec"}],
     "properties": []},
    {"type": "Action.Translate", "group": "Actions", "name": {"ru": "Сдвинуть", "en": "Translate"}, "hint": {"ru": "", "en": ""}, "event": false,
     "pins": [{"name": "exec", "input": true, "type": "exec"}, {"name": "Target", "input": true, "type": "object"}, {"name": "Offset", "input": true, "type": "vec3"},
              {"name": "then", "input": false, "type": "exec"}], "properties": []},
    {"type": "Variable.Get", "group": "Variables", "name": {"ru": "Переменная", "en": "Get variable"}, "hint": {"ru": "", "en": ""}, "event": false,
     "pins": [{"name": "Value", "input": false, "type": "variable"}], "properties": [{"name": "variable", "kind": "variable"}]},
    {"type": "Const.Float", "group": "Constants", "name": {"ru": "Число", "en": "Number"}, "hint": {"ru": "", "en": ""}, "event": false,
     "pins": [{"name": "Value", "input": false, "type": "float"}], "properties": [{"name": "value", "kind": "float"}]},
    {"type": "Math.Add", "group": "Math", "name": {"ru": "Сложение", "en": "Add"}, "hint": {"ru": "", "en": ""}, "event": false,
     "pins": [{"name": "A", "input": true, "type": "float"}, {"name": "B", "input": true, "type": "float"}, {"name": "Out", "input": false, "type": "float"}],
     "properties": []}
  ]
})json";

inline int RunCodeGraphSchemaTests()
{
    using namespace EditorCore;

    CodeGraphSchema schema;
    QString error;
    CheckCodeSchema(!schema.LoadCatalog(QStringLiteral("{\"schema\":\"ai.bt\",\"nodes\":[]}"), &error) && !error.isEmpty() && !schema.IsLoaded(),
                    "A catalog of another schema is refused");
    CheckCodeSchema(schema.LoadCatalog(QString::fromUtf8(CodeCatalog), &error) && schema.IsLoaded(), "Engine catalog loads");
    CheckCodeSchema(schema.IsKnownNode(QStringLiteral("Event.Tick")) && !schema.IsKnownNode(QStringLiteral("Teleport")), "Known nodes come from the catalog");
    CheckCodeSchema(schema.KeyNames() == QStringList({QStringLiteral("A"), QStringLiteral("W"), QStringLiteral("Space")}), "Key names come from the catalog");
    const auto groups = schema.Groups();
    CheckCodeSchema(groups.size() == 6 && groups[0].types.contains(QStringLiteral("Event.KeyPressed")) && groups[0].searchText.contains(QStringLiteral("Events")),
                    "Groups keep catalog order and search names");
    CheckCodeSchema(schema.SearchKeywords(QStringLiteral("Action.Translate")).contains(QStringLiteral("Сдвинуть")), "Russian name is searchable");
    CheckCodeSchema(schema.Id() == QStringLiteral("code") && schema.ClipboardMarker() != QStringLiteral("materialGraphNodes"), "Code schema has its own id and marker");

    // Пины по свойствам ноды и по переменным документа.
    GraphNodeData sequence;
    sequence.type = QStringLiteral("Flow.Sequence");
    sequence.properties[QStringLiteral("outputs")] = 3;
    CheckCodeSchema(schema.Pins(sequence).size() == 4 && schema.Pins(sequence).last().name == QStringLiteral("then2"), "Sequence outputs follow the property");
    sequence.properties[QStringLiteral("outputs")] = 99;
    CheckCodeSchema(schema.Pins(sequence).size() == 9, "Sequence outputs are clamped to the catalog maximum");

    GraphNodeData variable;
    variable.type = QStringLiteral("Variable.Get");
    variable.properties[QStringLiteral("variable")] = QStringLiteral("Speed");
    CheckCodeSchema(schema.Pins(variable)[0].type == QStringLiteral("any"), "Unknown variable gives an untyped pin");
    schema.SetVariables(QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("Speed")}, {QStringLiteral("type"), QStringLiteral("vec3")}}});
    CheckCodeSchema(schema.Pins(variable)[0].type == QStringLiteral("vec3") && schema.VariableNames() == QStringList{QStringLiteral("Speed")},
                    "Variable pin takes the variable type");

    // Правила связей.
    const auto pin = [](const char* type, bool input) { return GraphPin{QStringLiteral("p"), input, QString::fromLatin1(type), {}}; };
    CheckCodeSchema(schema.OutputTakesOneLink(pin("exec", false)) && !schema.InputTakesOneLink(pin("exec", true)), "Exec: one link out, many in");
    CheckCodeSchema(!schema.OutputTakesOneLink(pin("float", false)) && schema.InputTakesOneLink(pin("float", true)), "Data: many links out, one in");
    CheckCodeSchema(schema.AllowsCycle(pin("exec", false), pin("exec", true)) && !schema.AllowsCycle(pin("float", false), pin("float", true)),
                    "Only exec links may loop");
    CheckCodeSchema(!schema.LinkProblem(pin("float", false), pin("vec3", true)).isEmpty(), "Different value types are refused");
    CheckCodeSchema(schema.LinkProblem(pin("vec3", false), pin("any", true)).isEmpty(), "Any input takes every value");
    CheckCodeSchema(!schema.LinkProblem(pin("exec", false), pin("any", true)).isEmpty(), "Flow cannot feed a value");

    GraphDocument document;
    document.SetSchema(schema.Id(), schema.Version());
    const int tick = document.AddNode(QStringLiteral("Event.Tick"), {}, {});
    const int print = document.AddNode(QStringLiteral("Action.Print"), {}, {});
    const int again = document.AddNode(QStringLiteral("Action.Print"), {}, {});
    const int a = document.AddNode(QStringLiteral("Math.Add"), {}, {});
    const int b = document.AddNode(QStringLiteral("Math.Add"), {}, {});
    document.AddConnection(tick, QStringLiteral("then"), print, QStringLiteral("exec"));
    document.AddConnection(print, QStringLiteral("then"), again, QStringLiteral("exec"));
    document.AddConnection(a, QStringLiteral("Out"), b, QStringLiteral("A"));
    CheckCodeSchema(schema.ConnectionProblem(document, again, QStringLiteral("then"), print, QStringLiteral("exec")).isEmpty(), "Exec loop is allowed");
    CheckCodeSchema(!schema.ConnectionProblem(document, b, QStringLiteral("Out"), a, QStringLiteral("B")).isEmpty(), "Data cycle is refused");
    CheckCodeSchema(schema.ConnectionProblem(document, tick, QStringLiteral("DeltaTime"), print, QStringLiteral("Text")).isEmpty(), "Number feeds Print");

    // Новые ноды получают свойства, которые ждёт компилятор движка.
    QVariantMap properties;
    QString title = QStringLiteral("x");
    schema.PrepareNewNode(document, QStringLiteral("Event.KeyPressed"), properties, title);
    CheckCodeSchema(title.isEmpty() && properties.value(QStringLiteral("key")).toString() == QStringLiteral("Space"), "Key event defaults to Space");
    properties.clear();
    schema.PrepareNewNode(document, QStringLiteral("Variable.Get"), properties, title);
    CheckCodeSchema(properties.value(QStringLiteral("variable")).toString() == QStringLiteral("Speed"), "Variable node takes the first variable");
    properties.clear();
    schema.PrepareNewNode(document, QStringLiteral("Flow.Sequence"), properties, title);
    CheckCodeSchema(properties.value(QStringLiteral("outputs")).toInt() == 2, "Sequence starts with two outputs");
    properties.clear();
    schema.PrepareNewNode(document, QStringLiteral("Const.Float"), properties, title);
    CheckCodeSchema(properties.contains(QStringLiteral("value")), "Constant gets a value");
    GraphNodeData titled;
    titled.type = QStringLiteral("Action.Translate");
    CheckCodeSchema(schema.NodeTitle(titled) == schema.DisplayName(QStringLiteral("Action.Translate")), "Nodes show the catalog name");

    // Файл скрипта: заголовок V1 со схемой code и буфер обмена этой схемы.
    GraphDocument loaded;
    CheckCodeSchema(loaded.ImportFromJson(document.ExportToJson()) && loaded.Schema() == QStringLiteral("code") &&
                        loaded.FileFormat() == GraphDocument::Format::V1, "Script file round-trips as code schema");
    const QString clip = GraphClipboard::Encode(document, {print, a}, schema.ClipboardMarker(), {});
    CheckCodeSchema(!GraphClipboard::Decode(clip.toUtf8(), schema.ClipboardMarker()).isEmpty() &&
                        GraphClipboard::Decode(clip.toUtf8(), QStringLiteral("materialGraphNodes")).isEmpty(), "Script nodes do not paste into materials");
    return gCodeSchemaChecks;
}

} // namespace
