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
}

int RunGraphPresetsTests()
{
    gGraphPresetChecks = 0;
    TestScriptPresets();
    TestFsmPresets();
    return gGraphPresetChecks;
}

} // namespace
