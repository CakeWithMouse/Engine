#include "ScriptGraphPresets.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>

namespace EditorCore
{

GraphDocument ScriptGraphPresets::BuildEmpty()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("code"), 1);
    return doc;
}

GraphDocument ScriptGraphPresets::BuildStartPrint()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("code"), 1);

    const int start = doc.AddNode(QStringLiteral("Event.BeginPlay"), QStringLiteral("Начало игры"), QPointF(100, 150));
    const int print = doc.AddNode(QStringLiteral("Action.Print"), QStringLiteral("Вывести в консоль"), QPointF(400, 150));

    doc.AddConnection(start, QStringLiteral("then"), print, QStringLiteral("exec"));
    return doc;
}

GraphDocument ScriptGraphPresets::BuildTickRotate()
{
    GraphDocument doc;
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

GraphDocument ScriptGraphPresets::BuildSinMoveY()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("code"), 1);

    const int tick = doc.AddNode(QStringLiteral("Event.Tick"), QStringLiteral("Каждый кадр"), QPointF(80, 100));
    const int time = doc.AddNode(QStringLiteral("Math.Time"), QStringLiteral("Время"), QPointF(80, 240));
    const int sin = doc.AddNode(QStringLiteral("Math.Sin"), QStringLiteral("Синус"), QPointF(280, 240));
    const int makeVec = doc.AddNode(QStringLiteral("Vec3.Make"), QStringLiteral("Собрать вектор"), QPointF(480, 220));
    const int setPos = doc.AddNode(QStringLiteral("Action.SetPosition"), QStringLiteral("Задать позицию"), QPointF(700, 100));

    doc.AddConnection(tick, QStringLiteral("then"), setPos, QStringLiteral("exec"));
    doc.AddConnection(time, QStringLiteral("Out"), sin, QStringLiteral("In"));
    doc.AddConnection(sin, QStringLiteral("Out"), makeVec, QStringLiteral("Y"));
    doc.AddConnection(makeVec, QStringLiteral("Out"), setPos, QStringLiteral("Position"));

    return doc;
}

GraphDocument ScriptGraphPresets::BuildTimerAction()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("code"), 1);

    const int start = doc.AddNode(QStringLiteral("Event.BeginPlay"), QStringLiteral("Начало игры"), QPointF(80, 100));

    QVariantMap floatProps;
    floatProps[QStringLiteral("value")] = 3.0;
    const int delayConst = doc.AddNode(QStringLiteral("Const.Float"), QStringLiteral("Задержка (сек)"), QPointF(80, 240), floatProps);

    const int delay = doc.AddNode(QStringLiteral("Flow.Delay"), QStringLiteral("Задержка"), QPointF(320, 100));

    QVariantMap strProps;
    strProps[QStringLiteral("value")] = QStringLiteral("Таймер сработал!");
    const int msgConst = doc.AddNode(QStringLiteral("Const.String"), QStringLiteral("Сообщение"), QPointF(320, 240), strProps);

    const int print = doc.AddNode(QStringLiteral("Action.Print"), QStringLiteral("Вывести в консоль"), QPointF(580, 100));

    doc.AddConnection(start, QStringLiteral("then"), delay, QStringLiteral("exec"));
    doc.AddConnection(delayConst, QStringLiteral("Value"), delay, QStringLiteral("Seconds"));
    doc.AddConnection(delay, QStringLiteral("then"), print, QStringLiteral("exec"));
    doc.AddConnection(msgConst, QStringLiteral("Value"), print, QStringLiteral("Value"));

    return doc;
}

GraphDocument ScriptGraphPresets::BuildCounterVariable()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("code"), 1);

    QJsonArray vars{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Counter")},
            {QStringLiteral("type"), QStringLiteral("float")},
            {QStringLiteral("value"), 0.0},
            {QStringLiteral("public"), true}
        }
    };
    doc.SetVariables(vars);

    const int tick = doc.AddNode(QStringLiteral("Event.Tick"), QStringLiteral("Каждый кадр"), QPointF(80, 100));

    QVariantMap getProps;
    getProps[QStringLiteral("name")] = QStringLiteral("Counter");
    const int getVar = doc.AddNode(QStringLiteral("Variable.Get"), QStringLiteral("Получить Counter"), QPointF(80, 240), getProps);

    QVariantMap oneProps;
    oneProps[QStringLiteral("value")] = 1.0;
    const int constOne = doc.AddNode(QStringLiteral("Const.Float"), QStringLiteral("Шаг (+1)"), QPointF(80, 360), oneProps);

    const int add = doc.AddNode(QStringLiteral("Math.Add"), QStringLiteral("Сложение"), QPointF(320, 260));

    QVariantMap setProps;
    setProps[QStringLiteral("name")] = QStringLiteral("Counter");
    const int setVar = doc.AddNode(QStringLiteral("Variable.Set"), QStringLiteral("Задать Counter"), QPointF(560, 100), setProps);

    doc.AddConnection(tick, QStringLiteral("then"), setVar, QStringLiteral("exec"));
    doc.AddConnection(getVar, QStringLiteral("Value"), add, QStringLiteral("A"));
    doc.AddConnection(constOne, QStringLiteral("Value"), add, QStringLiteral("B"));
    doc.AddConnection(add, QStringLiteral("Out"), setVar, QStringLiteral("Value"));

    return doc;
}

GraphDocument ScriptGraphPresets::BuildConditionThreshold()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("code"), 1);

    QJsonArray vars{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Score")},
            {QStringLiteral("type"), QStringLiteral("float")},
            {QStringLiteral("value"), 10.0},
            {QStringLiteral("public"), true}
        }
    };
    doc.SetVariables(vars);

    const int tick = doc.AddNode(QStringLiteral("Event.Tick"), QStringLiteral("Каждый кадр"), QPointF(80, 100));

    QVariantMap getProps;
    getProps[QStringLiteral("name")] = QStringLiteral("Score");
    const int getVar = doc.AddNode(QStringLiteral("Variable.Get"), QStringLiteral("Получить Score"), QPointF(80, 240), getProps);

    QVariantMap threshProps;
    threshProps[QStringLiteral("value")] = 5.0;
    const int threshConst = doc.AddNode(QStringLiteral("Const.Float"), QStringLiteral("Порог (5.0)"), QPointF(80, 360), threshProps);

    const int greater = doc.AddNode(QStringLiteral("Logic.Greater"), QStringLiteral("Больше"), QPointF(320, 260));
    const int branch = doc.AddNode(QStringLiteral("Flow.Branch"), QStringLiteral("Ветвление"), QPointF(540, 100));

    QVariantMap strProps;
    strProps[QStringLiteral("value")] = QStringLiteral("Порог превышен!");
    const int msgConst = doc.AddNode(QStringLiteral("Const.String"), QStringLiteral("Сообщение"), QPointF(540, 260), strProps);

    const int print = doc.AddNode(QStringLiteral("Action.Print"), QStringLiteral("Вывести в консоль"), QPointF(780, 100));

    doc.AddConnection(tick, QStringLiteral("then"), branch, QStringLiteral("exec"));
    doc.AddConnection(getVar, QStringLiteral("Value"), greater, QStringLiteral("A"));
    doc.AddConnection(threshConst, QStringLiteral("Value"), greater, QStringLiteral("B"));
    doc.AddConnection(greater, QStringLiteral("Out"), branch, QStringLiteral("Condition"));
    doc.AddConnection(branch, QStringLiteral("True"), print, QStringLiteral("exec"));
    doc.AddConnection(msgConst, QStringLiteral("Value"), print, QStringLiteral("Value"));

    return doc;
}

} // namespace EditorCore
