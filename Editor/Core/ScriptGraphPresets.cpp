#include "ScriptGraphPresets.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>

namespace EditorCore
{

GraphDocument ScriptGraphPresets::BuildEmpty()
{
    GraphDocument doc;
    doc.SetFileFormat(GraphDocument::Format::V1);
    doc.SetSchema(QStringLiteral("code"), 1);
    return doc;
}

GraphDocument ScriptGraphPresets::BuildStartPrint()
{
    GraphDocument doc;
    doc.SetFileFormat(GraphDocument::Format::V1);
    doc.SetSchema(QStringLiteral("code"), 1);

    const int start = doc.AddNode(QStringLiteral("Event.BeginPlay"), QStringLiteral("Начало игры"), QPointF(100, 150));
    const int print = doc.AddNode(QStringLiteral("Action.Print"), QStringLiteral("Вывести в консоль"), QPointF(400, 150));

    doc.AddConnection(start, QStringLiteral("then"), print, QStringLiteral("exec"));
    return doc;
}

GraphDocument ScriptGraphPresets::BuildTickRotate()
{
    GraphDocument doc;
    doc.SetFileFormat(GraphDocument::Format::V1);
    doc.SetSchema(QStringLiteral("code"), 1);

    const int tick = doc.AddNode(QStringLiteral("Event.Tick"), QStringLiteral("Каждый кадр"), QPointF(80, 120));

    QVariantMap vecProps;
    vecProps[QStringLiteral("value")] = QJsonArray{0.0, 45.0, 0.0};
    const int vec = doc.AddNode(QStringLiteral("Const.Vec3"), QStringLiteral("Вектор"), QPointF(80, 260), vecProps);

    const int scale = doc.AddNode(QStringLiteral("Vec3.Scale"), QStringLiteral("Вектор × число"), QPointF(340, 220));
    const int rotate = doc.AddNode(QStringLiteral("Action.Rotate"), QStringLiteral("Повернуть"), QPointF(580, 120));

    doc.AddConnection(tick, QStringLiteral("then"), rotate, QStringLiteral("exec"));
    doc.AddConnection(vec, QStringLiteral("Value"), scale, QStringLiteral("Vector"));
    doc.AddConnection(tick, QStringLiteral("DeltaTime"), scale, QStringLiteral("Factor"));
    doc.AddConnection(scale, QStringLiteral("Out"), rotate, QStringLiteral("Degrees"));

    return doc;
}

} // namespace EditorCore
