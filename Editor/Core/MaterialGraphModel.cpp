#include "MaterialGraphModel.h"

namespace EditorCore
{

bool MaterialGraphModel::AddConnection(int fromNode, const QString& fromPin, int toNode, const QString& toPin)
{
    for (const auto& c : Connections())
        if (c.toNode == toNode && c.toPin == toPin) return false;
    return GraphDocument::AddConnection(fromNode, fromPin, toNode, toPin);
}

void MaterialGraphModel::SetupDefaultMasterNode()
{
    Clear();

    // Выход материала: поля MaterialOutput освещения движка (Блинн-Фонг)
    AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(560, 180));

    // ColorParam BaseColor
    QVariantMap colorProps;
    colorProps[QStringLiteral("name")] = QStringLiteral("BaseColor");
    colorProps[QStringLiteral("color")] = QColor(220, 220, 225);
    AddNode(QStringLiteral("ColorParam"), QStringLiteral("BaseColor"), QPointF(200, 120), colorProps);

    // ScalarParam Specular
    QVariantMap specularProps;
    specularProps[QStringLiteral("name")] = QStringLiteral("Specular");
    specularProps[QStringLiteral("value")] = 0.5;
    AddNode(QStringLiteral("ScalarParam"), QStringLiteral("Specular"), QPointF(200, 280), specularProps);

    // Connect BaseColor.RGB -> MaterialOutput.Albedo
    AddConnection(2, QStringLiteral("RGB"), 1, QStringLiteral("Albedo"));
    // Connect Specular -> MaterialOutput.Specular
    AddConnection(3, QStringLiteral("Value"), 1, QStringLiteral("Specular"));
}

} // namespace EditorCore
