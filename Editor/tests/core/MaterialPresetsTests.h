#pragma once

#include "../../Core/MaterialGraphPresets.h"
#include "../../Core/HlslGraphCompiler.h"

#include <QString>
#include <cstdio>
#include <cstdlib>

namespace
{

int gMaterialPresetChecks = 0;

void CheckPreset(bool cond, const char* msg)
{
    ++gMaterialPresetChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (MaterialPresets): %s\n", msg);
        std::exit(1);
    }
}

const EditorCore::GraphNodeData* FindByType(const EditorCore::GraphDocument& doc, const QString& type)
{
    for (const auto& n : doc.Nodes())
        if (n.type == type) return &n;
    return nullptr;
}

bool HasConnection(const EditorCore::GraphDocument& doc, int from, const QString& fromPin, int to, const QString& toPin)
{
    for (const auto& c : doc.Connections())
        if (c.fromNode == from && c.fromPin == fromPin && c.toNode == to && c.toPin == toPin) return true;
    return false;
}

void TestSimplePreset()
{
    const EditorCore::MaterialGraphModel model = EditorCore::MaterialGraphPresets::BuildSimple();
    CheckPreset(FindByType(model, QStringLiteral("MaterialOutput")) != nullptr, "Simple has MaterialOutput");
    CheckPreset(FindByType(model, QStringLiteral("Texture2D")) != nullptr, "Simple has Texture2D nodes");
    CheckPreset(FindByType(model, QStringLiteral("NormalMap")) != nullptr, "Simple has NormalMap node");

    const EditorCore::GraphCompileResult result = EditorCore::HlslGraphCompiler::Compile(model);
    CheckPreset(result.problems.isEmpty(), "Simple compiles without problems");
    CheckPreset(result.hlsl.contains(QStringLiteral("o.Albedo = saturate(BaseMap.Sample(MaterialSampler, i.UV).rgb);")), "Simple albedo from BaseMap");
    CheckPreset(result.hlsl.contains(QStringLiteral("Texture2D BaseMap : register(t6);")), "Simple declares BaseMap register t6");
    CheckPreset(result.hlsl.contains(QStringLiteral("Texture2D NormalMap : register(t7);")), "Simple declares NormalMap register t7");
    CheckPreset(result.hlsl.contains(QStringLiteral("o.Normal = NormalFromMap(i, NormalMap.Sample(MaterialSampler, i.UV).xyz, 1.0f);")), "Simple normal from NormalMap");
}

void TestOrmPresetStructure()
{
    const EditorCore::MaterialGraphModel model = EditorCore::MaterialGraphPresets::BuildOrm();
    const auto* orm = FindByType(model, QStringLiteral("Texture2D"));
    const auto* split = FindByType(model, QStringLiteral("Split"));
    const auto* inv = FindByType(model, QStringLiteral("OneMinus"));
    const auto* master = FindByType(model, QStringLiteral("MaterialOutput"));
    const auto* normal = FindByType(model, QStringLiteral("NormalMap"));
    CheckPreset(orm && split && inv && master && normal, "ORM has Texture2D, Split, OneMinus, MaterialOutput, NormalMap");
    CheckPreset(HasConnection(model, split->id, QStringLiteral("R"), master->id, QStringLiteral("AmbientModel")), "ORM R(AO) -> AmbientModel");
    CheckPreset(HasConnection(model, split->id, QStringLiteral("G"), inv->id, QStringLiteral("In")), "ORM G(Roughness) -> OneMinus");
    CheckPreset(HasConnection(model, inv->id, QStringLiteral("Out"), master->id, QStringLiteral("Specular")), "ORM (1-roughness) -> Specular");
    CheckPreset(HasConnection(model, normal->id, QStringLiteral("Normal"), master->id, QStringLiteral("Normal")), "ORM NormalMap -> Normal");
    // Metallic (B) распакован, но не подключён: входа Metallic в движке нет.
    bool bUsed = false;
    for (const auto& c : model.Connections())
        if (c.fromNode == split->id && c.fromPin == QStringLiteral("B")) bUsed = true;
    CheckPreset(!bUsed, "ORM B(Metallic) unpacked but left unconnected (no engine input)");
}

void TestOrmUnpackChannels()
{
    // Чистая распаковка ORM: Texture2D -> Split, каналы R/G/B на три входа выхода.
    EditorCore::MaterialGraphModel model;
    const int master = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(600, 180));
    const int orm = model.AddNode(QStringLiteral("Texture2D"), QStringLiteral("ORM"), QPointF(-160, 120),
                                  {{QStringLiteral("textureName"), QStringLiteral("ORM")}, {QStringLiteral("slot"), 6}});
    const int split = model.AddNode(QStringLiteral("Split"), QStringLiteral("Unpack"), QPointF(100, 120));
    model.AddConnection(orm, QStringLiteral("RGBA"), split, QStringLiteral("In"));
    model.AddConnection(split, QStringLiteral("R"), master, QStringLiteral("AmbientModel"));
    model.AddConnection(split, QStringLiteral("G"), master, QStringLiteral("Specular"));
    model.AddConnection(split, QStringLiteral("B"), master, QStringLiteral("Alpha"));

    const EditorCore::GraphCompileResult result = EditorCore::HlslGraphCompiler::Compile(model);
    CheckPreset(result.problems.isEmpty(), "ORM unpack compiles without problems");
    CheckPreset(result.hlsl.contains(QStringLiteral("Texture2D ORM : register(t6);")), "ORM unpack declares texture t6");
    const QString sample = QStringLiteral("ORM.Sample(MaterialSampler, i.UV)");
    CheckPreset(result.hlsl.contains(QStringLiteral("o.AmbientModel = saturate((%1).x);").arg(sample)), "ORM unpack R channel (.x)");
    CheckPreset(result.hlsl.contains(QStringLiteral("o.SpecularWeight = saturate((%1).y);").arg(sample)), "ORM unpack G channel (.y)");
    CheckPreset(result.hlsl.contains(QStringLiteral("o.Alpha = saturate((%1).z);").arg(sample)), "ORM unpack B channel (.z)");
}

void TestOrmPresetCompile()
{
    const EditorCore::MaterialGraphModel model = EditorCore::MaterialGraphPresets::BuildOrm();
    const EditorCore::GraphCompileResult result = EditorCore::HlslGraphCompiler::Compile(model);
    CheckPreset(result.problems.isEmpty(), "ORM preset compiles without problems");
    CheckPreset(result.hlsl.contains(QStringLiteral("Texture2D ORM : register(t6);")), "ORM preset declares texture t6");
    CheckPreset(result.hlsl.contains(QStringLiteral("Texture2D BaseMap : register(t7);")), "ORM preset declares BaseMap t7");
    CheckPreset(result.hlsl.contains(QStringLiteral("Texture2D NormalMap : register(t8);")), "ORM preset declares NormalMap t8");
    CheckPreset(result.hlsl.contains(QStringLiteral("o.Albedo = saturate(BaseMap.Sample(MaterialSampler, i.UV).rgb);")), "ORM preset albedo from BaseMap");
    CheckPreset(result.hlsl.contains(QStringLiteral("o.AmbientModel = saturate((ORM.Sample(MaterialSampler, i.UV)).x);")),
                "ORM preset AO wired to AmbientModel");
    CheckPreset(result.hlsl.contains(QStringLiteral("o.SpecularWeight = saturate((1.0f - (ORM.Sample(MaterialSampler, i.UV)).y));")),
                "ORM preset roughness inverted to SpecularWeight");
    CheckPreset(result.hlsl.contains(QStringLiteral("o.Normal = NormalFromMap(i, NormalMap.Sample(MaterialSampler, i.UV).xyz, 1.0f);")),
                "ORM preset normal from NormalMap");
}

} // namespace

inline int RunMaterialPresetsTests()
{
    gMaterialPresetChecks = 0;
    TestSimplePreset();
    TestOrmPresetStructure();
    TestOrmUnpackChannels();
    TestOrmPresetCompile();
    std::printf("MaterialPresets tests: %d checks passed\n", gMaterialPresetChecks);
    return gMaterialPresetChecks;
}