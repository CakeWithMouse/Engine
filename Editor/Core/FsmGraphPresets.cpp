#include "FsmGraphPresets.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>

namespace EditorCore
{

GraphDocument FsmGraphPresets::BuildIdlePatrol()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("ai.fsm"), 1);

    QJsonArray vars{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("PatrolPoint")},
            {QStringLiteral("type"), QStringLiteral("vec3")},
            {QStringLiteral("value"), QJsonArray{0.0, 0.0, 0.0}},
            {QStringLiteral("public"), true}
        }
    };
    doc.SetVariables(vars);

    QVariantMap idleProps;
    idleProps[QStringLiteral("name")] = QStringLiteral("Idle");
    idleProps[QStringLiteral("tasks")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.Wait")}, {QStringLiteral("seconds"), 2.0}}
    };
    const int idle = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Idle"), QPointF(100, 100), idleProps);

    QVariantMap patrolProps;
    patrolProps[QStringLiteral("name")] = QStringLiteral("Patrol");
    patrolProps[QStringLiteral("tasks")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.MoveTo")},
                    {QStringLiteral("target"), QStringLiteral("PatrolPoint")},
                    {QStringLiteral("radius"), 1.0}}
    };
    const int patrol = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Patrol"), QPointF(420, 100), patrolProps);

    QVariantMap transProps;
    transProps[QStringLiteral("trigger")] = QStringLiteral("completed");

    doc.AddConnection(idle, QStringLiteral("out"), patrol, QStringLiteral("in"), transProps);
    doc.AddConnection(patrol, QStringLiteral("out"), idle, QStringLiteral("in"), transProps);

    return doc;
}

GraphDocument FsmGraphPresets::BuildIdleChase()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("ai.fsm"), 1);

    QJsonArray vars{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Target")},
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("public"), true}
        }
    };
    doc.SetVariables(vars);

    QVariantMap idleProps;
    idleProps[QStringLiteral("name")] = QStringLiteral("Idle");
    idleProps[QStringLiteral("tasks")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.Wait")}, {QStringLiteral("seconds"), 1.0}}
    };
    const int idle = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Idle"), QPointF(100, 100), idleProps);

    QVariantMap chaseProps;
    chaseProps[QStringLiteral("name")] = QStringLiteral("Chase");
    chaseProps[QStringLiteral("tasks")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.MoveTo")},
                    {QStringLiteral("target"), QStringLiteral("Target")},
                    {QStringLiteral("radius"), 1.5}}
    };
    const int chase = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Chase"), QPointF(420, 100), chaseProps);

    QVariantMap seeTarget;
    seeTarget[QStringLiteral("trigger")] = QStringLiteral("tick");
    seeTarget[QStringLiteral("conditions")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.CanSee")}, {QStringLiteral("target"), QStringLiteral("Target")}}
    };

    QVariantMap loseTarget;
    loseTarget[QStringLiteral("trigger")] = QStringLiteral("tick");
    loseTarget[QStringLiteral("conditions")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.CanSee")}, {QStringLiteral("target"), QStringLiteral("Target")}, {QStringLiteral("not"), true}}
    };

    doc.AddConnection(idle, QStringLiteral("out"), chase, QStringLiteral("in"), seeTarget);
    doc.AddConnection(chase, QStringLiteral("out"), idle, QStringLiteral("in"), loseTarget);

    return doc;
}

GraphDocument FsmGraphPresets::BuildIdleAttack()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("ai.fsm"), 1);

    QJsonArray vars{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Target")},
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("public"), true}
        }
    };
    doc.SetVariables(vars);

    QVariantMap idleProps;
    idleProps[QStringLiteral("name")] = QStringLiteral("Idle");
    idleProps[QStringLiteral("tasks")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.Wait")}, {QStringLiteral("seconds"), 1.0}}
    };
    const int idle = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Idle"), QPointF(100, 100), idleProps);

    QVariantMap attackProps;
    attackProps[QStringLiteral("name")] = QStringLiteral("Attack");
    attackProps[QStringLiteral("tasks")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.LookAt")}, {QStringLiteral("target"), QStringLiteral("Target")}},
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.Wait")}, {QStringLiteral("seconds"), 1.0}}
    };
    const int attack = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Attack"), QPointF(420, 100), attackProps);

    QVariantMap nearTarget;
    nearTarget[QStringLiteral("trigger")] = QStringLiteral("tick");
    nearTarget[QStringLiteral("conditions")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.Near")}, {QStringLiteral("target"), QStringLiteral("Target")}, {QStringLiteral("distance"), 2.0}}
    };

    QVariantMap doneAttack;
    doneAttack[QStringLiteral("trigger")] = QStringLiteral("completed");

    doc.AddConnection(idle, QStringLiteral("out"), attack, QStringLiteral("in"), nearTarget);
    doc.AddConnection(attack, QStringLiteral("out"), idle, QStringLiteral("in"), doneAttack);

    return doc;
}

} // namespace EditorCore
