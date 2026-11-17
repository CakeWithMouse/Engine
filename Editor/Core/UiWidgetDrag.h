#pragma once

#include <QByteArray>
#include <QMimeData>
#include <QString>

namespace EditorCore
{
// MIME перетаскивания типа виджета из палитры UI-редактора на холст. Полезная нагрузка — JSON
// (QJsonDocument) с именем типа (Panel/Label/Button/Bar/Group). Drop создаёт элемент внутри
// выбранной панели в точке броска (правило DropRules UiPalette -> UiCanvas, "UI.AddWidget").
namespace UiWidgetDrag
{
    inline constexpr auto kMimeType = "application/x-painengine-ui-widget";

    struct Payload
    {
        QString type;
    };

    QByteArray Encode(const Payload& payload);
    // false: не JSON-объект или тип не входит в набор движковых виджетов.
    bool Decode(const QByteArray& data, Payload& outPayload);

    inline bool HasMime(const QMimeData& mime)
    {
        return mime.hasFormat(QLatin1String(kMimeType));
    }

    // Пустой payload, если MIME нет или данные битые.
    Payload FromMime(const QMimeData& mime);
}
}