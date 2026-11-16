#include "GraphDocument.h"

#include <QColor>
#include <QJsonDocument>
#include <QJsonValue>

#include <algorithm>

namespace EditorCore
{

namespace
{
const QStringList LegacyRootKeys{QStringLiteral("version"), QStringLiteral("nodes"), QStringLiteral("connections"), QStringLiteral("comments")};
const QStringList RootKeys{QStringLiteral("format"), QStringLiteral("version"), QStringLiteral("schema"), QStringLiteral("schemaVersion"),
                           QStringLiteral("variables"), QStringLiteral("nodes"), QStringLiteral("connections"), QStringLiteral("comments")};
const QStringList NodeKeys{QStringLiteral("id"), QStringLiteral("type"), QStringLiteral("title"), QStringLiteral("x"), QStringLiteral("y"),
                           QStringLiteral("properties")};
const QStringList LinkKeys{QStringLiteral("fromNode"), QStringLiteral("fromPin"), QStringLiteral("toNode"), QStringLiteral("toPin"),
                           QStringLiteral("properties")};

QJsonObject Unknown(const QJsonObject& object, const QStringList& known)
{
    QJsonObject rest;
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!known.contains(it.key())) rest.insert(it.key(), it.value());
    return rest;
}

void Merge(QJsonObject& target, const QJsonObject& extra)
{
    for (auto it = extra.begin(); it != extra.end(); ++it)
        if (!target.contains(it.key())) target.insert(it.key(), it.value());
}

// Legacy: QColor -> [r,g,b,a]. canConvert<QColor>() верен и для строк ("BaseMap" разбирается как имя цвета),
// поэтому проверяется сам тип.
QJsonObject PropertiesToJson(const QVariantMap& properties, bool legacy)
{
    QJsonObject props;
    for (auto it = properties.begin(); it != properties.end(); ++it)
    {
        if (legacy && it.value().metaType().id() == QMetaType::QColor)
        {
            const QColor c = it.value().value<QColor>();
            props[it.key()] = QJsonArray{c.redF(), c.greenF(), c.blueF(), c.alphaF()};
        }
        else
            props[it.key()] = QJsonValue::fromVariant(it.value());
    }
    return props;
}

QVariantMap PropertiesFromJson(const QJsonObject& props, bool legacy)
{
    QVariantMap properties;
    for (auto it = props.begin(); it != props.end(); ++it)
    {
        if (legacy && it.value().isArray())
        {
            const QJsonArray color = it.value().toArray();
            if (color.size() == 4)
            {
                properties[it.key()] = QColor::fromRgbF(color[0].toDouble(), color[1].toDouble(), color[2].toDouble(), color[3].toDouble());
                continue;
            }
        }
        properties[it.key()] = it.value().toVariant();
    }
    return properties;
}
} // namespace

int GraphDocument::AddNode(const QString& type, const QString& title, const QPointF& pos, const QVariantMap& props)
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

bool GraphDocument::RemoveNode(int nodeId)
{
    for (int i = 0; i < nodes_.size(); ++i)
    {
        if (nodes_[i].id != nodeId) continue;
        nodes_.removeAt(i);
        for (int c = connections_.size() - 1; c >= 0; --c)
            if (connections_[c].fromNode == nodeId || connections_[c].toNode == nodeId) connections_.removeAt(c);
        return true;
    }
    return false;
}

bool GraphDocument::AddConnection(int fromNode, const QString& fromPin, int toNode, const QString& toPin, const QVariantMap& props)
{
    for (const auto& c : connections_)
        if (c.fromNode == fromNode && c.fromPin == fromPin && c.toNode == toNode && c.toPin == toPin) return false;
    connections_.append({fromNode, fromPin, toNode, toPin, props});
    return true;
}

bool GraphDocument::RemoveConnection(int fromNode, const QString& fromPin, int toNode, const QString& toPin)
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

bool GraphDocument::SetConnectionProperties(int fromNode, const QString& fromPin, int toNode, const QString& toPin, const QVariantMap& props)
{
    for (auto& c : connections_)
        if (c.fromNode == fromNode && c.fromPin == fromPin && c.toNode == toNode && c.toPin == toPin)
        {
            c.properties = props;
            return true;
        }
    return false;
}

void GraphDocument::Clear()
{
    nodes_.clear();
    connections_.clear();
    comments_.clear();
    nextNodeId_ = 1;
    nextCommentId_ = 1;
}

int GraphDocument::AddComment(const QRectF& rect, const QString& text, const QColor& color)
{
    GraphCommentData comment;
    comment.id = nextCommentId_++;
    comment.rect = rect.normalized();
    comment.text = text;
    comment.color = color;
    comments_.append(comment);
    return comment.id;
}

bool GraphDocument::RemoveComment(int id)
{
    for (int i = 0; i < comments_.size(); ++i)
        if (comments_[i].id == id)
        {
            comments_.removeAt(i);
            return true;
        }
    return false;
}

const GraphCommentData* GraphDocument::FindComment(int id) const
{
    for (const auto& comment : comments_)
        if (comment.id == id) return &comment;
    return nullptr;
}

GraphCommentData* GraphDocument::FindComment(int id)
{
    for (auto& comment : comments_)
        if (comment.id == id) return &comment;
    return nullptr;
}

const GraphNodeData* GraphDocument::FindNode(int id) const
{
    for (const auto& n : nodes_)
        if (n.id == id) return &n;
    return nullptr;
}

GraphNodeData* GraphDocument::FindNode(int id)
{
    for (auto& n : nodes_)
        if (n.id == id) return &n;
    return nullptr;
}

void GraphDocument::SetSchema(const QString& schema, int schemaVersion)
{
    schema_ = schema;
    schemaVersion_ = schemaVersion;
    format_ = schema.isEmpty() ? Format::Legacy : Format::V1;
    version_ = FormatVersion;
}

QString GraphDocument::ExportToJson() const
{
    const bool legacy = format_ == Format::Legacy;
    QJsonObject root;
    if (legacy)
        root[QStringLiteral("version")] = 1;
    else
    {
        root[QStringLiteral("format")] = QString::fromLatin1(FormatName);
        root[QStringLiteral("version")] = version_;
        root[QStringLiteral("schema")] = schema_;
        root[QStringLiteral("schemaVersion")] = schemaVersion_;
        root[QStringLiteral("variables")] = variables_;
    }

    QJsonArray nodes;
    for (const auto& n : nodes_)
    {
        QJsonObject node;
        node[QStringLiteral("id")] = n.id;
        node[QStringLiteral("type")] = n.type;
        node[QStringLiteral("title")] = n.title;
        node[QStringLiteral("x")] = n.pos.x();
        node[QStringLiteral("y")] = n.pos.y();
        node[QStringLiteral("properties")] = PropertiesToJson(n.properties, legacy);
        Merge(node, n.extra);
        nodes.append(node);
    }
    root[QStringLiteral("nodes")] = nodes;

    QJsonArray connections;
    for (const auto& c : connections_)
    {
        QJsonObject link;
        link[QStringLiteral("fromNode")] = c.fromNode;
        link[QStringLiteral("fromPin")] = c.fromPin;
        link[QStringLiteral("toNode")] = c.toNode;
        link[QStringLiteral("toPin")] = c.toPin;
        if (!c.properties.isEmpty()) link[QStringLiteral("properties")] = PropertiesToJson(c.properties, legacy);
        connections.append(link);
    }
    root[QStringLiteral("connections")] = connections;
    if (!comments_.isEmpty())
    {
        QJsonArray comments;
        for (const auto& comment : comments_)
            comments.append(QJsonObject{{QStringLiteral("id"), comment.id}, {QStringLiteral("x"), comment.rect.x()}, {QStringLiteral("y"), comment.rect.y()},
                                        {QStringLiteral("width"), comment.rect.width()}, {QStringLiteral("height"), comment.rect.height()},
                                        {QStringLiteral("text"), comment.text}, {QStringLiteral("color"), comment.color.name()}});
        root[QStringLiteral("comments")] = comments;
    }
    Merge(root, extra_);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool GraphDocument::ImportFromJson(const QString& jsonString)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
    if (document.isNull() || !document.isObject()) return false;

    const QJsonObject root = document.object();
    const bool v1 = root.value(QStringLiteral("format")).toString() == QLatin1String(FormatName);
    const bool legacy = !v1;

    GraphDocument loaded;
    loaded.format_ = v1 ? Format::V1 : Format::Legacy;
    loaded.version_ = v1 ? root.value(QStringLiteral("version")).toInt(FormatVersion) : FormatVersion;
    loaded.schema_ = v1 ? root.value(QStringLiteral("schema")).toString() : QString();
    loaded.schemaVersion_ = v1 ? root.value(QStringLiteral("schemaVersion")).toInt() : 0;
    loaded.variables_ = v1 ? root.value(QStringLiteral("variables")).toArray() : QJsonArray();
    loaded.extra_ = Unknown(root, v1 ? RootKeys : LegacyRootKeys);

    for (const auto& value : root.value(QStringLiteral("nodes")).toArray())
    {
        const QJsonObject object = value.toObject();
        GraphNodeData node;
        node.id = object.value(QStringLiteral("id")).toInt();
        node.type = object.value(QStringLiteral("type")).toString();
        node.title = object.value(QStringLiteral("title")).toString();
        node.pos = QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
        node.properties = PropertiesFromJson(object.value(QStringLiteral("properties")).toObject(), legacy);
        node.extra = Unknown(object, NodeKeys);
        loaded.nodes_.append(node);
        if (node.id >= loaded.nextNodeId_) loaded.nextNodeId_ = node.id + 1;
    }

    for (const auto& value : root.value(QStringLiteral("connections")).toArray())
    {
        const QJsonObject object = value.toObject();
        GraphConnectionData link;
        link.fromNode = object.value(QStringLiteral("fromNode")).toInt();
        link.fromPin = object.value(QStringLiteral("fromPin")).toString();
        link.toNode = object.value(QStringLiteral("toNode")).toInt();
        link.toPin = object.value(QStringLiteral("toPin")).toString();
        link.properties = PropertiesFromJson(object.value(QStringLiteral("properties")).toObject(), legacy);
        loaded.connections_.append(link);
    }

    for (const auto& value : root.value(QStringLiteral("comments")).toArray())
    {
        const QJsonObject object = value.toObject();
        GraphCommentData comment;
        comment.id = object.value(QStringLiteral("id")).toInt();
        comment.rect = QRectF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble(),
                              object.value(QStringLiteral("width")).toDouble(200), object.value(QStringLiteral("height")).toDouble(120)).normalized();
        comment.text = object.value(QStringLiteral("text")).toString();
        const QColor color(object.value(QStringLiteral("color")).toString());
        if (color.isValid()) comment.color = color;
        if (comment.id <= 0) comment.id = loaded.nextCommentId_;
        loaded.comments_.append(comment);
        loaded.nextCommentId_ = std::max(loaded.nextCommentId_, comment.id + 1);
    }

    // Наследник (MaterialGraphModel) не добавляет полей: присваивание базовой части полное.
    GraphDocument::operator=(loaded);
    return true;
}

} // namespace EditorCore
