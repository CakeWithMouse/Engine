#pragma once

#include "../../Core/ScriptGraphPresets.h"
#include "../../Core/FsmGraphPresets.h"
#include "../../Core/FsmGraphSchema.h"

#include <cstdio>
#include <cstdlib>

namespace
{

int gGraphPresetChecks = 0;

void CheckGraphPreset(bool cond, const char* msg)
{
    ++gGraphPresetChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (GraphPresets): %s\n", msg);
        std::exit(1);
    }
}

const EditorCore::GraphNodeData* FindGraphNode(const EditorCore::GraphDocument& doc, const QString& type)
{
    for (const auto& n : doc.Nodes())
        if (n.type == type) return &n;
    return nullptr;
}

bool HasPresetConnection(const EditorCore::GraphDocument& doc, int from, const QString& fromPin, int to, const QString& toPin)
{
    for (const auto& c : doc.Connections())
        if (c.fromNode == from && c.fromPin == fromPin && c.toNode == to && c.toPin == toPin) return true;
    return false;
}

void TestScriptPresets()
{
    using namespace EditorCore;

    // 1. Empty
    const GraphDocument emptyDoc = ScriptGraphPresets::BuildEmpty();
    CheckGraphPreset(emptyDoc.FileFormat() == GraphDocument::Format::V1, "Script Empty format V1");
    CheckGraphPreset(emptyDoc.Schema() == QStringLiteral("code"), "Script Empty schema code");
    CheckGraphPreset(emptyDoc.Nodes().isEmpty(), "Script Empty has no nodes");
    CheckGraphPreset(emptyDoc.Connections().isEmpty(), "Script Empty has no connections");

    // 2. StartPrint
    const GraphDocument startDoc = ScriptGraphPresets::BuildStartPrint();
    const auto* startNode = FindGraphNode(startDoc, QStringLiteral("Event.BeginPlay"));
    const auto* printNode = FindGraphNode(startDoc, QStringLiteral("Action.Print"));
    CheckGraphPreset(startNode && printNode, "StartPrint has BeginPlay and Print nodes");
    CheckGraphPreset(HasPresetConnection(startDoc, startNode->id, QStringLiteral("then"), printNode->id, QStringLiteral("exec")),
                "StartPrint then -> exec connected");

    // 3. TickRotate
    const GraphDocument tickDoc = ScriptGraphPresets::BuildTickRotate();
    const auto* tickNode = FindGraphNode(tickDoc, QStringLiteral("Event.Tick"));
    const auto* vecNode = FindGraphNode(tickDoc, QStringLiteral("Const.Vec3"));
    const auto* scaleNode = FindGraphNode(tickDoc, QStringLiteral("Vec3.Scale"));
    const auto* rotNode = FindGraphNode(tickDoc, QStringLiteral("Action.Rotate"));
    CheckGraphPreset(tickNode && vecNode && scaleNode && rotNode, "TickRotate has all 4 nodes");
    CheckGraphPreset(HasPresetConnection(tickDoc, tickNode->id, QStringLiteral("then"), rotNode->id, QStringLiteral("exec")),
                "TickRotate Tick.then -> Rotate.exec");
    CheckGraphPreset(HasPresetConnection(tickDoc, vecNode->id, QStringLiteral("Value"), scaleNode->id, QStringLiteral("Vector")),
                "TickRotate ConstVec3.Value -> Vec3Scale.Vector");
    CheckGraphPreset(HasPresetConnection(tickDoc, tickNode->id, QStringLiteral("DeltaTime"), scaleNode->id, QStringLiteral("Factor")),
                "TickRotate Tick.DeltaTime -> Vec3Scale.Factor");
    CheckGraphPreset(HasPresetConnection(tickDoc, scaleNode->id, QStringLiteral("Out"), rotNode->id, QStringLiteral("Degrees")),
                "TickRotate Vec3Scale.Out -> Rotate.Degrees");

    // 4. SinMoveY
    const GraphDocument sinDoc = ScriptGraphPresets::BuildSinMoveY();
    CheckGraphPreset(sinDoc.FileFormat() == GraphDocument::Format::V1, "SinMoveY format V1");
    CheckGraphPreset(sinDoc.Schema() == QStringLiteral("code"), "SinMoveY schema code");
    const auto* sinTick = FindGraphNode(sinDoc, QStringLiteral("Event.Tick"));
    const auto* sinTime = FindGraphNode(sinDoc, QStringLiteral("Math.Time"));
    const auto* sinSin = FindGraphNode(sinDoc, QStringLiteral("Math.Sin"));
    const auto* sinVec = FindGraphNode(sinDoc, QStringLiteral("Vec3.Make"));
    const auto* sinPos = FindGraphNode(sinDoc, QStringLiteral("Action.SetPosition"));
    CheckGraphPreset(sinTick && sinTime && sinSin && sinVec && sinPos, "SinMoveY has all 5 nodes");
    CheckGraphPreset(HasPresetConnection(sinDoc, sinTick->id, QStringLiteral("then"), sinPos->id, QStringLiteral("exec")),
                "SinMoveY Tick.then -> SetPos.exec");
    CheckGraphPreset(HasPresetConnection(sinDoc, sinTime->id, QStringLiteral("Out"), sinSin->id, QStringLiteral("In")),
                "SinMoveY Time.Out -> Sin.In");
    CheckGraphPreset(HasPresetConnection(sinDoc, sinSin->id, QStringLiteral("Out"), sinVec->id, QStringLiteral("Y")),
                "SinMoveY Sin.Out -> Vec3Make.Y");
    CheckGraphPreset(HasPresetConnection(sinDoc, sinVec->id, QStringLiteral("Out"), sinPos->id, QStringLiteral("Position")),
                "SinMoveY Vec3Make.Out -> SetPos.Position");

    // 5. TimerAction
    const GraphDocument timerDoc = ScriptGraphPresets::BuildTimerAction();
    const auto* tmStart = FindGraphNode(timerDoc, QStringLiteral("Event.BeginPlay"));
    const auto* tmDelayConst = FindGraphNode(timerDoc, QStringLiteral("Const.Float"));
    const auto* tmDelay = FindGraphNode(timerDoc, QStringLiteral("Flow.Delay"));
    const auto* tmMsg = FindGraphNode(timerDoc, QStringLiteral("Const.String"));
    const auto* tmPrint = FindGraphNode(timerDoc, QStringLiteral("Action.Print"));
    CheckGraphPreset(tmStart && tmDelayConst && tmDelay && tmMsg && tmPrint, "TimerAction has all 5 nodes");
    CheckGraphPreset(HasPresetConnection(timerDoc, tmStart->id, QStringLiteral("then"), tmDelay->id, QStringLiteral("exec")),
                "TimerAction Start.then -> Delay.exec");
    CheckGraphPreset(HasPresetConnection(timerDoc, tmDelayConst->id, QStringLiteral("Value"), tmDelay->id, QStringLiteral("Seconds")),
                "TimerAction DelayConst.Value -> Delay.Seconds");
    CheckGraphPreset(HasPresetConnection(timerDoc, tmDelay->id, QStringLiteral("then"), tmPrint->id, QStringLiteral("exec")),
                "TimerAction Delay.then -> Print.exec");
    CheckGraphPreset(HasPresetConnection(timerDoc, tmMsg->id, QStringLiteral("Value"), tmPrint->id, QStringLiteral("Value")),
                "TimerAction MsgConst.Value -> Print.Value");

    // 6. CounterVariable
    const GraphDocument counterDoc = ScriptGraphPresets::BuildCounterVariable();
    CheckGraphPreset(counterDoc.Variables().size() == 1, "CounterVariable has 1 variable");
    CheckGraphPreset(counterDoc.Variables().first().toObject().value(QStringLiteral("name")).toString() == QStringLiteral("Counter"),
                "CounterVariable variable is Counter");
    const auto* cntTick = FindGraphNode(counterDoc, QStringLiteral("Event.Tick"));
    const auto* cntGet = FindGraphNode(counterDoc, QStringLiteral("Variable.Get"));
    const auto* cntOne = FindGraphNode(counterDoc, QStringLiteral("Const.Float"));
    const auto* cntAdd = FindGraphNode(counterDoc, QStringLiteral("Math.Add"));
    const auto* cntSet = FindGraphNode(counterDoc, QStringLiteral("Variable.Set"));
    CheckGraphPreset(cntTick && cntGet && cntOne && cntAdd && cntSet, "CounterVariable has all 5 nodes");
    CheckGraphPreset(HasPresetConnection(counterDoc, cntTick->id, QStringLiteral("then"), cntSet->id, QStringLiteral("exec")),
                "CounterVariable Tick.then -> VarSet.exec");
    CheckGraphPreset(HasPresetConnection(counterDoc, cntGet->id, QStringLiteral("Value"), cntAdd->id, QStringLiteral("A")),
                "CounterVariable VarGet.Value -> Add.A");
    CheckGraphPreset(HasPresetConnection(counterDoc, cntOne->id, QStringLiteral("Value"), cntAdd->id, QStringLiteral("B")),
                "CounterVariable ConstOne.Value -> Add.B");
    CheckGraphPreset(HasPresetConnection(counterDoc, cntAdd->id, QStringLiteral("Out"), cntSet->id, QStringLiteral("Value")),
                "CounterVariable Add.Out -> VarSet.Value");

    // 7. ConditionThreshold
    const GraphDocument condDoc = ScriptGraphPresets::BuildConditionThreshold();
    CheckGraphPreset(condDoc.Variables().size() == 1, "ConditionThreshold has 1 variable");
    const auto* cndTick = FindGraphNode(condDoc, QStringLiteral("Event.Tick"));
    const auto* cndGet = FindGraphNode(condDoc, QStringLiteral("Variable.Get"));
    const auto* cndConst = FindGraphNode(condDoc, QStringLiteral("Const.Float"));
    const auto* cndGreater = FindGraphNode(condDoc, QStringLiteral("Logic.Greater"));
    const auto* cndBranch = FindGraphNode(condDoc, QStringLiteral("Flow.Branch"));
    const auto* cndMsg = FindGraphNode(condDoc, QStringLiteral("Const.String"));
    const auto* cndPrint = FindGraphNode(condDoc, QStringLiteral("Action.Print"));
    CheckGraphPreset(cndTick && cndGet && cndConst && cndGreater && cndBranch && cndMsg && cndPrint, "ConditionThreshold has all 7 nodes");
    CheckGraphPreset(HasPresetConnection(condDoc, cndTick->id, QStringLiteral("then"), cndBranch->id, QStringLiteral("exec")),
                "ConditionThreshold Tick.then -> Branch.exec");
    CheckGraphPreset(HasPresetConnection(condDoc, cndGet->id, QStringLiteral("Value"), cndGreater->id, QStringLiteral("A")),
                "ConditionThreshold VarGet.Value -> Greater.A");
    CheckGraphPreset(HasPresetConnection(condDoc, cndConst->id, QStringLiteral("Value"), cndGreater->id, QStringLiteral("B")),
                "ConditionThreshold Const.Value -> Greater.B");
    CheckGraphPreset(HasPresetConnection(condDoc, cndGreater->id, QStringLiteral("Out"), cndBranch->id, QStringLiteral("Condition")),
                "ConditionThreshold Greater.Out -> Branch.Condition");
    CheckGraphPreset(HasPresetConnection(condDoc, cndBranch->id, QStringLiteral("True"), cndPrint->id, QStringLiteral("exec")),
                "ConditionThreshold Branch.True -> Print.exec");
    CheckGraphPreset(HasPresetConnection(condDoc, cndMsg->id, QStringLiteral("Value"), cndPrint->id, QStringLiteral("Value")),
                "ConditionThreshold Msg.Value -> Print.Value");
}

void TestFsmPresets()
{
    using namespace EditorCore;
    FsmGraphSchema schema;

    // 1. Idle -> Patrol
    const GraphDocument patrolDoc = FsmGraphPresets::BuildIdlePatrol();
    CheckGraphPreset(patrolDoc.FileFormat() == GraphDocument::Format::V1, "FSM Patrol format V1");
    CheckGraphPreset(patrolDoc.Schema() == QStringLiteral("ai.fsm"), "FSM Patrol schema ai.fsm");
    CheckGraphPreset(patrolDoc.Variables().size() == 1, "FSM Patrol has 1 variable");
    CheckGraphPreset(patrolDoc.Nodes().size() == 2, "FSM Patrol has 2 states");
    CheckGraphPreset(patrolDoc.Connections().size() == 2, "FSM Patrol has 2 transitions");
    CheckGraphPreset(schema.Warnings(patrolDoc).isEmpty(), "FSM Patrol has 0 schema warnings");

    // 2. Idle -> Chase
    const GraphDocument chaseDoc = FsmGraphPresets::BuildIdleChase();
    CheckGraphPreset(chaseDoc.Variables().size() == 1, "FSM Chase has 1 variable");
    CheckGraphPreset(chaseDoc.Nodes().size() == 2, "FSM Chase has 2 states");
    CheckGraphPreset(chaseDoc.Connections().size() == 2, "FSM Chase has 2 transitions");
    CheckGraphPreset(schema.Warnings(chaseDoc).isEmpty(), "FSM Chase has 0 schema warnings");

    // 3. Idle -> Attack
    const GraphDocument attackDoc = FsmGraphPresets::BuildIdleAttack();
    CheckGraphPreset(attackDoc.Variables().size() == 1, "FSM Attack has 1 variable");
    CheckGraphPreset(attackDoc.Nodes().size() == 2, "FSM Attack has 2 states");
    CheckGraphPreset(attackDoc.Connections().size() == 2, "FSM Attack has 2 transitions");
    CheckGraphPreset(schema.Warnings(attackDoc).isEmpty(), "FSM Attack has 0 schema warnings");

    // 4. Patrol -> Chase -> Return
    const GraphDocument pcrDoc = FsmGraphPresets::BuildPatrolChaseReturn();
    CheckGraphPreset(pcrDoc.FileFormat() == GraphDocument::Format::V1, "PatrolChaseReturn format V1");
    CheckGraphPreset(pcrDoc.Schema() == QStringLiteral("ai.fsm"), "PatrolChaseReturn schema ai.fsm");
    CheckGraphPreset(pcrDoc.Variables().size() == 3, "PatrolChaseReturn has 3 variables");
    CheckGraphPreset(pcrDoc.Nodes().size() == 3, "PatrolChaseReturn has 3 states");
    CheckGraphPreset(pcrDoc.Connections().size() == 3, "PatrolChaseReturn has 3 transitions");
    CheckGraphPreset(schema.Warnings(pcrDoc).isEmpty(), "PatrolChaseReturn has 0 schema warnings");

    // 5. Guard: Idle <-> Attack by distance
    const GraphDocument guardDoc = FsmGraphPresets::BuildGuardAttackDistance();
    CheckGraphPreset(guardDoc.Variables().size() == 1, "GuardAttackDistance has 1 variable");
    CheckGraphPreset(guardDoc.Nodes().size() == 2, "GuardAttackDistance has 2 states");
    CheckGraphPreset(guardDoc.Connections().size() == 2, "GuardAttackDistance has 2 transitions");
    CheckGraphPreset(schema.Warnings(guardDoc).isEmpty(), "GuardAttackDistance has 0 schema warnings");

    // 6. Idle -> Flee
    const GraphDocument fleeDoc = FsmGraphPresets::BuildIdleFlee();
    CheckGraphPreset(fleeDoc.Variables().size() == 2, "IdleFlee has 2 variables");
    CheckGraphPreset(fleeDoc.Nodes().size() == 2, "IdleFlee has 2 states");
    CheckGraphPreset(fleeDoc.Connections().size() == 2, "IdleFlee has 2 transitions");
    CheckGraphPreset(schema.Warnings(fleeDoc).isEmpty(), "IdleFlee has 0 schema warnings");
}

int RunGraphPresetsTests()
{
    gGraphPresetChecks = 0;
    TestScriptPresets();
    TestFsmPresets();
    return gGraphPresetChecks;
}

} // namespace
