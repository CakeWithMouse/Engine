#include "HlslGraphCompiler.h"
#include "EditorLocale.h"

#include <QColor>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <functional>

namespace EditorCore
{

namespace
{
QString F4(double val)
{
    return QLocale::c().toString(val, 'f', 4);
}

QString F2(double val)
{
    return QLocale::c().toString(val, 'f', 2);
}

bool IsTextureNode(const QString& type)
{
    return type == QStringLiteral("Texture2D") || type == QStringLiteral("NormalMap");
}

QString DefaultTextureName(const QString& type)
{
    return type == QStringLiteral("NormalMap") ? QStringLiteral("NormalMap") : QStringLiteral("BaseMap");
}

// Выражение HLSL и число его компонент (1 — float, 2..4 — floatN).
struct Value
{
    QString expr;
    int size = 1;
};

QString VectorType(int size)
{
    return size == 1 ? QStringLiteral("float") : QStringLiteral("float%1").arg(size);
}

// Приведение к размеру входа: скаляр растягивается, лишние компоненты отбрасываются,
// недостающие добиваются нулями (альфа — единицей). Так HLSL собирается при любой связи.
QString Convert(const Value& value, int size)
{
    if (value.size == size) return value.expr;
    if (value.size == 1) return QStringLiteral("((%1)(%2))").arg(VectorType(size), value.expr);
    if (value.size > size)
        return QStringLiteral("(%1).%2").arg(value.expr, QStringLiteral("xyzw").left(size));
    QString padded = QStringLiteral("%1(%2").arg(VectorType(size), value.expr);
    for (int i = value.size; i < size; ++i) padded += i == 3 ? QStringLiteral(", 1.0f") : QStringLiteral(", 0.0f");
    return padded + QLatin1Char(')');
}

// Компонента вектора для Split: у скаляра все компоненты — он сам, недостающие — 0 (альфа — 1).
QString Component(const Value& value, int index)
{
    if (value.size == 1) return value.expr;
    if (index < value.size) return QStringLiteral("(%1).%2").arg(value.expr, QStringLiteral("xyzw").mid(index, 1));
    return index == 3 ? QStringLiteral("1.0f") : QStringLiteral("0.0f");
}

struct NodeSpec
{
    QStringList inputs, outputs; // "имя:тип", тип float/float2/float3/float4/any
};

const QHash<QString, NodeSpec>& Specs()
{
    static const QHash<QString, NodeSpec> specs{
        {QStringLiteral("MaterialOutput"), {{"Albedo:float3", "Alpha:float", "Specular:float", "AmbientModel:float", "Emissive:float3", "Normal:float3"}, {}}},
        // Параметры и константы
        {QStringLiteral("ColorParam"), {{}, {"RGBA:float4", "RGB:float3", "A:float"}}},
        {QStringLiteral("ScalarParam"), {{}, {"Value:float"}}},
        {QStringLiteral("Vector2Param"), {{}, {"Value:float2"}}},
        {QStringLiteral("Vector3Param"), {{}, {"Value:float3"}}},
        {QStringLiteral("Constant"), {{}, {"Value:float"}}},
        // Входы поверхности
        {QStringLiteral("ObjectColor"), {{}, {"RGBA:float4", "RGB:float3", "A:float"}}},
        {QStringLiteral("SurfaceDefaults"), {{}, {"Albedo:float3", "Alpha:float", "Normal:float3"}}},
        {QStringLiteral("InstanceData"), {{}, {"Data:float3", "X:float", "Y:float", "Z:float"}}},
        {QStringLiteral("WorldPosition"), {{}, {"Position:float3"}}},
        {QStringLiteral("SurfaceNormal"), {{}, {"Normal:float3"}}},
        {QStringLiteral("ViewDirection"), {{}, {"Direction:float3"}}},
        {QStringLiteral("FrontFace"), {{}, {"IsFront:float"}}},
        {QStringLiteral("Time"), {{}, {"Time:float", "SineTime:float"}}},
        {QStringLiteral("UVCoord"), {{}, {"UV:float2"}}},
        {QStringLiteral("Panner"), {{"UV:float2"}, {"UV_Out:float2"}}},
        // Текстуры
        {QStringLiteral("Texture2D"), {{"UV:float2"}, {"RGBA:float4", "RGB:float3", "R:float", "G:float", "B:float", "A:float"}}},
        {QStringLiteral("NormalMap"), {{"UV:float2", "Strength:float"}, {"Normal:float3"}}},
        // Каналы
        {QStringLiteral("Split"), {{"In:any"}, {"R:float", "G:float", "B:float", "A:float"}}},
        {QStringLiteral("Combine"), {{"R:float", "G:float", "B:float", "A:float"}, {"RGBA:float4", "RGB:float3", "RG:float2"}}},
        // Математика: размер результата берётся по входам
        {QStringLiteral("Add"), {{"A:any", "B:any"}, {"Out:any"}}},
        {QStringLiteral("Subtract"), {{"A:any", "B:any"}, {"Out:any"}}},
        {QStringLiteral("Multiply"), {{"A:any", "B:any"}, {"Out:any"}}},
        {QStringLiteral("Divide"), {{"A:any", "B:any"}, {"Out:any"}}},
        {QStringLiteral("Power"), {{"Base:any", "Exp:any"}, {"Out:any"}}},
        {QStringLiteral("Min"), {{"A:any", "B:any"}, {"Out:any"}}},
        {QStringLiteral("Max"), {{"A:any", "B:any"}, {"Out:any"}}},
        {QStringLiteral("Lerp"), {{"A:any", "B:any", "Alpha:any"}, {"Out:any"}}},
        {QStringLiteral("Clamp"), {{"In:any", "Min:any", "Max:any"}, {"Out:any"}}},
        {QStringLiteral("Saturate"), {{"In:any"}, {"Out:any"}}},
        {QStringLiteral("OneMinus"), {{"In:any"}, {"Out:any"}}},
        {QStringLiteral("Abs"), {{"In:any"}, {"Out:any"}}},
        {QStringLiteral("Frac"), {{"In:any"}, {"Out:any"}}},
        {QStringLiteral("Sine"), {{"In:any"}, {"Out:any"}}},
        {QStringLiteral("Normalize"), {{"In:any"}, {"Out:any"}}},
        {QStringLiteral("Step"), {{"Edge:any", "In:any"}, {"Out:any"}}},
        {QStringLiteral("Smoothstep"), {{"Min:any", "Max:any", "In:any"}, {"Out:any"}}},
        {QStringLiteral("Dot"), {{"A:any", "B:any"}, {"Out:float"}}},
        {QStringLiteral("Fresnel"), {{"Power:float"}, {"Out:float"}}},
    };
    return specs;
}
} // namespace

QVector<GraphPin> HlslGraphCompiler::NodePins(const QString& type)
{
    QVector<GraphPin> pins;
    const auto spec = Specs().find(type);
    if (spec == Specs().end()) return pins;
    for (const bool input : {true, false})
        for (const QString& pin : input ? spec->inputs : spec->outputs)
            pins.append({pin.section(QLatin1Char(':'), 0, 0), input, pin.section(QLatin1Char(':'), 1, 1), {}});
    return pins;
}

bool HlslGraphCompiler::IsKnownNode(const QString& type)
{
    return Specs().contains(type);
}

QString HlslGraphCompiler::NameProblem(const QString& name)
{
    static const QRegularExpression identifier(QStringLiteral(R"(^[A-Za-z_]\w*$)"));
    if (!identifier.match(name).hasMatch())
        return Tr("Имя «%1»: латинские буквы, цифры и «_», не с цифры.", "Name \"%1\": Latin letters, digits and _, not starting with a digit.").arg(name);
    // Типы с размерами: float3, int2x2, half4 и т.д.
    static const QRegularExpression vectorType(QStringLiteral(
        R"(^(bool|int|uint|dword|half|float|double|min16float|min10float|min16int|min12int|min16uint)([1-4](x[1-4])?)?$)"));
    static const QSet<QString> keywords{
        "AppendStructuredBuffer", "asm", "asm_fragment", "BlendState", "break", "Buffer", "ByteAddressBuffer", "case", "cbuffer",
        "centroid", "class", "column_major", "compile", "compile_fragment", "CompileShader", "const", "continue", "ComputeShader",
        "ConsumeStructuredBuffer", "default", "DepthStencilState", "DepthStencilView", "discard", "do", "DomainShader", "else",
        "export", "extern", "false", "for", "fxgroup", "GeometryShader", "groupshared", "Hullshader", "if", "in", "inline", "inout",
        "InputPatch", "interface", "line", "lineadj", "linear", "LineStream", "matrix", "namespace", "nointerpolation",
        "noperspective", "NULL", "out", "OutputPatch", "packoffset", "pass", "pixelfragment", "PixelShader", "point", "PointStream",
        "precise", "RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer",
        "RWStructuredBuffer", "RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D", "sample",
        "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state", "static",
        "string", "struct", "switch", "StructuredBuffer", "tbuffer", "technique", "technique10", "technique11", "texture",
        "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS", "Texture2DMSArray", "Texture3D",
        "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream", "uniform", "unorm",
        "unsigned", "vector", "vertexfragment", "VertexShader", "void", "volatile", "while", "auto", "catch", "char", "const_cast",
        "delete", "dynamic_cast", "enum", "explicit", "friend", "goto", "long", "mutable", "new", "operator", "private",
        "protected", "public", "reinterpret_cast", "short", "signed", "sizeof", "static_cast", "template", "this", "throw", "try",
        "typename", "union", "using", "virtual",
        // Встроенные функции: параметр с таким именем закрыл бы функцию внутри Surface.
        "abs", "acos", "all", "any", "asin", "atan", "atan2", "ceil", "clamp", "clip", "cos", "cosh", "cross", "ddx", "ddy",
        "degrees", "determinant", "distance", "dot", "exp", "exp2", "floor", "fmod", "frac", "fwidth", "isnan", "length", "lerp",
        "log", "log2", "max", "min", "modf", "mul", "normalize", "pow", "radians", "rcp", "reflect", "refract", "round", "rsqrt",
        "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step", "tan", "tanh", "transpose", "trunc"};
    if (keywords.contains(name) || vectorType.match(name).hasMatch())
        return Tr("«%1» — слово HLSL, выберите другое имя.", "\"%1\" is an HLSL word, choose another name.").arg(name);
    // Имена шаблона материала (MaterialTemplate.hlsl, MaterialCommon.hlsli, Common.hlsli) и аргументы Surface.
    static const QSet<QString> engine{
        "i", "o", "defaults", "Surface", "MaterialInput", "MaterialOutput", "MaterialSampler", "MaterialParams", "NormalFromMap",
        "ComponentTexture", "ComponentSampler", "MaterialDefaultSpecularWeight", "MaterialDefaultAmbientModel", "InstBuf",
        "InstData", "MaterialPassTransform", "PassViewProjection", "VS_IN", "PS_IN", "VSMain", "PSMain", "EvaluateMaterial",
        "ShadeForward", "BakeOut", "GBufferOut", "ConstantBuffer", "worldMatrix", "viewMatrix", "projectionMatrix",
        "invViewMatrix", "invProjectionMatrix", "ObjectColor", "UVOffset", "HasTexture", "padding", "CameraPosition",
        "LightPositions", "LightColors", "LightParams", "LightMeta", "ReflectionData", "LightViewProjection", "CascadeSplits",
        "ShadowParams", "LightDirection", "DirectionalLightColorIntensity", "normalMatrix", "StaticShadowViewProjection",
        "StaticShadowParams", "TimeParams"};
    if (engine.contains(name) || name.startsWith(QStringLiteral("MATERIAL_")) || name.startsWith(QStringLiteral("SV_")))
        return Tr("Имя «%1» занято шаблоном материала движка.", "The name \"%1\" is taken by the engine material template.").arg(name);
    return {};
}

GraphCompileResult HlslGraphCompiler::Compile(const MaterialGraphModel& model)
{
    GraphCompileResult result;
    QVector<QPair<QString, int>> cbufferParams;       // строка и нода
    QVector<QPair<QString, QList<int>>> textureDecls; // строка и все ноды этой текстуры
    bool hasEmissive = false, usesDefaults = false;

    const auto& nodes = model.Nodes();
    const auto& connections = model.Connections();

    // Ошибки, видные до компиляции: неизвестная нода, негодное или повторённое имя.
    // Ноды текстур с одним именем читают одну текстуру — это не повтор.
    QHash<QString, bool> names; // имя -> занято текстурой
    for (const auto& n : nodes)
    {
        if (!IsKnownNode(n.type))
        {
            result.problems.insert(n.id, Tr("Неизвестный тип ноды «%1»: в HLSL она даёт 0.", "Unknown node type \"%1\": it gives 0 in HLSL.").arg(n.type));
            continue;
        }
        const bool texture = IsTextureNode(n.type);
        if (!texture && !n.type.endsWith(QStringLiteral("Param"))) continue;
        const QString name = texture ? n.properties.value(QStringLiteral("textureName"), DefaultTextureName(n.type)).toString()
                                     : n.properties.value(QStringLiteral("name")).toString();
        if (const QString problem = NameProblem(name); !problem.isEmpty())
            result.problems.insert(n.id, problem);
        else if (names.contains(name) && !(texture && names.value(name)))
            result.problems.insert(n.id, Tr("Имя «%1» уже есть в графе: переименуйте одну из нод.",
                                            "The name \"%1\" is already in the graph: rename one of the nodes.").arg(name));
        else
            names.insert(name, texture);
    }

    // Сбор параметров и текстур
    for (const auto& n : nodes)
    {
        if (n.type == QStringLiteral("ColorParam"))
        {
            QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Color_%1").arg(n.id)).toString();
            QColor col = n.properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>();
            cbufferParams.append({QStringLiteral("    float4 %1 = float4(%2f, %3f, %4f, %5f);")
                                      .arg(name)
                                      .arg(F4(col.redF()))
                                      .arg(F4(col.greenF()))
                                      .arg(F4(col.blueF()))
                                      .arg(F4(col.alphaF())), n.id});
        }
        else if (n.type == QStringLiteral("ScalarParam"))
        {
            QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Param_%1").arg(n.id)).toString();
            double val = n.properties.value(QStringLiteral("value"), 0.5).toDouble();
            cbufferParams.append({QStringLiteral("    float %1 = %2f;").arg(name).arg(F4(val)), n.id});
        }
        else if (n.type == QStringLiteral("Vector2Param") || n.type == QStringLiteral("Vector3Param"))
        {
            const int size = n.type == QStringLiteral("Vector2Param") ? 2 : 3;
            const QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Vector_%1").arg(n.id)).toString();
            const QVariantList stored = n.properties.value(QStringLiteral("value")).toList();
            QStringList components;
            for (int i = 0; i < size; ++i) components << F4(i < stored.size() ? stored[i].toDouble() : 0.0) + QLatin1Char('f');
            cbufferParams.append({QStringLiteral("    %1 %2 = %1(%3);").arg(VectorType(size), name, components.join(QStringLiteral(", "))), n.id});
        }
    }

    // Текстуры: ноды с одним именем читают одну текстуру; занятый регистр заменяется
    // следующим свободным (t0-t5 и t32 принадлежат движку; D3D11 имеет 128 SRV).
    QStringList declaredTextures;
    QList<int> usedSlots;
    for (const auto& n : nodes)
    {
        if (!IsTextureNode(n.type))
            continue;
        const QString texName = n.properties.value(QStringLiteral("textureName"), DefaultTextureName(n.type)).toString();
        if (const auto index = declaredTextures.indexOf(texName); index >= 0)
        {
            textureDecls[index].second << n.id;
            continue;
        }
        int slot = std::clamp(n.properties.value(QStringLiteral("slot"), 6).toInt(), 6, 127);
        const int first = slot;
        while (slot == 32 || usedSlots.contains(slot))
        {
            slot = slot == 127 ? 6 : slot + 1;
            if (slot == first)
            {
                result.problems.insert(n.id, Tr("Нет свободных слотов текстур материала (максимум 121).",
                    "No free material texture slots (maximum 121)."));
                break;
            }
        }
        declaredTextures << texName;
        usedSlots << slot;
        textureDecls.append({QStringLiteral("Texture2D %1 : register(t%2);").arg(texName).arg(slot), {n.id}});
    }

    // Рекурсивный генератор выражений для пина. Узлы на текущем пути отслеживаются:
    // цикл из повреждённого файла даёт 0 вместо бесконечной рекурсии.
    QSet<int> path;
    QList<int> touched, defaultsNodes; // ноды выражения текущего выхода; ноды SurfaceDefaults
    std::function<Value(int, const QString&)> evalPin;
    // Вход ноды: связанное выражение или значение по умолчанию.
    auto input = [&](int nodeId, const char* pin, const Value& fallback) -> Value {
        for (const auto& c : connections)
            if (c.toNode == nodeId && c.toPin == QLatin1String(pin)) return evalPin(c.fromNode, c.fromPin);
        return fallback;
    };
    const Value zero{QStringLiteral("0.0f"), 1}, one{QStringLiteral("1.0f"), 1};

    evalPin = [&](int nodeId, const QString& pinName) -> Value
    {
        const GraphNodeData* found = model.FindNode(nodeId);
        if (!found || path.contains(nodeId)) return zero;
        const GraphNodeData& n = *found;
        path.insert(nodeId);
        if (!touched.contains(nodeId)) touched << nodeId;
        struct Leave { QSet<int>& path; int id; ~Leave() { path.remove(id); } } leave{path, nodeId};
        const auto pin = [&](const char* name) { return pinName == QLatin1String(name); };
        const auto prop = [&](const char* key, const QVariant& fallback) { return n.properties.value(QLatin1String(key), fallback); };

        if (n.type == QStringLiteral("ColorParam"))
        {
            const QString name = prop("name", QStringLiteral("Color")).toString();
            if (pin("RGB")) return {name + QStringLiteral(".rgb"), 3};
            if (pin("A")) return {name + QStringLiteral(".a"), 1};
            return {name, 4};
        }
        if (n.type == QStringLiteral("ScalarParam")) return {prop("name", QStringLiteral("Param")).toString(), 1};
        if (n.type == QStringLiteral("Vector2Param")) return {prop("name", QStringLiteral("Vector")).toString(), 2};
        if (n.type == QStringLiteral("Vector3Param")) return {prop("name", QStringLiteral("Vector")).toString(), 3};
        if (n.type == QStringLiteral("Constant")) return {F4(prop("value", 1.0).toDouble()) + QLatin1Char('f'), 1};

        if (n.type == QStringLiteral("ObjectColor"))
        {
            if (pin("RGB")) return {QStringLiteral("i.Color.rgb"), 3};
            if (pin("A")) return {QStringLiteral("i.Color.a"), 1};
            return {QStringLiteral("i.Color"), 4};
        }
        if (n.type == QStringLiteral("SurfaceDefaults"))
        {
            usesDefaults = true;
            if (!defaultsNodes.contains(nodeId)) defaultsNodes << nodeId;
            if (pin("Alpha")) return {QStringLiteral("defaults.Alpha"), 1};
            if (pin("Normal")) return {QStringLiteral("defaults.Normal"), 3};
            return {QStringLiteral("defaults.Albedo"), 3};
        }
        if (n.type == QStringLiteral("InstanceData"))
        {
            if (pin("X")) return {QStringLiteral("i.InstanceData.x"), 1};
            if (pin("Y")) return {QStringLiteral("i.InstanceData.y"), 1};
            if (pin("Z")) return {QStringLiteral("i.InstanceData.z"), 1};
            return {QStringLiteral("i.InstanceData"), 3};
        }
        if (n.type == QStringLiteral("WorldPosition")) return {QStringLiteral("i.WorldPos"), 3};
        if (n.type == QStringLiteral("SurfaceNormal")) return {QStringLiteral("i.Normal"), 3};
        if (n.type == QStringLiteral("ViewDirection")) return {QStringLiteral("i.ViewDir"), 3};
        if (n.type == QStringLiteral("FrontFace")) return {QStringLiteral("(i.FrontFace ? 1.0f : 0.0f)"), 1};
        if (n.type == QStringLiteral("Time"))
            return {pin("SineTime") ? QStringLiteral("sin(i.Time)") : QStringLiteral("i.Time"), 1};
        if (n.type == QStringLiteral("UVCoord"))
        {
            const double tx = prop("tilingX", 1.0).toDouble(), ty = prop("tilingY", 1.0).toDouble();
            return {QStringLiteral("(i.UV * float2(%1f, %2f))").arg(F2(tx), F2(ty)), 2};
        }
        if (n.type == QStringLiteral("Panner"))
        {
            const double sx = prop("speedX", 0.5).toDouble(), sy = prop("speedY", 0.0).toDouble();
            const QString uv = Convert(input(nodeId, "UV", {QStringLiteral("i.UV"), 2}), 2);
            return {QStringLiteral("(%1 + i.Time * float2(%2f, %3f))").arg(uv, F2(sx), F2(sy)), 2};
        }
        if (n.type == QStringLiteral("Texture2D"))
        {
            const QString texName = prop("textureName", QStringLiteral("BaseMap")).toString();
            const QString uv = Convert(input(nodeId, "UV", {QStringLiteral("i.UV"), 2}), 2);
            const QString sample = QStringLiteral("%1.Sample(MaterialSampler, %2)").arg(texName, uv);
            if (pin("RGB")) return {sample + QStringLiteral(".rgb"), 3};
            if (pin("R")) return {sample + QStringLiteral(".r"), 1};
            if (pin("G")) return {sample + QStringLiteral(".g"), 1};
            if (pin("B")) return {sample + QStringLiteral(".b"), 1};
            if (pin("A")) return {sample + QStringLiteral(".a"), 1};
            return {sample, 4};
        }
        if (n.type == QStringLiteral("NormalMap"))
        {
            const QString texName = prop("textureName", QStringLiteral("NormalMap")).toString();
            const QString uv = Convert(input(nodeId, "UV", {QStringLiteral("i.UV"), 2}), 2);
            const QString strength = Convert(input(nodeId, "Strength", one), 1);
            // Мировая нормаль: базис касательных из производных, тангенты в вершинах не нужны.
            return {QStringLiteral("NormalFromMap(i, %1.Sample(MaterialSampler, %2).xyz, %3)").arg(texName, uv, strength), 3};
        }

        if (n.type == QStringLiteral("Split"))
        {
            const int index = QStringLiteral("RGBA").indexOf(pinName);
            return {Component(input(nodeId, "In", zero), std::max(0, index)), 1};
        }
        if (n.type == QStringLiteral("Combine"))
        {
            const QString r = Convert(input(nodeId, "R", zero), 1), g = Convert(input(nodeId, "G", zero), 1);
            if (pin("RG")) return {QStringLiteral("float2(%1, %2)").arg(r, g), 2};
            const QString b = Convert(input(nodeId, "B", zero), 1);
            if (pin("RGB")) return {QStringLiteral("float3(%1, %2, %3)").arg(r, g, b), 3};
            return {QStringLiteral("float4(%1, %2, %3, %4)").arg(r, g, b, Convert(input(nodeId, "A", one), 1)), 4};
        }

        // Математика: размер результата — наибольший из входов, остальные приводятся к нему.
        static const QHash<QString, QString> binary{
            {QStringLiteral("Add"), QStringLiteral("(%1 + %2)")}, {QStringLiteral("Subtract"), QStringLiteral("(%1 - %2)")},
            {QStringLiteral("Multiply"), QStringLiteral("(%1 * %2)")}, {QStringLiteral("Divide"), QStringLiteral("(%1 / %2)")},
            {QStringLiteral("Power"), QStringLiteral("pow(%1, %2)")}, {QStringLiteral("Min"), QStringLiteral("min(%1, %2)")},
            {QStringLiteral("Max"), QStringLiteral("max(%1, %2)")}, {QStringLiteral("Step"), QStringLiteral("step(%1, %2)")}};
        if (binary.contains(n.type))
        {
            const bool power = n.type == QStringLiteral("Power"), step = n.type == QStringLiteral("Step");
            const bool identity = n.type == QStringLiteral("Multiply") || n.type == QStringLiteral("Divide") || power;
            const Value a = input(nodeId, power ? "Base" : step ? "Edge" : "A", identity && !power ? one : zero);
            const Value b = input(nodeId, power ? "Exp" : step ? "In" : "B", identity ? one : zero);
            const int size = std::max(a.size, b.size);
            return {binary.value(n.type).arg(Convert(a, size), Convert(b, size)), size};
        }
        static const QHash<QString, QString> unary{
            {QStringLiteral("Saturate"), QStringLiteral("saturate(%1)")}, {QStringLiteral("OneMinus"), QStringLiteral("(1.0f - %1)")},
            {QStringLiteral("Abs"), QStringLiteral("abs(%1)")}, {QStringLiteral("Frac"), QStringLiteral("frac(%1)")},
            {QStringLiteral("Sine"), QStringLiteral("sin(%1)")}, {QStringLiteral("Normalize"), QStringLiteral("normalize(%1)")}};
        if (unary.contains(n.type))
        {
            const Value in = input(nodeId, "In", n.type == QStringLiteral("Sine") ? Value{QStringLiteral("i.Time"), 1} : zero);
            // normalize скаляра — это его знак
            if (n.type == QStringLiteral("Normalize") && in.size == 1) return {QStringLiteral("sign(%1)").arg(in.expr), 1};
            return {unary.value(n.type).arg(in.expr), in.size};
        }
        if (n.type == QStringLiteral("Lerp"))
        {
            const Value a = input(nodeId, "A", zero), b = input(nodeId, "B", one), alpha = input(nodeId, "Alpha", {QStringLiteral("0.5f"), 1});
            const int size = std::max(a.size, b.size);
            return {QStringLiteral("lerp(%1, %2, %3)").arg(Convert(a, size), Convert(b, size), alpha.size == 1 ? alpha.expr : Convert(alpha, size)), size};
        }
        if (n.type == QStringLiteral("Clamp") || n.type == QStringLiteral("Smoothstep"))
        {
            const bool clamp = n.type == QStringLiteral("Clamp");
            const Value in = input(nodeId, "In", zero), lo = input(nodeId, "Min", zero), hi = input(nodeId, "Max", one);
            const int size = std::max({in.size, lo.size, hi.size});
            return {clamp ? QStringLiteral("clamp(%1, %2, %3)").arg(Convert(in, size), Convert(lo, size), Convert(hi, size))
                          : QStringLiteral("smoothstep(%1, %2, %3)").arg(Convert(lo, size), Convert(hi, size), Convert(in, size)), size};
        }
        if (n.type == QStringLiteral("Dot"))
        {
            const Value a = input(nodeId, "A", zero), b = input(nodeId, "B", zero);
            const int size = std::max(a.size, b.size);
            if (size == 1) return {QStringLiteral("(%1 * %2)").arg(a.expr, b.expr), 1};
            return {QStringLiteral("dot(%1, %2)").arg(Convert(a, size), Convert(b, size)), 1};
        }
        if (n.type == QStringLiteral("Fresnel"))
        {
            const QString power = Convert(input(nodeId, "Power", {QStringLiteral("3.0f"), 1}), 1);
            return {QStringLiteral("pow(1.0f - saturate(dot(i.Normal, i.ViewDir)), %1)").arg(power), 1};
        }
        return zero;
    };

    // Выход материала: входы — поля MaterialOutput освещения движка (Блинн-Фонг).
    // Неподключённый вход не пишется: поле сохраняет значение шаблона (цвет и текстура компонента и т.д.).
    struct OutputPin
    {
        const char* pin;
        const char* field;
        int size;
        bool saturate;
    };
    static const OutputPin outputs[] = {
        {"Albedo", "Albedo", 3, true}, {"Alpha", "Alpha", 1, true}, {"Specular", "SpecularWeight", 1, true},
        {"AmbientModel", "AmbientModel", 1, true}, {"Emissive", "Emissive", 3, false}, {"Normal", "Normal", 3, false}};

    int outputId = 0;
    for (const auto& n : nodes)
    {
        if (n.type == QStringLiteral("MaterialOutput"))
        {
            outputId = n.id;
            break;
        }
    }

    struct Assignment
    {
        QString text;
        QList<int> nodes;
        QString pin;
    };
    QVector<Assignment> assignments;
    for (const auto& output : outputs)
    {
        for (const auto& c : connections)
        {
            if (outputId == 0 || c.toNode != outputId || c.toPin != QLatin1String(output.pin))
                continue;
            touched.clear();
            const QString expr = Convert(evalPin(c.fromNode, c.fromPin), output.size);
            assignments.append({output.saturate ? QStringLiteral("    o.%1 = saturate(%2);").arg(QLatin1String(output.field), expr)
                                                : QStringLiteral("    o.%1 = %2;").arg(QLatin1String(output.field), expr),
                                QList<int>{outputId} + touched, QString::fromLatin1(output.pin)});
            hasEmissive = hasEmissive || QLatin1String(output.pin) == QLatin1String("Emissive");
        }
    }
    // Значения шаблона до записей графа: нода SurfaceDefaults читает их, а не уже изменённые поля.
    if (usesDefaults)
        assignments.prepend({QStringLiteral("    const MaterialOutput defaults = o;"), defaultsNodes, {}});

    // Строки по одной: для каждой запоминаются ноды, из которых она собрана.
    QStringList lines;
    const auto line = [&](const QString& text, const QList<int>& sources = {}, const QString& pin = {}) {
        lines << text;
        const int number = static_cast<int>(lines.size());
        if (!sources.isEmpty()) result.lineNodes.insert(number, sources);
        if (!pin.isEmpty()) result.lineOutputs.insert(number, pin);
    };
    line(QStringLiteral("// ========================================================="));
    line(QStringLiteral("// Generated by PainEngine Material Node Graph"));
    line(QStringLiteral("// ========================================================="));
    line({});

    if (hasEmissive)
    {
        line(QStringLiteral("#define MATERIAL_EMISSIVE 1"), {outputId}, QStringLiteral("Emissive"));
        line({});
    }

    if (!cbufferParams.isEmpty())
    {
        line(QStringLiteral("cbuffer MaterialParams : register(b1)"));
        line(QStringLiteral("{"));
        for (const auto& [text, id] : cbufferParams) line(text, {id});
        line(QStringLiteral("};"));
        line({});
    }

    if (!textureDecls.isEmpty())
    {
        for (const auto& [text, ids] : textureDecls) line(text, ids);
        line({});
    }

    line(QStringLiteral("void Surface(MaterialInput i, inout MaterialOutput o)"));
    line(QStringLiteral("{"));
    for (const auto& assignment : assignments) line(assignment.text, assignment.nodes, assignment.pin);
    line(QStringLiteral("}"));

    result.hlsl = lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
    return result;
}

} // namespace EditorCore
