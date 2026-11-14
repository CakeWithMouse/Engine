#include "GraphClipboard.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace EditorCore
{

QString GraphClipboard::Encode(const GraphDocument& document, const QList<int>& ids, const QString& marker,
                               const std::function<bool(const GraphNodeData&)>& copyable)
{
    const QJsonObject graph = QJsonDocument::fromJson(document.ExportToJson().toUtf8()).object();
    QJsonArray nodes, connections;
    QSet<int> chosen;
    for (const auto& value : graph.value(QStringLiteral("nodes")).toArray())
    {
        const QJsonObject node = value.toObject();
        const int id = node.value(QStringLiteral("id")).toInt();
        const GraphNodeData* data = document.FindNode(id);
        if (!ids.contains(id) || !data || (copyable && !copyable(*data))) continue;
        nodes.append(node);
        chosen.insert(id);
    }
    if (nodes.isEmpty()) return {};
    for (const auto& value : graph.value(QStringLiteral("connections")).toArray())
    {
        const QJsonObject link = value.toObject();
        if (chosen.contains(link.value(QStringLiteral("fromNode")).toInt()) && chosen.contains(link.value(QStringLiteral("toNode")).toInt()))
            connections.append(link);
    }
    QJsonObject clip;
    clip[marker] = 1;
    // Заголовок V1 едет с нодами: вставка читает значения так же, как файл (без цветов Legacy).
    for (const char* key : {"format", "version", "schema", "schemaVersion"})
        if (document.FileFormat() == GraphDocument::Format::V1 && graph.contains(QLatin1String(key)))
            clip[QLatin1String(key)] = graph.value(QLatin1String(key));
    clip[QStringLiteral("nodes")] = nodes;
    clip[QStringLiteral("connections")] = connections;
    return QString::fromUtf8(QJsonDocument(clip).toJson(QJsonDocument::Indented));
}

QString GraphClipboard::Decode(const QByteArray& data, const QString& marker)
{
    const QJsonDocument document = QJsonDocument::fromJson(data);
    return document.isObject() && document.object().contains(marker) ? QString::fromUtf8(data) : QString();
}

} // namespace EditorCore
