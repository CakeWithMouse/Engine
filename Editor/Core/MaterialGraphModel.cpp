#include "MaterialGraphModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace EditorCore
{

int MaterialGraphModel::AddNode(const QString& type, const QString& title, const QPointF& pos, const QVariantMap& props)
{
    GraphNodeData node;
    node.id = nextNodeId_++;
    node.type = type;
    node.title = title;
    node.pos = pos;
    node.properties = props;
    nodes_.append(node);
    return node.id;
}

bool MaterialGraphModel::RemoveNode(int nodeId)
{
    int index = -1;
    for (int i = 0; i < nodes_.size(); ++i)
    {
        if (nodes_[i].id == nodeId)
        {
            index = i;
            break;
        }
    }
    if (index < 0)
        return false;

    nodes_.removeAt(index);

    // Remove associated connections
    for (int i = connections_.size() - 1; i >= 0; --i)
    {
        if (connections_[i].fromNode == nodeId || connections_[i].toNode == nodeId)
        {
            connections_.removeAt(i);
        }
    }
    return true;
}

bool MaterialGraphModel::AddConnection(int fromNode, const QString& fromPin, int toNode, const QString& toPin)
{
    for (const auto& c : connections_)
    {
        if (c.fromNode == fromNode && c.fromPin == fromPin && c.toNode == toNode && c.toPin == toPin)
            return false;
        // An input pin can only receive one connection
        if (c.toNode == toNode && c.toPin == toPin)
            return false;
    }

    GraphConnectionData conn;
    conn.fromNode = fromNode;
    conn.fromPin = fromPin;
    conn.toNode = toNode;
    conn.toPin = toPin;
    connections_.append(conn);
    return true;
}

bool MaterialGraphModel::RemoveConnection(int fromNode, const QString& fromPin, int toNode, const QString& toPin)
{
    for (int i = 0; i < connections_.size(); ++i)
    {
        const auto& c = connections_[i];
        if (c.fromNode == fromNode && c.fromPin == fromPin && c.toNode == toNode && c.toPin == toPin)
        {
            connections_.removeAt(i);
            return true;
        }
    }
    return false;
}

void MaterialGraphModel::Clear()
{
    nodes_.clear();
    connections_.clear();
    nextNodeId_ = 1;
}

void MaterialGraphModel::SetupDefaultMasterNode()
{
    Clear();

    // Выход материала: поля MaterialOutput освещения движка (Блинн-Фонг)
    AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(560, 180));

    // ColorParam BaseColor
    QVariantMap colorProps;
    colorProps[QStringLiteral("name")] = QStringLiteral("BaseColor");
    colorProps[QStringLiteral("color")] = QColor(220, 220, 225);
    AddNode(QStringLiteral("ColorParam"), QStringLiteral("BaseColor"), QPointF(200, 120), colorProps);

    // ScalarParam Specular
    QVariantMap specularProps;
    specularProps[QStringLiteral("name")] = QStringLiteral("Specular");
    specularProps[QStringLiteral("value")] = 0.5;
    AddNode(QStringLiteral("ScalarParam"), QStringLiteral("Specular"), QPointF(200, 280), specularProps);

    // Connect BaseColor.RGB -> MaterialOutput.Albedo
    AddConnection(2, QStringLiteral("RGB"), 1, QStringLiteral("Albedo"));
    // Connect Specular -> MaterialOutput.Specular
    AddConnection(3, QStringLiteral("Value"), 1, QStringLiteral("Specular"));
}

const GraphNodeData* MaterialGraphModel::FindNode(int id) const
{
    for (const auto& n : nodes_)
    {
        if (n.id == id)
            return &n;
    }
    return nullptr;
}

GraphNodeData* MaterialGraphModel::FindNode(int id)
{
    for (auto& n : nodes_)
    {
        if (n.id == id)
            return &n;
    }
    return nullptr;
}

QString MaterialGraphModel::ExportToJson() const
{
    QJsonObject root;
    root[QStringLiteral("version")] = 1;

    QJsonArray nodesArr;
    for (const auto& n : nodes_)
    {
        QJsonObject no;
        no[QStringLiteral("id")] = n.id;
        no[QStringLiteral("type")] = n.type;
        no[QStringLiteral("title")] = n.title;
        no[QStringLiteral("x")] = n.pos.x();
        no[QStringLiteral("y")] = n.pos.y();

        QJsonObject props;
        for (auto it = n.properties.begin(); it != n.properties.end(); ++it)
        {
            // canConvert<QColor>() верен и для строк ("BaseMap" разбирается как имя цвета): проверяем сам тип.
            if (it.value().metaType().id() == QMetaType::QColor)
            {
                QColor c = it.value().value<QColor>();
                QJsonArray colArr = { c.redF(), c.greenF(), c.blueF(), c.alphaF() };
                props[it.key()] = colArr;
            }
            else
            {
                props[it.key()] = QJsonValue::fromVariant(it.value());
            }
        }
        no[QStringLiteral("properties")] = props;
        nodesArr.append(no);
    }
    root[QStringLiteral("nodes")] = nodesArr;

    QJsonArray connArr;
    for (const auto& c : connections_)
    {
        QJsonObject co;
        co[QStringLiteral("fromNode")] = c.fromNode;
        co[QStringLiteral("fromPin")] = c.fromPin;
        co[QStringLiteral("toNode")] = c.toNode;
        co[QStringLiteral("toPin")] = c.toPin;
        connArr.append(co);
    }
    root[QStringLiteral("connections")] = connArr;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool MaterialGraphModel::ImportFromJson(const QString& jsonString)
{
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject())
        return false;

    QJsonObject root = doc.object();
    nodes_.clear();
    connections_.clear();
    nextNodeId_ = 1;

    QJsonArray nodesArr = root[QStringLiteral("nodes")].toArray();
    for (const auto& val : nodesArr)
    {
        QJsonObject no = val.toObject();
        GraphNodeData n;
        n.id = no[QStringLiteral("id")].toInt();
        n.type = no[QStringLiteral("type")].toString();
        n.title = no[QStringLiteral("title")].toString();
        n.pos = QPointF(no[QStringLiteral("x")].toDouble(), no[QStringLiteral("y")].toDouble());

        QJsonObject props = no[QStringLiteral("properties")].toObject();
        for (auto it = props.begin(); it != props.end(); ++it)
        {
            if (it.value().isArray())
            {
                QJsonArray colArr = it.value().toArray();
                if (colArr.size() == 4)
                {
                    n.properties[it.key()] = QColor::fromRgbF(
                        colArr[0].toDouble(), colArr[1].toDouble(),
                        colArr[2].toDouble(), colArr[3].toDouble());
                    continue;
                }
            }
            n.properties[it.key()] = it.value().toVariant();
        }

        nodes_.append(n);
        if (n.id >= nextNodeId_)
            nextNodeId_ = n.id + 1;
    }

    QJsonArray connArr = root[QStringLiteral("connections")].toArray();
    for (const auto& val : connArr)
    {
        QJsonObject co = val.toObject();
        GraphConnectionData c;
        c.fromNode = co[QStringLiteral("fromNode")].toInt();
        c.fromPin = co[QStringLiteral("fromPin")].toString();
        c.toNode = co[QStringLiteral("toNode")].toInt();
        c.toPin = co[QStringLiteral("toPin")].toString();
        connections_.append(c);
    }

    return true;
}

} // namespace EditorCore
