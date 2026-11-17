#include "AssetDrag.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace EditorCore
{
namespace AssetDrag
{
    QByteArray Encode(const Payload& payload)
    {
        QJsonObject obj;
        QJsonArray paths;
        for (const QString& path : payload.virtualPaths)
            paths.append(path);
        obj.insert(QStringLiteral("paths"), paths);
        obj.insert(QStringLiteral("kind"), static_cast<int>(payload.kind));
        obj.insert(QStringLiteral("source"), payload.source);
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    bool Decode(const QByteArray& data, Payload& outPayload)
    {
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject())
            return false;
        const QJsonObject obj = doc.object();
        const QJsonValue pathsValue = obj.value(QStringLiteral("paths"));
        if (!pathsValue.isArray())
            return false;

        Payload payload;
        for (const QJsonValue& value : pathsValue.toArray())
        {
            if (!value.isString())
                return false;
            payload.virtualPaths.append(value.toString());
        }
        const int kind = obj.value(QStringLiteral("kind")).toInt(static_cast<int>(AssetKind::Unknown));
        payload.kind = (kind >= static_cast<int>(AssetKind::Model) && kind <= static_cast<int>(AssetKind::Unknown))
                           ? static_cast<AssetKind>(kind)
                           : AssetKind::Unknown;
        payload.source = obj.value(QStringLiteral("source")).toString();
        outPayload = payload;
        return true;
    }

    QStringList LocalPathsFromUrls(const QList<QUrl>& urls)
    {
        QStringList paths;
        for (const QUrl& url : urls)
        {
            if (url.isLocalFile())
                paths.append(url.toLocalFile());
        }
        return paths;
    }

    Payload FromMime(const QMimeData& mime, const QString& contentRoot)
    {
        if (HasAssetMime(mime))
        {
            Payload payload;
            if (Decode(mime.data(QLatin1String(kMimeType)), payload))
                return payload;
            return {};
        }

        Payload payload;
        payload.source = QStringLiteral("Explorer");
        const QStringList localPaths = LocalPathsFromUrls(mime.urls());
        for (const QString& path : localPaths)
            payload.virtualPaths.append(NormalizeVirtualPath(path, contentRoot));
        if (!localPaths.isEmpty())
            payload.kind = KindForFile(localPaths.first());
        return payload;
    }
}
}