#pragma once

#include "AssetExtensions.h"

#include <QByteArray>
#include <QList>
#include <QMimeData>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace EditorCore
{
// Единый MIME-формат перетаскивания ресурсов редактора. Полезная нагрузка — JSON
// (QJsonDocument): список виртуальных путей "/Game/...", тип ресурса и источник.
// Внешние файлы (Проводник) приходят как text/uri-list и нормализуются в пути.
namespace AssetDrag
{
    inline constexpr auto kMimeType = "application/x-painengine-assets";

    struct Payload
    {
        QStringList virtualPaths;
        AssetKind kind = AssetKind::Unknown;
        QString source;
    };

    QByteArray Encode(const Payload& payload);
    bool Decode(const QByteArray& data, Payload& outPayload);

    QStringList LocalPathsFromUrls(const QList<QUrl>& urls);

    inline bool HasAssetMime(const QMimeData& mime)
    {
        return mime.hasFormat(QLatin1String(kMimeType));
    }

    // Наш MIME — декодируем; иначе uri-list внешних файлов (kind по расширению первого файла).
    Payload FromMime(const QMimeData& mime, const QString& contentRoot = QString());
}
}