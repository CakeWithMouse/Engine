#include "ScriptVariableDrag.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace EditorCore
{
namespace ScriptVariableDrag
{
    QByteArray Encode(const Payload& payload)
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), payload.name);
        obj.insert(QStringLiteral("type"), payload.type);
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    bool Decode(const QByteArray& data, Payload& outPayload)
    {
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject())
            return false;
        const QJsonObject obj = doc.object();
        const QJsonValue name = obj.value(QStringLiteral("name"));
        if (!name.isString() || name.toString().isEmpty())
            return false;
        Payload payload;
        payload.name = name.toString();
        payload.type = obj.value(QStringLiteral("type")).toString();
        outPayload = payload;
        return true;
    }

    Payload FromMime(const QMimeData& mime)
    {
        Payload payload;
        if (HasMime(mime))
            Decode(mime.data(QLatin1String(kMimeType)), payload);
        return payload;
    }
}
}