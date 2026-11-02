#include "HlslGraphCompiler.h"

#include <QColor>
#include <QLocale>
#include <QStringList>
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
} // namespace

QString HlslGraphCompiler::CompileGraphToHlsl(const MaterialGraphModel& model)
{
    QStringList cbufferParams;
    QStringList textureDecls;
    bool hasEmissive = false;

    const auto& nodes = model.Nodes();
    const auto& connections = model.Connections();

    // Сбор параметров и текстур
    for (const auto& n : nodes)
    {
        if (n.type == QStringLiteral("ColorParam"))
        {
            QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Color_%1").arg(n.id)).toString();
            QColor col = n.properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>();
            cbufferParams << QStringLiteral("    float4 %1 = float4(%2f, %3f, %4f, %5f);")
                                .arg(name)
                                .arg(F4(col.redF()))
                                .arg(F4(col.greenF()))
                                .arg(F4(col.blueF()))
                                .arg(F4(col.alphaF()));
        }
        else if (n.type == QStringLiteral("ScalarParam"))
        {
            QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Param_%1").arg(n.id)).toString();
            double val = n.properties.value(QStringLiteral("value"), 0.5).toDouble();
            cbufferParams << QStringLiteral("    float %1 = %2f;").arg(name).arg(F4(val));
        }
        else if (n.type == QStringLiteral("Texture2D"))
        {
            QString texName = n.properties.value(QStringLiteral("textureName"), QStringLiteral("BaseMap")).toString();
            int slot = n.properties.value(QStringLiteral("slot"), 6).toInt();
            textureDecls << QStringLiteral("Texture2D %1 : register(t%2);").arg(texName).arg(slot);
        }
        else if (n.type == QStringLiteral("ORMTexture"))
        {
            QString texName = n.properties.value(QStringLiteral("textureName"), QStringLiteral("OrmMap")).toString();
            int slot = n.properties.value(QStringLiteral("slot"), 7).toInt();
            textureDecls << QStringLiteral("Texture2D %1 : register(t%2);").arg(texName).arg(slot);
        }
    }

    // Рекурсивный генератор выражений для пина
    std::function<QString(int nodeId, const QString& pinName)> evalPin;
    evalPin = [&nodes, &connections, &evalPin](int nodeId, const QString& pinName) -> QString
    {
        for (const auto& n : nodes)
        {
            if (n.id == nodeId)
            {
                if (n.type == QStringLiteral("ColorParam"))
                {
                    QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Color")).toString();
                    if (pinName == QStringLiteral("RGB")) return name + QStringLiteral(".rgb");
                    if (pinName == QStringLiteral("A")) return name + QStringLiteral(".a");
                    return name;
                }
                if (n.type == QStringLiteral("ScalarParam"))
                {
                    return n.properties.value(QStringLiteral("name"), QStringLiteral("Param")).toString();
                }
                if (n.type == QStringLiteral("Time"))
                {
                    if (pinName == QStringLiteral("SineTime")) return QStringLiteral("sin(i.Time)");
                    return QStringLiteral("i.Time");
                }
                if (n.type == QStringLiteral("UVCoord"))
                {
                    double tx = n.properties.value(QStringLiteral("tilingX"), 1.0).toDouble();
                    double ty = n.properties.value(QStringLiteral("tilingY"), 1.0).toDouble();
                    return QStringLiteral("(i.UV * float2(%1f, %2f))").arg(F2(tx)).arg(F2(ty));
                }
                if (n.type == QStringLiteral("Texture2D"))
                {
                    QString texName = n.properties.value(QStringLiteral("textureName"), QStringLiteral("BaseMap")).toString();
                    QString uvExpr = QStringLiteral("i.UV");
                    for (const auto& c : connections)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("UV"))
                        {
                            uvExpr = evalPin(c.fromNode, c.fromPin);
                            break;
                        }
                    }
                    QString sampleExpr = QStringLiteral("%1.Sample(MaterialSampler, %2)").arg(texName, uvExpr);
                    if (pinName == QStringLiteral("RGB")) return sampleExpr + QStringLiteral(".rgb");
                    if (pinName == QStringLiteral("A")) return sampleExpr + QStringLiteral(".a");
                    return sampleExpr;
                }
                if (n.type == QStringLiteral("ORMTexture"))
                {
                    QString texName = n.properties.value(QStringLiteral("textureName"), QStringLiteral("OrmMap")).toString();
                    QString uvExpr = QStringLiteral("i.UV");
                    for (const auto& c : connections)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("UV"))
                        {
                            uvExpr = evalPin(c.fromNode, c.fromPin);
                            break;
                        }
                    }
                    QString sampleExpr = QStringLiteral("%1.Sample(MaterialSampler, %2)").arg(texName, uvExpr);
                    if (pinName == QStringLiteral("AO")) return sampleExpr + QStringLiteral(".r");
                    if (pinName == QStringLiteral("Roughness")) return sampleExpr + QStringLiteral(".g");
                    if (pinName == QStringLiteral("Metallic")) return sampleExpr + QStringLiteral(".b");
                    return sampleExpr;
                }
                if (n.type == QStringLiteral("Multiply"))
                {
                    QString a = QStringLiteral("1.0f"), b = QStringLiteral("1.0f");
                    for (const auto& c : connections)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("A")) a = evalPin(c.fromNode, c.fromPin);
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("B")) b = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("(%1 * %2)").arg(a, b);
                }
                if (n.type == QStringLiteral("Add"))
                {
                    QString a = QStringLiteral("0.0f"), b = QStringLiteral("0.0f");
                    for (const auto& c : connections)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("A")) a = evalPin(c.fromNode, c.fromPin);
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("B")) b = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("(%1 + %2)").arg(a, b);
                }
                if (n.type == QStringLiteral("Lerp"))
                {
                    QString a = QStringLiteral("0.0f"), b = QStringLiteral("1.0f"), alpha = QStringLiteral("0.5f");
                    for (const auto& c : connections)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("A")) a = evalPin(c.fromNode, c.fromPin);
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("B")) b = evalPin(c.fromNode, c.fromPin);
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("Alpha")) alpha = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("lerp(%1, %2, %3)").arg(a, b, alpha);
                }
                if (n.type == QStringLiteral("OneMinus"))
                {
                    QString inVal = QStringLiteral("0.0f");
                    for (const auto& c : connections)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("In")) inVal = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("(1.0f - %1)").arg(inVal);
                }
                if (n.type == QStringLiteral("Sine"))
                {
                    QString inVal = QStringLiteral("i.Time");
                    for (const auto& c : connections)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("In")) inVal = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("sin(%1)").arg(inVal);
                }
                if (n.type == QStringLiteral("Fresnel"))
                {
                    QString power = QStringLiteral("3.0f");
                    for (const auto& c : connections)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("Power")) power = evalPin(c.fromNode, c.fromPin);
                    }
                    return QStringLiteral("pow(1.0f - saturate(dot(i.Normal, i.ViewDir)), %1)").arg(power);
                }
                break;
            }
        }
        return QStringLiteral("0.0f");
    };

    // Находим Master Node
    int masterId = 0;
    for (const auto& n : nodes)
    {
        if (n.type == QStringLiteral("PBRMasterOutput"))
        {
            masterId = n.id;
            break;
        }
    }

    QString baseColorExpr = QStringLiteral("float3(0.8, 0.8, 0.8)");
    QString metallicExpr = QStringLiteral("0.0f");
    QString roughnessExpr = QStringLiteral("0.5f");
    QString normalExpr;
    QString emissiveExpr;
    QString aoExpr = QStringLiteral("1.0f");
    QString alphaExpr = QStringLiteral("1.0f");

    if (masterId != 0)
    {
        for (const auto& c : connections)
        {
            if (c.toNode == masterId)
            {
                if (c.toPin == QStringLiteral("BaseColor")) baseColorExpr = evalPin(c.fromNode, c.fromPin);
                else if (c.toPin == QStringLiteral("Metallic")) metallicExpr = evalPin(c.fromNode, c.fromPin);
                else if (c.toPin == QStringLiteral("Roughness")) roughnessExpr = evalPin(c.fromNode, c.fromPin);
                else if (c.toPin == QStringLiteral("Normal")) normalExpr = evalPin(c.fromNode, c.fromPin);
                else if (c.toPin == QStringLiteral("Emissive"))
                {
                    emissiveExpr = evalPin(c.fromNode, c.fromPin);
                    hasEmissive = true;
                }
                else if (c.toPin == QStringLiteral("AO")) aoExpr = evalPin(c.fromNode, c.fromPin);
                else if (c.toPin == QStringLiteral("Alpha")) alphaExpr = evalPin(c.fromNode, c.fromPin);
            }
        }
    }

    QString hlsl;
    hlsl += QStringLiteral("// =========================================================\n");
    hlsl += QStringLiteral("// Generated by PainEngine Material Node Graph\n");
    hlsl += QStringLiteral("// =========================================================\n\n");

    if (hasEmissive)
    {
        hlsl += QStringLiteral("#define MATERIAL_EMISSIVE 1\n\n");
    }

    if (!cbufferParams.isEmpty())
    {
        hlsl += QStringLiteral("cbuffer MaterialParams : register(b1)\n{\n");
        hlsl += cbufferParams.join(QStringLiteral("\n")) + QStringLiteral("\n");
        hlsl += QStringLiteral("};\n\n");
    }

    if (!textureDecls.isEmpty())
    {
        hlsl += textureDecls.join(QStringLiteral("\n")) + QStringLiteral("\n\n");
    }

    hlsl += QStringLiteral("void Surface(MaterialInput i, inout MaterialOutput o)\n{\n");
    hlsl += QStringLiteral("    o.Albedo = saturate(%1);\n").arg(baseColorExpr);
    hlsl += QStringLiteral("    o.SpecularWeight = saturate(1.0f - (%1));\n").arg(roughnessExpr);
    hlsl += QStringLiteral("    o.AmbientModel = saturate(%1);\n").arg(aoExpr);
    hlsl += QStringLiteral("    o.Alpha = saturate(%1);\n").arg(alphaExpr);

    if (!normalExpr.isEmpty())
    {
        hlsl += QStringLiteral("    o.Normal = %1;\n").arg(normalExpr);
    }
    if (!emissiveExpr.isEmpty())
    {
        hlsl += QStringLiteral("    o.Emissive = %1;\n").arg(emissiveExpr);
    }

    hlsl += QStringLiteral("}\n");

    return hlsl;
}

} // namespace EditorCore
