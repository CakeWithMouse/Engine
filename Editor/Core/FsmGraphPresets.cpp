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

GraphDocument FsmGraphPresets::BuildPatrolChaseReturn()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("ai.fsm"), 1);

    QJsonArray vars{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Target")},
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("public"), true}
        },
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("PatrolPoint")},
            {QStringLiteral("type"), QStringLiteral("vec3")},
            {QStringLiteral("value"), QJsonArray{5.0, 0.0, 0.0}},
            {QStringLiteral("public"), true}
        },
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("HomePoint")},
            {QStringLiteral("type"), QStringLiteral("vec3")},
            {QStringLiteral("value"), QJsonArray{0.0, 0.0, 0.0}},
            {QStringLiteral("public"), true}
        }
    };
    doc.SetVariables(vars);

    QVariantMap patrolProps;
    patrolProps[QStringLiteral("name")] = QStringLiteral("Patrol");
    patrolProps[QStringLiteral("tasks")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.MoveTo")},
                    {QStringLiteral("target"), QStringLiteral("PatrolPoint")},
                    {QStringLiteral("radius"), 1.0}},
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.Wait")},
                    {QStringLiteral("seconds"), 2.0}}
    };
    const int patrol = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Patrol"), QPointF(80, 100), patrolProps);

    QVariantMap chaseProps;
    chaseProps[QStringLiteral("name")] = QStringLiteral("Chase");
    chaseProps[QStringLiteral("tasks")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.MoveTo")},
                    {QStringLiteral("target"), QStringLiteral("Target")},
                    {QStringLiteral("radius"), 1.5}}
    };
    const int chase = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Chase"), QPointF(380, 100), chaseProps);

    QVariantMap returnProps;
    returnProps[QStringLiteral("name")] = QStringLiteral("Return");
    returnProps[QStringLiteral("tasks")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.MoveTo")},
                    {QStringLiteral("target"), QStringLiteral("HomePoint")},
                    {QStringLiteral("radius"), 1.0}}
    };
    const int returnState = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Return"), QPointF(680, 100), returnProps);

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

    QVariantMap doneReturn;
    doneReturn[QStringLiteral("trigger")] = QStringLiteral("completed");

    doc.AddConnection(patrol, QStringLiteral("out"), chase, QStringLiteral("in"), seeTarget);
    doc.AddConnection(chase, QStringLiteral("out"), returnState, QStringLiteral("in"), loseTarget);
    doc.AddConnection(returnState, QStringLiteral("out"), patrol, QStringLiteral("in"), doneReturn);

    return doc;
}

GraphDocument FsmGraphPresets::BuildGuardAttackDistance()
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
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.Wait")}, {QStringLiteral("seconds"), 0.5}}
    };
    const int attack = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Attack"), QPointF(420, 100), attackProps);

    QVariantMap nearTarget;
    nearTarget[QStringLiteral("trigger")] = QStringLiteral("tick");
    nearTarget[QStringLiteral("conditions")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.Near")}, {QStringLiteral("target"), QStringLiteral("Target")}, {QStringLiteral("distance"), 3.0}}
    };

    QVariantMap farTarget;
    farTarget[QStringLiteral("trigger")] = QStringLiteral("tick");
    farTarget[QStringLiteral("conditions")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.Near")}, {QStringLiteral("target"), QStringLiteral("Target")}, {QStringLiteral("distance"), 3.0}, {QStringLiteral("not"), true}}
    };

    doc.AddConnection(idle, QStringLiteral("out"), attack, QStringLiteral("in"), nearTarget);
    doc.AddConnection(attack, QStringLiteral("out"), idle, QStringLiteral("in"), farTarget);

    return doc;
}

GraphDocument FsmGraphPresets::BuildIdleFlee()
{
    GraphDocument doc;
    doc.SetSchema(QStringLiteral("ai.fsm"), 1);

    QJsonArray vars{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Threat")},
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("public"), true}
        },
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("SafePoint")},
            {QStringLiteral("type"), QStringLiteral("vec3")},
            {QStringLiteral("value"), QJsonArray{10.0, 0.0, 10.0}},
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

    QVariantMap fleeProps;
    fleeProps[QStringLiteral("name")] = QStringLiteral("Flee");
    fleeProps[QStringLiteral("tasks")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.MoveTo")},
                    {QStringLiteral("target"), QStringLiteral("SafePoint")},
                    {QStringLiteral("radius"), 1.0}}
    };
    const int flee = doc.AddNode(QStringLiteral("Fsm.State"), QStringLiteral("Flee"), QPointF(420, 100), fleeProps);

    QVariantMap threatNear;
    threatNear[QStringLiteral("trigger")] = QStringLiteral("tick");
    threatNear[QStringLiteral("conditions")] = QVariantList{
        QVariantMap{{QStringLiteral("type"), QStringLiteral("Ai.Near")}, {QStringLiteral("target"), QStringLiteral("Threat")}, {QStringLiteral("distance"), 5.0}}
    };

    QVariantMap safeReached;
    safeReached[QStringLiteral("trigger")] = QStringLiteral("completed");

    doc.AddConnection(idle, QStringLiteral("out"), flee, QStringLiteral("in"), threatNear);
    doc.AddConnection(flee, QStringLiteral("out"), idle, QStringLiteral("in"), safeReached);

    return doc;
}

} // namespace EditorCore
