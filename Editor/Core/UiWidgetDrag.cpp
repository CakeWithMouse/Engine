#include "UiWidgetDrag.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace EditorCore
{
namespace UiWidgetDrag
{
    QByteArray Encode(const Payload& payload)
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("type"), payload.type);
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    bool Decode(const QByteArray& data, Payload& outPayload)
    {
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject())
            return false;
        const QJsonValue type = doc.object().value(QStringLiteral("type"));
        if (!type.isString())
            return false;
        const QString value = type.toString();
        if (value != QLatin1String("Panel") && value != QLatin1String("Label") &&
            value != QLatin1String("Button") && value != QLatin1String("Bar") &&
            value != QLatin1String("Group"))
            return false;
        outPayload.type = value;
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