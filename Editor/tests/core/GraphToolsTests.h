#pragma once

#include "../../Core/CodeGraphSchema.h"
#include "../../Core/Graph/GraphClipboard.h"
#include "../../Core/Graph/GraphLayout.h"
#include "../../Core/Graph/GraphSchemaRegistry.h"
#include "../../Core/MaterialGraphSchema.h"

#include <QRectF>
#include <cstdio>
#include <cstdlib>

namespace
{

int gGraphToolChecks = 0;

void CheckGraphTool(bool cond, const char* msg)
{
    ++gGraphToolChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (GraphTools): %s\n", msg);
        std::exit(1);
    }
}

bool Overlap(const QHash<int, QPointF>& placed, const QSizeF& size)
{
    const QList<int> ids = placed.keys();
    for (int i = 0; i < ids.size(); ++i)
        for (int j = i + 1; j < ids.size(); ++j)
            if (QRectF(placed[ids[i]], size).intersects(QRectF(placed[ids[j]], size))) return true;
    return false;
}

int RunGraphToolsTests()
{
    using namespace EditorCore;
    gGraphToolChecks = 0;
    auto& registry = GraphSchemaRegistry::Instance();

    // Реестр: встроенные схемы, имена, буфер обмена и файлы.
    {
        CheckGraphTool(registry.Find(QStringLiteral("code")) && registry.Find(QString()), "material and code are registered");
        CheckGraphTool(registry.Find(QStringLiteral("code"))->fileSuffix == QStringLiteral(".vscript") && registry.Find(QStringLiteral("code"))->create,
                       "code schema has a file suffix and a factory");
        CheckGraphTool(registry.Find(QStringLiteral("code"))->create()->Id() == QStringLiteral("code"), "factory creates the code schema");
        CheckGraphTool(registry.DisplayName(QStringLiteral("unknown.schema")) == QStringLiteral("unknown.schema"), "unknown id is shown as is");

        GraphDocument material;
        material.AddNode(QStringLiteral("Constant"), {}, QPointF(0, 0));
        const QString clip = GraphClipboard::Encode(material, {1}, MaterialGraphSchema::Instance().ClipboardMarker(), {});
        const GraphSchemaEntry* fromClip = registry.ForClipboard(clip.toUtf8());
        CheckGraphTool(fromClip && fromClip->id.isEmpty(), "material nodes in the clipboard are recognized");
        CheckGraphTool(!registry.ForClipboard("{\"nodes\":[]}") && !registry.ForClipboard("text"), "foreign clipboard text is not a graph");

        GraphDocument script;
        script.SetSchema(QStringLiteral("code"), 1);
        script.AddNode(QStringLiteral("Event.Tick"), {}, QPointF(0, 0));
        CheckGraphTool(registry.ForFile(script.ExportToJson().toUtf8()) == registry.Find(QStringLiteral("code")), "script file is recognized by header");
        CheckGraphTool(registry.ForFile(material.ExportToJson().toUtf8()) == registry.Find(QString()), "headerless graph is a material");
        CheckGraphTool(!registry.ForFile("{\"version\":1}") && !registry.ForFile("not json"), "other JSON is not a graph");
        script.SetSchema(QStringLiteral("ai.bt"), 1);
        CheckGraphTool(!registry.ForFile(script.ExportToJson().toUtf8()), "unregistered schema is unknown");

        // Новая схема регистрируется и заменяет прежнюю запись с тем же id.
        registry.Register({QStringLiteral("test.tools"), QStringLiteral("Тест"), QStringLiteral("Test"), QStringLiteral("testToolNodes"), {}, {}});
        registry.Register({QStringLiteral("test.tools"), QStringLiteral("Тест 2"), QStringLiteral("Test 2"), QStringLiteral("testToolNodes"), {}, {}});
        int count = 0;
        for (const auto& entry : registry.Entries()) count += entry.id == QStringLiteral("test.tools");
        CheckGraphTool(count == 1 && registry.Find(QStringLiteral("test.tools"))->nameRu == QStringLiteral("Тест 2"), "same id replaces the entry");
        CheckGraphTool(registry.ForClipboard("{\"testToolNodes\":1,\"nodes\":[]}") == registry.Find(QStringLiteral("test.tools")), "registered marker");
    }

    // Свойства связи: запись, файл, отсутствующая связь.
    {
        GraphDocument document;
        document.SetSchema(QStringLiteral("test.tools"), 1);
        const int a = document.AddNode(QStringLiteral("State"), {}, QPointF(0, 0));
        const int b = document.AddNode(QStringLiteral("State"), {}, QPointF(200, 0));
        document.AddConnection(a, QStringLiteral("out"), b, QStringLiteral("in"));
        const QVariantMap condition{{QStringLiteral("condition"), QStringLiteral("Health < 20")}, {QStringLiteral("priority"), 2}};
        CheckGraphTool(document.SetConnectionProperties(a, QStringLiteral("out"), b, QStringLiteral("in"), condition), "properties set");
        CheckGraphTool(!document.SetConnectionProperties(b, QStringLiteral("out"), a, QStringLiteral("in"), condition), "missing link rejected");
        GraphDocument loaded;
        CheckGraphTool(loaded.ImportFromJson(document.ExportToJson()) && loaded.Connections().size() == 1 &&
                       loaded.Connections()[0].properties.value(QStringLiteral("condition")).toString() == QStringLiteral("Health < 20") &&
                       loaded.Connections()[0].properties.value(QStringLiteral("priority")).toInt() == 2, "link properties survive the file");
        CheckGraphTool(document.SetConnectionProperties(a, QStringLiteral("out"), b, QStringLiteral("in"), {}) &&
                       !document.ExportToJson().contains(QStringLiteral("Health")), "cleared properties are not written");
    }

    const QSizeF size(150, 80);
    auto nodeSize = [size](const GraphNodeData&) { return size; };

    // Слои: поток слева направо, константа перед потребителем, цикл не мешает, граф остаётся на месте.
    {
        GraphDocument document;
        const int tick = document.AddNode(QStringLiteral("Event.Tick"), {}, QPointF(900, 500));
        const int rotate = document.AddNode(QStringLiteral("Action.Rotate"), {}, QPointF(100, 100));
        const int print = document.AddNode(QStringLiteral("Action.Print"), {}, QPointF(300, 700));
        const int constant = document.AddNode(QStringLiteral("Const.Vec3"), {}, QPointF(400, 900));
        const int lonely = document.AddNode(QStringLiteral("Const.Float"), {}, QPointF(1200, 1200));
        document.AddConnection(tick, QStringLiteral("then"), rotate, QStringLiteral("exec"));
        document.AddConnection(rotate, QStringLiteral("then"), print, QStringLiteral("exec"));
        document.AddConnection(print, QStringLiteral("then"), rotate, QStringLiteral("exec")); // поток возвращается назад
        document.AddConnection(constant, QStringLiteral("Out"), print, QStringLiteral("Text"));
        const auto placed = ArrangeGraph(document, GraphLayoutStyle::Layered, nodeSize);
        CheckGraphTool(placed.size() == 5 && !Overlap(placed, size), "every node placed without overlap");
        CheckGraphTool(placed[tick].x() < placed[rotate].x() && placed[rotate].x() < placed[print].x(), "flow goes left to right");
        CheckGraphTool(placed[constant].x() < placed[print].x() && placed[constant].x() >= placed[rotate].x(), "constant sits just before its consumer");
        qreal minX = 1e9, minY = 1e9;
        for (const auto& p : placed)
        {
            minX = std::min(minX, p.x());
            minY = std::min(minY, p.y());
            CheckGraphTool(std::fmod(p.x(), 20.0) == 0 && std::fmod(p.y(), 20.0) == 0, "positions snap to the grid");
        }
        CheckGraphTool(minX == 100 && minY == 100, "graph keeps its top-left corner");
        CheckGraphTool(placed.contains(lonely), "unconnected node is placed too");
    }

    // Дерево: дети под родителем в порядке X, родитель над ними по центру, второй родитель не дублирует.
    {
        GraphDocument document;
        const int root = document.AddNode(QStringLiteral("Selector"), {}, QPointF(500, 600));
        const int right = document.AddNode(QStringLiteral("Task"), {}, QPointF(900, 0));
        const int left = document.AddNode(QStringLiteral("Sequence"), {}, QPointF(100, 0));
        const int leaf = document.AddNode(QStringLiteral("Task"), {}, QPointF(0, 900));
        document.AddConnection(root, QStringLiteral("children"), right, QStringLiteral("parent"));
        document.AddConnection(root, QStringLiteral("children"), left, QStringLiteral("parent"));
        document.AddConnection(left, QStringLiteral("children"), leaf, QStringLiteral("parent"));
        document.AddConnection(right, QStringLiteral("children"), leaf, QStringLiteral("parent")); // второй родитель
        const auto placed = ArrangeGraph(document, GraphLayoutStyle::Tree, nodeSize);
        CheckGraphTool(placed.size() == 4 && !Overlap(placed, size), "tree nodes placed without overlap");
        CheckGraphTool(placed[root].y() < placed[left].y() && placed[left].y() == placed[right].y() && placed[left].y() < placed[leaf].y(),
                       "depth goes top to bottom");
        CheckGraphTool(placed[left].x() < placed[right].x(), "children keep their X order");
        const qreal centre = placed[root].x() + size.width() / 2;
        CheckGraphTool(centre > placed[left].x() + size.width() / 2 && centre < placed[right].x() + size.width() / 2, "parent is centred over children");
        CheckGraphTool(std::abs(placed[leaf].x() - placed[left].x()) <= 20, "a node with two parents stays under the first");
    }

    CheckGraphTool(ArrangeGraph(GraphDocument(), GraphLayoutStyle::Layered, nodeSize).isEmpty(), "empty graph");
    return gGraphToolChecks;
}

} // namespace
