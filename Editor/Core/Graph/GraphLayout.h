#pragma once

#include "GraphSchema.h"

#include <QHash>
#include <QPointF>
#include <QSizeF>
#include <functional>

namespace EditorCore
{

// Автораскладка графа («Упорядочить»): новые позиции левых верхних углов нод.
//  * Layered — слоями слева направо по связям: нода правее всех, от кого берёт данные или поток.
//    Ноды без входящих связей (константы, «Сам объект») встают перед первым потребителем.
//    Порядок внутри слоя — по средним положениям соседей; обратные связи циклов не учитываются.
//  * Tree — дерево сверху вниз: дети под родителем в порядке их X, родитель по центру над детьми.
//    У ноды с несколькими родителями учитывается первый; ноды без родителя — отдельные корни.
// Граф остаётся на месте: левый верхний угол результата совпадает с прежним, позиции кратны grid.
using GraphNodeSize = std::function<QSizeF(const GraphNodeData&)>;
QHash<int, QPointF> ArrangeGraph(const GraphDocument& document, GraphLayoutStyle style, const GraphNodeSize& size, qreal grid = 20.0);

} // namespace EditorCore
