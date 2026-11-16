#pragma once

#include "../../Core/Graph/GraphClipboard.h"
#include "../../Core/Graph/GraphDocument.h"
#include "../../Core/Graph/GraphHistory.h"
#include "../../Core/Graph/GraphSchema.h"
#include "../../Core/MaterialGraphModel.h"
#include "../../Core/MaterialGraphSchema.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cstdlib>

namespace
{

int gGraphCoreChecks = 0;

void CheckGraphCore(bool cond, const char* msg)
{
    ++gGraphCoreChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (GraphCore): %s\n", msg);
        std::exit(1);
    }
}

// Граф материала по умолчанию, записанный редактором до выноса ядра графа (Legacy, *.mat.graph.json).
const char* const LegacyDefaultGraph = R"json({
    "connections": [
        {
            "fromNode": 2,
            "fromPin": "RGB",
            "toNode": 1,
            "toPin": "Albedo"
        },
        {
            "fromNode": 3,
            "fromPin": "Value",
            "toNode": 1,
            "toPin": "Specular"
        }
    ],
    "nodes": [
        {
            "id": 1,
            "properties": {
            },
            "title": "Material Output",
            "type": "MaterialOutput",
            "x": 560,
            "y": 180
        },
        {
            "id": 2,
            "properties": {
                "color": [
                    0.8627451062202454,
                    0.8627451062202454,
                    0.8823529481887817,
                    1
                ],
                "name": "BaseColor"
            },
            "title": "BaseColor",
            "type": "ColorParam",
            "x": 200,
            "y": 120
        },
        {
            "id": 3,
            "properties": {
                "name": "Specular",
                "value": 0.5
            },
            "title": "Specular",
            "type": "ScalarParam",
            "x": 200,
            "y": 280
        }
    ],
    "version": 1
}
)json";

// Схема, похожая на будущую схему кода: exec-пины (у выхода одна связь, у входа много, циклы можно)
// и типизированные data-пины. Число выходов Sequence задаётся свойством outputs.
class TestCodeSchema final : public EditorCore::GraphSchema
{
public:
    QString Id() const override { return QStringLiteral("test.code"); }
    QString ObjectPrefix() const override { return QStringLiteral("TestGraph"); }
    QString ClipboardMime() const override { return QStringLiteral("application/x-test-nodes"); }
    QString ClipboardMarker() const override { return QStringLiteral("testGraphNodes"); }
    QVector<EditorCore::GraphNodeGroup> Groups() const override
    {
        return {{QStringLiteral("Flow"), QStringLiteral("Flow"), {QStringLiteral("Event"), QStringLiteral("Print"), QStringLiteral("Sequence")}, QColor(Qt::red)},
                {QStringLiteral("Data"), QStringLiteral("Data"), {QStringLiteral("Number"), QStringLiteral("Add"), QStringLiteral("Variable")}, QColor(Qt::green)}};
    }
    bool IsKnownNode(const QString& type) const override
    {
        for (const auto& group : Groups())
            if (group.types.contains(type)) return true;
        return false;
    }
    QVector<EditorCore::GraphPin> Pins(const EditorCore::GraphNodeData& node) const override
    {
        using EditorCore::GraphPin;
        auto in = [](const char* name, const char* type) { return GraphPin{QString::fromLatin1(name), true, QString::fromLatin1(type), {}}; };
        auto out = [](const char* name, const char* type) { return GraphPin{QString::fromLatin1(name), false, QString::fromLatin1(type), {}}; };
        if (node.type == QStringLiteral("Event")) return {out("then", "exec")};
        if (node.type == QStringLiteral("Print")) return {in("exec", "exec"), in("Text", "string"), out("then", "exec")};
        if (node.type == QStringLiteral("Number") || node.type == QStringLiteral("Variable")) return {out("Value", "float")};
        if (node.type == QStringLiteral("Add")) return {in("A", "float"), in("B", "float"), out("Out", "float")};
        if (node.type == QStringLiteral("Sequence"))
        {
            QVector<GraphPin> pins{in("exec", "exec")};
            for (int i = 0; i < node.properties.value(QStringLiteral("outputs"), 2).toInt(); ++i)
                pins << GraphPin{QStringLiteral("then%1").arg(i), false, QStringLiteral("exec"), {}};
            return pins;
        }
        return {};
    }
    bool InputTakesOneLink(const EditorCore::GraphPin& input) const override { return input.type != QStringLiteral("exec"); }
    bool OutputTakesOneLink(const EditorCore::GraphPin& output) const override { return output.type == QStringLiteral("exec"); }
    bool AllowsCycle(const EditorCore::GraphPin& output, const EditorCore::GraphPin&) const override { return output.type == QStringLiteral("exec"); }
    QString LinkProblem(const EditorCore::GraphPin& output, const EditorCore::GraphPin& input) const override
    {
        return output.type == input.type ? QString() : QStringLiteral("type mismatch");
    }
    bool IsFixed(const EditorCore::GraphNodeData& node) const override { return node.type == QStringLiteral("Event"); }
    QString NodeName(const EditorCore::GraphNodeData& node) const override
    {
        return node.type == QStringLiteral("Variable") ? node.properties.value(QStringLiteral("name")).toString() : QString();
    }
    void SetNodeName(EditorCore::GraphNodeData& node, const QString& name) const override { node.properties[QStringLiteral("name")] = name; }
};

inline int RunGraphCoreTests()
{
    using namespace EditorCore;

    // Legacy: формат графа материала не меняется ни на байт.
    {
        MaterialGraphModel model;
        model.SetupDefaultMasterNode();
        CheckGraphCore(model.ExportToJson() == QString::fromUtf8(LegacyDefaultGraph), "Default material graph is written exactly as before");
        GraphDocument loaded;
        CheckGraphCore(loaded.ImportFromJson(QString::fromUtf8(LegacyDefaultGraph)), "Legacy file loads");
        CheckGraphCore(loaded.FileFormat() == GraphDocument::Format::Legacy && loaded.Schema().isEmpty(), "Legacy file stays Legacy");
        CheckGraphCore(loaded.ExportToJson() == QString::fromUtf8(LegacyDefaultGraph), "Legacy file round-trips byte for byte");
        CheckGraphCore(loaded.FindNode(2)->properties.value(QStringLiteral("color")).metaType().id() == QMetaType::QColor, "Legacy colours load as QColor");
    }

    // Неизвестные поля и свойства связей переживают чтение и запись.
    {
        QJsonObject root = QJsonDocument::fromJson(QByteArray(LegacyDefaultGraph)).object();
        root[QStringLiteral("futureRoot")] = 7;
        QJsonArray nodes = root[QStringLiteral("nodes")].toArray();
        QJsonObject first = nodes[0].toObject();
        first[QStringLiteral("comment")] = QStringLiteral("keep me");
        nodes[0] = first;
        root[QStringLiteral("nodes")] = nodes;
        GraphDocument document;
        CheckGraphCore(document.ImportFromJson(QString::fromUtf8(QJsonDocument(root).toJson())), "Document with unknown fields loads");
        CheckGraphCore(document.FindNode(1)->extra.value(QStringLiteral("comment")).toString() == QStringLiteral("keep me"), "Unknown node field is kept");
        const QJsonObject written = QJsonDocument::fromJson(document.ExportToJson().toUtf8()).object();
        CheckGraphCore(written.value(QStringLiteral("futureRoot")).toInt() == 7, "Unknown root field is written back");
        CheckGraphCore(written.value(QStringLiteral("nodes")).toArray()[0].toObject().value(QStringLiteral("comment")).toString() == QStringLiteral("keep me"),
                       "Unknown node field is written back");
        CheckGraphCore(!written.value(QStringLiteral("connections")).toArray()[0].toObject().contains(QStringLiteral("properties")),
                       "Empty link properties are not written");

        const QString before = document.ExportToJson();
        CheckGraphCore(!document.ImportFromJson(QStringLiteral("[1, 2]")) && document.ExportToJson() == before, "Failed import keeps the document");
    }

    // V1: заголовок схемы, значения JSON как есть, переменные, более новая версия файла.
    {
        GraphDocument document;
        document.SetSchema(QStringLiteral("test.code"), 3);
        document.SetVariables(QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("Speed")}, {QStringLiteral("type"), QStringLiteral("float")}, {QStringLiteral("value"), 90}}});
        const int a = document.AddNode(QStringLiteral("Number"), QString(), QPointF(20, 40), {{QStringLiteral("value"), QVariantList{1.0, 2.0, 3.0, 4.0}}});
        const int b = document.AddNode(QStringLiteral("Add"), QString(), QPointF(200, 40));
        document.AddConnection(a, QStringLiteral("Value"), b, QStringLiteral("A"), {{QStringLiteral("priority"), 2}});
        const QString json = document.ExportToJson();
        const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();
        CheckGraphCore(root.value(QStringLiteral("format")).toString() == QLatin1String(GraphDocument::FormatName) &&
                           root.value(QStringLiteral("version")).toInt() == 1 && root.value(QStringLiteral("schema")).toString() == QStringLiteral("test.code") &&
                           root.value(QStringLiteral("schemaVersion")).toInt() == 3, "V1 header is written");

        GraphDocument loaded;
        CheckGraphCore(loaded.ImportFromJson(json) && loaded.FileFormat() == GraphDocument::Format::V1, "V1 file loads as V1");
        CheckGraphCore(loaded.Schema() == QStringLiteral("test.code") && loaded.SchemaVersion() == 3, "Schema id and version load");
        CheckGraphCore(loaded.Variables().size() == 1 && loaded.Variables()[0].toObject().value(QStringLiteral("name")).toString() == QStringLiteral("Speed"),
                       "Variables round-trip");
        CheckGraphCore(loaded.FindNode(a)->properties.value(QStringLiteral("value")).metaType().id() == QMetaType::QVariantList,
                       "V1 array of four numbers is not turned into a colour");
        CheckGraphCore(loaded.Connections().size() == 1 && loaded.Connections()[0].properties.value(QStringLiteral("priority")).toInt() == 2,
                       "Link properties round-trip");
        CheckGraphCore(loaded.ExportToJson() == json, "V1 file round-trips byte for byte");
        CheckGraphCore(!loaded.IsNewerFormat(), "Current version is not newer");

        QJsonObject future = root;
        future[QStringLiteral("version")] = 2;
        GraphDocument newer;
        CheckGraphCore(newer.ImportFromJson(QString::fromUtf8(QJsonDocument(future).toJson())) && newer.IsNewerFormat(), "Newer file version is detected");
    }

    // Документ сам не ограничивает число связей пина: это правило схемы. Модель материала его сохраняет.
    {
        GraphDocument document;
        const int a = document.AddNode(QStringLiteral("Number"), {}, {});
        const int b = document.AddNode(QStringLiteral("Number"), {}, {});
        const int add = document.AddNode(QStringLiteral("Add"), {}, {});
        CheckGraphCore(document.AddConnection(a, QStringLiteral("Value"), add, QStringLiteral("A")), "First link to an input");
        CheckGraphCore(document.AddConnection(b, QStringLiteral("Value"), add, QStringLiteral("A")), "Document accepts a second link to the input");
        CheckGraphCore(!document.AddConnection(b, QStringLiteral("Value"), add, QStringLiteral("A")), "Exact duplicate is refused");

        MaterialGraphModel material;
        material.SetupDefaultMasterNode();
        const int extra = material.AddNode(QStringLiteral("ColorParam"), QStringLiteral("Other"), {});
        CheckGraphCore(!material.AddConnection(extra, QStringLiteral("RGB"), 1, QStringLiteral("Albedo")), "Material input keeps one link");
        CheckGraphCore(document.RemoveNode(add) && document.Connections().isEmpty(), "Removing a node removes its links");
    }

    // История: склейка по ключу, отмена, повтор, предел, пустой шаг протяжки связи.
    {
        GraphHistory history;
        CheckGraphCore(!history.CanUndo() && !history.CanRedo(), "Empty history");
        history.Push(QStringLiteral("A"));
        history.Push(QStringLiteral("A"));
        QString restored;
        CheckGraphCore(history.Undo(QStringLiteral("B"), restored) && restored == QStringLiteral("A") && !history.CanUndo(), "Repeated snapshot is stored once");
        CheckGraphCore(history.Redo(QStringLiteral("A"), restored) && restored == QStringLiteral("B"), "Redo returns the undone state");

        history.Clear();
        history.Push(QStringLiteral("S0"), QStringLiteral("slider"));
        CheckGraphCore(history.Merges(QStringLiteral("slider")) && !history.Merges(QString()), "Same key merges, empty key never does");
        history.BreakMerge();
        CheckGraphCore(!history.Merges(QStringLiteral("slider")), "BreakMerge ends the merged step");

        history.Clear();
        history.Push(QStringLiteral("before"));
        history.Undo(QStringLiteral("after"), restored);
        history.Push(QStringLiteral("other"));
        CheckGraphCore(!history.CanRedo(), "A new edit drops redo");

        history.Clear();
        history.Push(QStringLiteral("G"), QStringLiteral("link:1"));
        CheckGraphCore(!history.DropEmptyStep(QStringLiteral("link:2"), QStringLiteral("G")), "Another key keeps the step");
        CheckGraphCore(history.DropEmptyStep(QStringLiteral("link:1"), QStringLiteral("G")) && !history.CanUndo(), "A link detached and put back leaves no step");

        history.Clear();
        for (int i = 0; i < GraphHistory::Limit + 25; ++i) history.Push(QString::number(i));
        int steps = 0;
        QString current = QStringLiteral("now");
        while (history.Undo(current, restored)) { current = restored; ++steps; }
        CheckGraphCore(steps == GraphHistory::Limit && restored == QString::number(25), "History keeps the newest Limit steps");
    }

    // Буфер обмена: выбранные ноды и связи между ними, обязательные ноды остаются, метка схемы.
    {
        const TestCodeSchema schema;
        GraphDocument document;
        document.SetSchema(schema.Id(), schema.Version());
        const int event = document.AddNode(QStringLiteral("Event"), {}, {});
        const int print = document.AddNode(QStringLiteral("Print"), {}, QPointF(200, 0));
        const int number = document.AddNode(QStringLiteral("Number"), {}, QPointF(0, 100), {{QStringLiteral("value"), QVariantList{1, 2, 3, 4}}});
        const int add = document.AddNode(QStringLiteral("Add"), {}, QPointF(100, 100));
        document.AddConnection(event, QStringLiteral("then"), print, QStringLiteral("exec"));
        document.AddConnection(number, QStringLiteral("Value"), add, QStringLiteral("A"));
        const auto copyable = [&](const GraphNodeData& node) { return !schema.IsFixed(node); };
        const QString clip = GraphClipboard::Encode(document, {event, print, number, add}, schema.ClipboardMarker(), copyable);
        const QJsonObject object = QJsonDocument::fromJson(clip.toUtf8()).object();
        CheckGraphCore(object.contains(schema.ClipboardMarker()) && object.value(QStringLiteral("schema")).toString() == schema.Id(), "Clip carries marker and schema");
        CheckGraphCore(object.value(QStringLiteral("nodes")).toArray().size() == 3, "Fixed node is not copied");
        CheckGraphCore(object.value(QStringLiteral("connections")).toArray().size() == 1, "Only links between copied nodes are copied");
        CheckGraphCore(GraphClipboard::Encode(document, {event}, schema.ClipboardMarker(), copyable).isEmpty(), "Nothing to copy gives an empty clip");
        CheckGraphCore(GraphClipboard::Decode(clip.toUtf8(), schema.ClipboardMarker()) == clip, "Clip of this schema is recognised");
        CheckGraphCore(GraphClipboard::Decode(clip.toUtf8(), QStringLiteral("materialGraphNodes")).isEmpty(), "Clip of another schema is refused");
        CheckGraphCore(GraphClipboard::Decode("not json", schema.ClipboardMarker()).isEmpty(), "Plain text is refused");
        GraphDocument pasted;
        CheckGraphCore(pasted.ImportFromJson(clip) && pasted.FileFormat() == GraphDocument::Format::V1 &&
                           pasted.FindNode(number)->properties.value(QStringLiteral("value")).metaType().id() == QMetaType::QVariantList,
                       "Clip of a V1 graph reads values as V1");

        MaterialGraphModel material;
        material.SetupDefaultMasterNode();
        const QString legacy = GraphClipboard::Encode(material, {1, 2, 3}, MaterialGraphSchema::Instance().ClipboardMarker(),
                                                      [](const GraphNodeData& n) { return !MaterialGraphSchema::Instance().IsFixed(n); });
        const QJsonObject legacyObject = QJsonDocument::fromJson(legacy.toUtf8()).object();
        CheckGraphCore(legacyObject.contains(QStringLiteral("materialGraphNodes")) && !legacyObject.contains(QStringLiteral("format")) &&
                           legacyObject.value(QStringLiteral("nodes")).toArray().size() == 2, "Material clip keeps the old marker and format");
    }

    // Правила связей схемы.
    {
        const TestCodeSchema schema;
        GraphDocument document;
        const int event = document.AddNode(QStringLiteral("Event"), {}, {});
        const int first = document.AddNode(QStringLiteral("Print"), {}, {});
        const int second = document.AddNode(QStringLiteral("Print"), {}, {});
        const int number = document.AddNode(QStringLiteral("Number"), {}, {});
        const int add = document.AddNode(QStringLiteral("Add"), {}, {});
        const int other = document.AddNode(QStringLiteral("Add"), {}, {});
        const int sequence = document.AddNode(QStringLiteral("Sequence"), {}, {}, {{QStringLiteral("outputs"), 4}});
        CheckGraphCore(schema.ConnectionProblem(document, event, QStringLiteral("then"), first, QStringLiteral("exec")).isEmpty(), "Exec link is valid");
        CheckGraphCore(!schema.ConnectionProblem(document, first, QStringLiteral("then"), first, QStringLiteral("exec")).isEmpty(), "Self link is refused");
        CheckGraphCore(!schema.ConnectionProblem(document, number, QStringLiteral("Nope"), add, QStringLiteral("A")).isEmpty(), "Unknown pin is refused");
        CheckGraphCore(!schema.ConnectionProblem(document, number, QStringLiteral("Value"), first, QStringLiteral("exec")).isEmpty(), "Data to exec is refused");
        CheckGraphCore(!schema.ConnectionProblem(document, number, QStringLiteral("Value"), first, QStringLiteral("Text")).isEmpty(), "Type mismatch is refused");
        CheckGraphCore(!schema.ConnectionProblem(document, add, QStringLiteral("A"), other, QStringLiteral("B")).isEmpty(), "Input used as output is refused");

        document.AddConnection(add, QStringLiteral("Out"), other, QStringLiteral("A"));
        CheckGraphCore(!schema.ConnectionProblem(document, other, QStringLiteral("Out"), add, QStringLiteral("B")).isEmpty(), "Data cycle is refused");
        document.AddConnection(first, QStringLiteral("then"), second, QStringLiteral("exec"));
        CheckGraphCore(schema.ConnectionProblem(document, second, QStringLiteral("then"), first, QStringLiteral("exec")).isEmpty(), "Exec loop is allowed");
        CheckGraphCore(schema.ConnectionProblem(document, sequence, QStringLiteral("then3"), first, QStringLiteral("exec")).isEmpty() &&
                           !schema.ConnectionProblem(document, sequence, QStringLiteral("then4"), first, QStringLiteral("exec")).isEmpty(),
                       "Pins follow node properties");

        const auto pins = schema.Pins(*document.FindNode(first));
        CheckGraphCore(!schema.InputTakesOneLink(*schema.FindPin(pins, QStringLiteral("exec"), true)) &&
                           schema.InputTakesOneLink(*schema.FindPin(pins, QStringLiteral("Text"), true)) &&
                           schema.OutputTakesOneLink(*schema.FindPin(pins, QStringLiteral("then"), false)),
                       "Link counts depend on the pin kind");
    }

    // Имена нод: свободное имя, вставка с занятым именем.
    {
        const TestCodeSchema schema;
        GraphDocument document;
        document.AddNode(QStringLiteral("Variable"), QStringLiteral("Speed"), {}, {{QStringLiteral("name"), QStringLiteral("Speed")}});
        document.AddNode(QStringLiteral("Variable"), QStringLiteral("Speed2"), {}, {{QStringLiteral("name"), QStringLiteral("Speed2")}});
        CheckGraphCore(schema.UniqueName(document, QStringLiteral("Speed")) == QStringLiteral("Speed3"), "Unique name skips taken names");
        CheckGraphCore(schema.UniqueName(document, QStringLiteral("Health")) == QStringLiteral("Health"), "Free name stays");
        GraphNodeData copy;
        copy.type = QStringLiteral("Variable");
        copy.title = QStringLiteral("Speed");
        copy.properties[QStringLiteral("name")] = QStringLiteral("Speed");
        schema.PreparePastedNode(document, copy);
        CheckGraphCore(schema.NodeName(copy) == QStringLiteral("Speed3") && copy.title == QStringLiteral("Speed3"), "Pasted node gets a free name");
        CheckGraphCore(schema.NodeTitle(*document.FindNode(1)) == QStringLiteral("Speed"), "Named node shows its name");
    }

    // Схема материала: имена HLSL, общие текстуры, обязательный выход.
    {
        const MaterialGraphSchema& schema = MaterialGraphSchema::Instance();
        MaterialGraphModel model;
        model.SetupDefaultMasterNode();
        QVariantMap properties;
        QString title;
        schema.PrepareNewNode(model, QStringLiteral("ColorParam"), properties, title);
        CheckGraphCore(properties.value(QStringLiteral("name")).toString() == QStringLiteral("Color") && title == QStringLiteral("Color") &&
                           properties.value(QStringLiteral("color")).value<QColor>() == QColor(Qt::white), "New colour parameter gets name and white");
        properties.clear();
        schema.PrepareNewNode(model, QStringLiteral("Panner"), properties, title);
        CheckGraphCore(title == QStringLiteral("Panner") && properties.value(QStringLiteral("speedX")).toDouble() == 0.5, "New panner gets default speed");

        const int texture = model.AddNode(QStringLiteral("Texture2D"), QStringLiteral("BaseMap"), {},
                                          {{QStringLiteral("textureName"), QStringLiteral("BaseMap")}, {QStringLiteral("texture"), QStringLiteral("/Game/a.png")}});
        GraphNodeData sharedTexture = *model.FindNode(texture);
        sharedTexture.properties.remove(QStringLiteral("texture"));
        schema.PreparePastedNode(model, sharedTexture);
        CheckGraphCore(schema.NodeName(sharedTexture) == QStringLiteral("BaseMap") &&
                           sharedTexture.properties.value(QStringLiteral("texture")).toString() == QStringLiteral("/Game/a.png"),
                       "Pasted texture shares the texture of the same name");
        GraphNodeData parameter = *model.FindNode(2);
        schema.PreparePastedNode(model, parameter);
        CheckGraphCore(schema.NodeName(parameter) == QStringLiteral("BaseColor2") && parameter.title == QStringLiteral("BaseColor2"),
                       "Pasted parameter gets a new name");

        GraphDocument empty;
        schema.EnsureRequiredNodes(empty);
        schema.EnsureRequiredNodes(empty);
        CheckGraphCore(empty.Nodes().size() == 1 && schema.IsFixed(empty.Nodes()[0]), "Material output is added once and fixed");
        CheckGraphCore(schema.NodeTitle(empty.Nodes()[0]) == schema.DisplayName(QStringLiteral("MaterialOutput")), "Output shows its type name");
        CheckGraphCore(schema.ConnectionProblem(model, 2, QStringLiteral("RGB"), texture, QStringLiteral("UV")).isEmpty(),
                       "Any material output may feed any input");
        CheckGraphCore(schema.Id().isEmpty() && schema.ClipboardMarker() == QStringLiteral("materialGraphNodes"), "Material schema keeps Legacy format and marker");
    }

    return gGraphCoreChecks;
}

} // namespace
