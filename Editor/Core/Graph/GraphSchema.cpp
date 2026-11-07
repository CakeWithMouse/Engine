#include "GraphSchema.h"
#include "EditorLocale.h"

#include <QSet>
#include <unordered_map>

namespace EditorCore
{

QColor GraphSchema::NodeColor(const GraphNodeData& node) const
{
    for (const auto& group : Groups())
        if (group.types.contains(node.type)) return group.color;
    return QColor(0x47, 0x55, 0x69);
}

QString GraphSchema::NodeTitle(const GraphNodeData& node) const
{
    // Своё имя (параметр, переменная, подпись из готового графа) или название типа на языке редактора.
    const bool ownTitle = !node.title.isEmpty() && (!NodeName(node).isEmpty() || node.title != node.type);
    return ownTitle ? node.title : DisplayName(node.type);
}

void GraphSchema::PreparePastedNode(const GraphDocument& document, GraphNodeData& node) const
{
    const QString name = NodeName(node);
    if (name.isEmpty()) return;
    const QString unique = UniqueName(document, name);
    if (unique == name) return;
    SetNodeName(node, unique);
    if (node.title == name) node.title = unique;
}

QString GraphSchema::UniqueName(const GraphDocument& document, const QString& base) const
{
    QSet<QString> taken;
    for (const auto& node : document.Nodes()) taken.insert(NodeName(node));
    QString name = base;
    for (int i = 2; taken.contains(name); ++i) name = base + QString::number(i);
    return name;
}

const GraphPin* GraphSchema::FindPin(const QVector<GraphPin>& pins, const QString& name, bool input) const
{
    for (const auto& pin : pins)
        if (pin.name == name && pin.isInput == input) return &pin;
    return nullptr;
}

QString GraphSchema::ConnectionProblem(const GraphDocument& document, int fromNode, const QString& fromPin, int toNode, const QString& toPin) const
{
    const GraphNodeData* from = document.FindNode(fromNode);
    const GraphNodeData* to = document.FindNode(toNode);
    if (!from || !to) return Tr("Нода не найдена.", "Node not found.");
    if (fromNode == toNode) return Tr("Нода не соединяется сама с собой.", "A node cannot link to itself.");
    const QVector<GraphPin> fromPins = Pins(*from), toPins = Pins(*to);
    const GraphPin* output = FindPin(fromPins, fromPin, false);
    const GraphPin* input = FindPin(toPins, toPin, true);
    if (!output || !input) return Tr("Нет такого пина.", "No such pin.");
    if (const QString problem = LinkProblem(*output, *input); !problem.isEmpty()) return problem;
    if (AllowsCycle(*output, *input)) return {};

    // Цикл: источник уже зависит от ноды назначения по связям, где циклы запрещены.
    // unordered_map: ссылки на значения переживают вставку новых.
    std::unordered_map<int, QVector<GraphPin>> pins;
    auto pinsOf = [&](int id) -> const QVector<GraphPin>& {
        auto it = pins.find(id);
        if (it == pins.end())
        {
            const GraphNodeData* node = document.FindNode(id);
            it = pins.emplace(id, node ? Pins(*node) : QVector<GraphPin>()).first;
        }
        return it->second;
    };
    QList<int> pending{fromNode};
    QSet<int> seen;
    while (!pending.isEmpty())
    {
        const int id = pending.takeLast();
        if (id == toNode) return Tr("Связь замкнула бы цикл.", "The link would make a cycle.");
        if (seen.contains(id)) continue;
        seen.insert(id);
        for (const auto& c : document.Connections())
        {
            if (c.toNode != id) continue;
            const GraphPin* out = FindPin(pinsOf(c.fromNode), c.fromPin, false);
            const GraphPin* in = FindPin(pinsOf(c.toNode), c.toPin, true);
            if (out && in && AllowsCycle(*out, *in)) continue;
            pending << c.fromNode;
        }
    }
    return {};
}

} // namespace EditorCore
