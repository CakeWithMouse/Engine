#pragma once

#include "../../Core/HlslToGraph.h"
#include "../../Core/HlslGraphCompiler.h"
#include "../../Core/MaterialGraphPresets.h"

#include <QColor>
#include <QString>
#include <cstdio>
#include <cstdlib>

namespace
{

int gHlslParseChecks = 0;

void CheckParse(bool cond, const char* msg)
{
    ++gHlslParseChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (HlslToGraph): %s\n", msg);
        std::exit(1);
    }
}

// Граф -> HLSL -> граф -> HLSL: обе сборки должны совпасть байт в байт.
void RoundTrip(const EditorCore::MaterialGraphModel& model, const char* label)
{
    const QString first = EditorCore::HlslGraphCompiler::Compile(model).hlsl;
    const auto parsed = EditorCore::HlslToGraph::Parse(first);
    CheckParse(parsed.ok, label);
    if (!parsed.ok)
    {
        for (const QString& p : parsed.problems) std::fprintf(stderr, "  problem: %s\n", p.toUtf8().constData());
        return;
    }
    const auto recompiled = EditorCore::HlslGraphCompiler::Compile(parsed.model);
    CheckParse(recompiled.problems.isEmpty(), "round-trip graph compiles clean");
    CheckParse(recompiled.hlsl == first, "round-trip HLSL is identical");
}

void TestSimpleRoundTrip()
{
    RoundTrip(EditorCore::MaterialGraphPresets::BuildSimple(), "Simple preset round-trip parses");
}

void TestOrmRoundTrip()
{
    RoundTrip(EditorCore::MaterialGraphPresets::BuildOrm(), "ORM preset round-trip parses");
}

void TestMultiplyRoundTrip()
{
    EditorCore::MaterialGraphModel model;
    const int out = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(600, 180));
    const int c = model.AddNode(QStringLiteral("Constant"), QStringLiteral("Constant"), QPointF(100, 100), {{QStringLiteral("value"), 0.75}});
    const int col = model.AddNode(QStringLiteral("ColorParam"), QStringLiteral("BaseColor"), QPointF(100, 260),
                                  {{QStringLiteral("name"), QStringLiteral("BaseColor")}, {QStringLiteral("color"), QColor(255, 255, 255)}});
    const int mul = model.AddNode(QStringLiteral("Multiply"), QStringLiteral("Multiply"), QPointF(360, 160));
    model.AddConnection(c, QStringLiteral("Value"), mul, QStringLiteral("A"));
    model.AddConnection(col, QStringLiteral("RGB"), mul, QStringLiteral("B"));
    model.AddConnection(mul, QStringLiteral("Out"), out, QStringLiteral("Albedo"));
    RoundTrip(model, "Constant * ColorParam round-trip parses");
}

void TestLerpRoundTrip()
{
    EditorCore::MaterialGraphModel model;
    const int out = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(600, 180));
    const int a = model.AddNode(QStringLiteral("ColorParam"), QStringLiteral("ColorA"), QPointF(100, 100),
                                {{QStringLiteral("name"), QStringLiteral("ColorA")}, {QStringLiteral("color"), QColor(255, 0, 0)}});
    const int b = model.AddNode(QStringLiteral("ColorParam"), QStringLiteral("ColorB"), QPointF(100, 260),
                                {{QStringLiteral("name"), QStringLiteral("ColorB")}, {QStringLiteral("color"), QColor(0, 0, 255)}});
    const int t = model.AddNode(QStringLiteral("ScalarParam"), QStringLiteral("Mix"), QPointF(100, 420),
                                {{QStringLiteral("name"), QStringLiteral("Mix")}, {QStringLiteral("value"), 0.5}});
    const int lerp = model.AddNode(QStringLiteral("Lerp"), QStringLiteral("Lerp"), QPointF(360, 160));
    model.AddConnection(a, QStringLiteral("RGB"), lerp, QStringLiteral("A"));
    model.AddConnection(b, QStringLiteral("RGB"), lerp, QStringLiteral("B"));
    model.AddConnection(t, QStringLiteral("Value"), lerp, QStringLiteral("Alpha"));
    model.AddConnection(lerp, QStringLiteral("Out"), out, QStringLiteral("Albedo"));
    RoundTrip(model, "Lerp of two colors round-trip parses");
}

void TestTexturePinRoundTrip()
{
    EditorCore::MaterialGraphModel model;
    const int out = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(600, 180));
    const int tex = model.AddNode(QStringLiteral("Texture2D"), QStringLiteral("BaseMap"), QPointF(100, 160),
                                  {{QStringLiteral("textureName"), QStringLiteral("BaseMap")}, {QStringLiteral("slot"), 6}});
    model.AddConnection(tex, QStringLiteral("RGB"), out, QStringLiteral("Albedo"));
    model.AddConnection(tex, QStringLiteral("A"), out, QStringLiteral("Alpha"));
    RoundTrip(model, "Texture RGB/A pins round-trip parses");
}

void TestRejectsHandHlsl()
{
    const QString hand = QStringLiteral("void Surface(MaterialInput i, inout MaterialOutput o)\n{\n    o.Albedo = i.Color.rgb;\n}\n");
    CheckParse(!EditorCore::HlslToGraph::IsGenerated(hand), "Hand-written HLSL is not generated");
    const auto parsed = EditorCore::HlslToGraph::Parse(hand);
    CheckParse(!parsed.ok, "Hand-written HLSL is rejected");
    CheckParse(!parsed.problems.isEmpty(), "Rejection reports a problem");
}

void TestUnknownIdentifierProblem()
{
    const QString shader = QStringLiteral(
        "// Generated by PainEngine Material Node Graph\n"
        "void Surface(MaterialInput i, inout MaterialOutput o)\n{\n"
        "    o.Albedo = saturate(Mystery.rgb);\n}\n");
    const auto parsed = EditorCore::HlslToGraph::Parse(shader);
    CheckParse(!parsed.ok, "Unknown identifier makes parse fail");
    bool found = false;
    for (const QString& p : parsed.problems) if (p.contains(QStringLiteral("Mystery"))) found = true;
    CheckParse(found, "Problem names the unknown identifier");
}

} // namespace

inline int RunHlslToGraphTests()
{
    gHlslParseChecks = 0;
    TestSimpleRoundTrip();
    TestOrmRoundTrip();
    TestMultiplyRoundTrip();
    TestLerpRoundTrip();
    TestTexturePinRoundTrip();
    TestRejectsHandHlsl();
    TestUnknownIdentifierProblem();
    std::printf("HlslToGraph tests: %d checks passed\n", gHlslParseChecks);
    return gHlslParseChecks;
}