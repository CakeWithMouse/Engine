#pragma once

#include "../../Core/MaterialGraphTransform.h"
#include "../../Core/HlslGraphCompiler.h"

#include <QString>
#include <cstdio>
#include <cstdlib>

namespace
{

int gMaterialTransformChecks = 0;

void CheckTransform(bool cond, const char* msg)
{
    ++gMaterialTransformChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (MaterialTransform): %s\n", msg);
        std::exit(1);
    }
}

const EditorCore::GraphNodeData* FindTransformNode(const EditorCore::GraphDocument& doc, const QString& type)
{
    for (const auto& n : doc.Nodes())
        if (n.type == type) return &n;
    return nullptr;
}

// Граф: Constant(0.75) * BaseColor -> Albedo.
EditorCore::MaterialGraphModel MakeConstantGraph(int& constantId, int& masterId)
{
    EditorCore::MaterialGraphModel model;
    masterId = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(600, 180));
    constantId = model.AddNode(QStringLiteral("Constant"), QStringLiteral("Constant"), QPointF(100, 100),
                               {{QStringLiteral("value"), 0.75}});
    const int color = model.AddNode(QStringLiteral("ColorParam"), QStringLiteral("BaseColor"), QPointF(100, 260),
                                    {{QStringLiteral("name"), QStringLiteral("BaseColor")}, {QStringLiteral("color"), QColor(255, 255, 255)}});
    const int mul = model.AddNode(QStringLiteral("Multiply"), QStringLiteral("Multiply"), QPointF(360, 160));
    model.AddConnection(constantId, QStringLiteral("Value"), mul, QStringLiteral("A"));
    model.AddConnection(color, QStringLiteral("RGB"), mul, QStringLiteral("B"));
    model.AddConnection(mul, QStringLiteral("Out"), masterId, QStringLiteral("Albedo"));
    return model;
}

void TestCanExpose()
{
    int constantId = 0, masterId = 0;
    const auto model = MakeConstantGraph(constantId, masterId);
    CheckTransform(EditorCore::MaterialGraphTransform::CanExpose(model, {constantId}), "CanExpose true for Constant");
    CheckTransform(!EditorCore::MaterialGraphTransform::CanExpose(model, {masterId}), "CanExpose false for MaterialOutput");
    CheckTransform(!EditorCore::MaterialGraphTransform::CanExpose(model, {}), "CanExpose false for empty selection");
}

void TestExposePreservesValueAndLinks()
{
    int constantId = 0, masterId = 0;
    const auto model = MakeConstantGraph(constantId, masterId);
    const auto result = EditorCore::MaterialGraphTransform::ExposeConstants(model, {constantId});

    CheckTransform(FindTransformNode(result, QStringLiteral("Constant")) == nullptr, "Constant removed after expose");
    const auto* param = FindTransformNode(result, QStringLiteral("ScalarParam"));
    CheckTransform(param != nullptr, "ScalarParam created after expose");
    CheckTransform(param->id == constantId, "Expose keeps node id (links preserved)");
    CheckTransform(qAbs(param->properties.value(QStringLiteral("value")).toDouble() - 0.75) < 1e-6, "Expose preserves value 0.75");
    CheckTransform(!param->properties.value(QStringLiteral("name")).toString().isEmpty(), "Expose assigns a name");

    bool linkKept = false;
    for (const auto& c : result.Connections())
        if (c.fromNode == constantId && c.fromPin == QStringLiteral("Value")) linkKept = true;
    CheckTransform(linkKept, "Value output connection preserved after expose");

    const auto compiled = EditorCore::HlslGraphCompiler::Compile(result);
    CheckTransform(compiled.problems.isEmpty(), "Exposed graph compiles without problems");
    CheckTransform(compiled.hlsl.contains(QStringLiteral("float Param = 0.7500f;")), "Exposed param in cbuffer with value");
    CheckTransform(compiled.hlsl.contains(QStringLiteral("Param")), "Exposed param referenced in HLSL");
}

void TestExposeUniqueNames()
{
    EditorCore::MaterialGraphModel model;
    const int master = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(600, 180));
    const int c1 = model.AddNode(QStringLiteral("Constant"), QStringLiteral("Constant"), QPointF(100, 100), {{QStringLiteral("value"), 1.0}});
    const int c2 = model.AddNode(QStringLiteral("Constant"), QStringLiteral("Constant"), QPointF(100, 200), {{QStringLiteral("value"), 2.0}});
    model.AddConnection(c1, QStringLiteral("Value"), master, QStringLiteral("Specular"));
    model.AddConnection(c2, QStringLiteral("Value"), master, QStringLiteral("AmbientModel"));
    const auto result = EditorCore::MaterialGraphTransform::ExposeConstants(model, {c1, c2});

    QString n1, n2;
    for (const auto& n : result.Nodes())
        if (n.type == QStringLiteral("ScalarParam"))
        {
            if (n.id == c1) n1 = n.properties.value(QStringLiteral("name")).toString();
            if (n.id == c2) n2 = n.properties.value(QStringLiteral("name")).toString();
        }
    CheckTransform(!n1.isEmpty() && !n2.isEmpty(), "Both constants exposed");
    CheckTransform(n1 != n2, "Exposed params get unique names");
    CheckTransform(EditorCore::HlslGraphCompiler::Compile(result).problems.isEmpty(), "Two exposed params compile without name clash");
}

void TestExposeIgnoresNonConstants()
{
    int constantId = 0, masterId = 0;
    const auto model = MakeConstantGraph(constantId, masterId);
    const auto result = EditorCore::MaterialGraphTransform::ExposeConstants(model, {masterId});
    CheckTransform(FindTransformNode(result, QStringLiteral("Constant")) != nullptr, "Non-constant selection leaves Constant intact");
    CheckTransform(FindTransformNode(result, QStringLiteral("ScalarParam")) == nullptr, "Non-constant selection creates no param");
}

} // namespace

inline int RunMaterialTransformTests()
{
    gMaterialTransformChecks = 0;
    TestCanExpose();
    TestExposePreservesValueAndLinks();
    TestExposeUniqueNames();
    TestExposeIgnoresNonConstants();
    std::printf("MaterialTransform tests: %d checks passed\n", gMaterialTransformChecks);
    return gMaterialTransformChecks;
}