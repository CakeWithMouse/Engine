#pragma once

#include <QByteArray>
#include <QMimeData>
#include <QString>

namespace EditorCore
{
// MIME перетаскивания переменной скрипта из панели «Переменные» на холст графа. Полезная нагрузка —
// JSON (QJsonDocument): имя и тип переменной. Drop создаёт ноду Variable.Get/Set (правило DropRules
// GraphVariable -> GraphCanvas). Отдельный от AssetDrag формат: переменная — не ресурс /Game.
namespace ScriptVariableDrag
{
    inline constexpr auto kMimeType = "application/x-painengine-script-variable";

    struct Payload
    {
        QString name, type;
    };

    QByteArray Encode(const Payload& payload);
    // false: не JSON-объект или имя пусто (без имени нода переменной бессмысленна).
    bool Decode(const QByteArray& data, Payload& outPayload);

    inline bool HasMime(const QMimeData& mime)
    {
        return mime.hasFormat(QLatin1String(kMimeType));
    }

    // Пустой payload, если MIME нет или данные битые.
    Payload FromMime(const QMimeData& mime);
}
}