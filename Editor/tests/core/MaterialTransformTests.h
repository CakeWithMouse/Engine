#pragma once

#include "../../Core/MaterialGraphTransform.h"
#include "../../Core/HlslGraphCompiler.h"

#include <QSet>
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

// Симулирует логику Apply(): если параметр удалён из графа,
// его имя должно пропасть из publicParams_.
void TestPublicParamFilterOnDelete()
{
    // Собираем граф с двумя параметрами: ScalarParam "Speed" и ColorParam "Tint"
    EditorCore::MaterialGraphModel model;
    const int master = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(600, 180));
    const int speedId = model.AddNode(QStringLiteral("ScalarParam"), QStringLiteral("Speed"),    QPointF(100, 100),
                                      {{QStringLiteral("name"), QStringLiteral("Speed")}});
    const int tintId  = model.AddNode(QStringLiteral("ColorParam"),  QStringLiteral("Tint"),     QPointF(100, 260),
                                      {{QStringLiteral("name"), QStringLiteral("Tint")},
                                       {QStringLiteral("color"), QColor(255, 255, 255)}});
    model.AddConnection(speedId, QStringLiteral("Value"), master, QStringLiteral("Specular"));
    model.AddConnection(tintId,  QStringLiteral("RGB"),   master, QStringLiteral("Albedo"));

    // Компилируем — оба параметра присутствуют
    const auto compiled = EditorCore::HlslGraphCompiler::Compile(model);
    CheckTransform(compiled.problems.isEmpty(), "Two-param graph compiles without problems");
    CheckTransform(compiled.hlsl.contains(QStringLiteral("Speed")), "HLSL contains Speed before delete");
    CheckTransform(compiled.hlsl.contains(QStringLiteral("Tint")),  "HLSL contains Tint before delete");

    // Удаляем узел "Speed" из модели
    EditorCore::MaterialGraphModel reduced = model;
    reduced.RemoveNode(speedId);
    const auto compiledReduced = EditorCore::HlslGraphCompiler::Compile(reduced);
    CheckTransform(compiledReduced.problems.isEmpty(), "Reduced graph compiles without problems");
    CheckTransform(!compiledReduced.hlsl.contains(QStringLiteral("Speed")), "HLSL no longer contains Speed after delete");
    CheckTransform(compiledReduced.hlsl.contains(QStringLiteral("Tint")),   "HLSL still contains Tint after delete");

    // Симулируем фильтрацию publicParams_: было {"Speed","Tint"}, новые параметры только {"Tint"}
    QSet<QString> publicParams = {QStringLiteral("Speed"), QStringLiteral("Tint")};
    // Текущие имена параметров — из нод типа *Param в актуальной модели
    QSet<QString> currentNames;
    for (const auto& n : reduced.Nodes())
    {
        if (n.type.endsWith(QStringLiteral("Param")))
            currentNames.insert(n.properties.value(QStringLiteral("name")).toString());
    }
    for (auto it = publicParams.begin(); it != publicParams.end(); )
    {
        if (!currentNames.contains(*it)) it = publicParams.erase(it);
        else ++it;
    }
    CheckTransform(!publicParams.contains(QStringLiteral("Speed")), "publicParams filter removes deleted Speed");
    CheckTransform(publicParams.contains(QStringLiteral("Tint")),   "publicParams filter keeps existing Tint");
}

void TestExtractDefaultParams()
{
    // 1. Проверяем сбор дефолтных параметров с мастер-ноды по умолчанию (BaseColor и Specular)
    EditorCore::MaterialGraphModel defaultModel;
    defaultModel.SetupDefaultMasterNode();
    const QJsonObject defaultParams = EditorCore::MaterialGraphTransform::ExtractDefaultParams(defaultModel);
    CheckTransform(defaultParams.contains(QStringLiteral("BaseColor")), "defaultParams contains BaseColor");
    CheckTransform(defaultParams.contains(QStringLiteral("Specular")), "defaultParams contains Specular");
    CheckTransform(defaultParams.value(QStringLiteral("BaseColor")).isArray(), "BaseColor is JsonArray");
    const QJsonArray baseColorArr = defaultParams.value(QStringLiteral("BaseColor")).toArray();
    CheckTransform(baseColorArr.size() == 4, "BaseColor array has 4 components");
    CheckTransform(qAbs(defaultParams.value(QStringLiteral("Specular")).toDouble() - 0.5) < 1e-4, "Specular is 0.5");

    // 2. Проверяем векторные параметры
    EditorCore::MaterialGraphModel vectorModel;
    vectorModel.AddNode(QStringLiteral("Vector2Param"), QStringLiteral("UVScale"), QPointF(0, 0),
                        {{QStringLiteral("name"), QStringLiteral("UVScale")},
                         {QStringLiteral("value"), QVariantList{2.0, 4.0}}});
    vectorModel.AddNode(QStringLiteral("Vector3Param"), QStringLiteral("Offset3D"), QPointF(0, 100),
                        {{QStringLiteral("name"), QStringLiteral("Offset3D")},
                         {QStringLiteral("value"), QVariantList{1.0, 2.0, 3.0}}});
    const QJsonObject vecParams = EditorCore::MaterialGraphTransform::ExtractDefaultParams(vectorModel);
    CheckTransform(vecParams.contains(QStringLiteral("UVScale")), "vecParams contains UVScale");
    CheckTransform(vecParams.value(QStringLiteral("UVScale")).toArray().size() == 2, "UVScale has 2 components");
    CheckTransform(vecParams.contains(QStringLiteral("Offset3D")), "vecParams contains Offset3D");
    CheckTransform(vecParams.value(QStringLiteral("Offset3D")).toArray().size() == 3, "Offset3D has 3 components");
}

} // namespace

inline int RunMaterialTransformTests()
{
    gMaterialTransformChecks = 0;
    TestCanExpose();
    TestExposePreservesValueAndLinks();
    TestExposeUniqueNames();
    TestExposeIgnoresNonConstants();
    TestPublicParamFilterOnDelete();
    TestExtractDefaultParams();
    std::printf("MaterialTransform tests: %d checks passed\n", gMaterialTransformChecks);
    return gMaterialTransformChecks;
}