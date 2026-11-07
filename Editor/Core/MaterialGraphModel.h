#pragma once

#include "Graph/GraphDocument.h"

#include <QColor>

namespace EditorCore
{

// Граф материала: документ в формате Legacy (*.mat.graph.json) с правилами прежней модели.
// Полей сверх GraphDocument не добавляет, поэтому свободно приводится к нему и обратно копированием.
class MaterialGraphModel : public GraphDocument
{
public:
    MaterialGraphModel() = default;
    MaterialGraphModel(const GraphDocument& document) : GraphDocument(document) {}

    // У входа материала одна связь: занятый вход отклоняет новую.
    bool AddConnection(int fromNode, const QString& fromPin, int toNode, const QString& toPin);

    void SetupDefaultMasterNode();
};

} // namespace EditorCore
