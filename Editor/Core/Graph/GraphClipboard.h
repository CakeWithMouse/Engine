#pragma once

#include "GraphDocument.h"

#include <QByteArray>
#include <QList>
#include <functional>

namespace EditorCore
{

// Ноды в буфере обмена: тот же JSON, что у файла графа, с ключом-меткой схемы. Вставка узнаёт его
// и в обычном тексте, а метка не даёт вставить ноды одной схемы в граф другой.
class GraphClipboard
{
public:
    // Выбранные ноды и связи между ними. copyable отсеивает обязательные ноды. Пусто — копировать нечего.
    static QString Encode(const GraphDocument& document, const QList<int>& ids, const QString& marker,
                          const std::function<bool(const GraphNodeData&)>& copyable);
    // JSON нод этой схемы или пустая строка.
    static QString Decode(const QByteArray& data, const QString& marker);
};

} // namespace EditorCore
