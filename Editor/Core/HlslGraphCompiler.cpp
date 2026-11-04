#include "HlslGraphCompiler.h"

#include <QColor>
#include <QLocale>
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
    }

    // Текстуры: ноды с одним именем читают одну текстуру; занятый регистр заменяется
    // следующим свободным (t0-t5 принадлежат движку).
    QStringList declaredTextures;
    QList<int> usedSlots;
    for (const auto& n : nodes)
    {
        if (!IsTextureNode(n.type))
            continue;
        const QString texName = n.properties.value(QStringLiteral("textureName"), DefaultTextureName(n.type)).toString();
        if (declaredTextures.contains(texName))
            continue;
        int slot = std::max(6, n.properties.value(QStringLiteral("slot"), 6).toInt());
        while (usedSlots.contains(slot))
            ++slot;
        declaredTextures << texName;
        usedSlots << slot;
        textureDecls << QStringLiteral("Texture2D %1 : register(t%2);").arg(texName).arg(slot);
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
                if (n.type == QStringLiteral("NormalMap"))
                {
                    QString texName = n.properties.value(QStringLiteral("textureName"), QStringLiteral("NormalMap")).toString();
                    QString uvExpr = QStringLiteral("i.UV"), strength = QStringLiteral("1.0f");
                    for (const auto& c : connections)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("UV")) uvExpr = evalPin(c.fromNode, c.fromPin);
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("Strength")) strength = evalPin(c.fromNode, c.fromPin);
                    }
                    // Мировая нормаль: базис касательных из производных, тангенты в вершинах не нужны.
                    return QStringLiteral("NormalFromMap(i, %1.Sample(MaterialSampler, %2).xyz, %3)").arg(texName, uvExpr, strength);
                }
                if (n.type == QStringLiteral("Panner"))
                {
                    double sx = n.properties.value(QStringLiteral("speedX"), 0.5).toDouble();
                    double sy = n.properties.value(QStringLiteral("speedY"), 0.0).toDouble();
                    QString uvExpr = QStringLiteral("i.UV");
                    for (const auto& c : connections)
                    {
                        if (c.toNode == nodeId && c.toPin == QStringLiteral("UV"))
                        {
                            uvExpr = evalPin(c.fromNode, c.fromPin);
                            break;
                        }
                    }
                    return QStringLiteral("(%1 + i.Time * float2(%2f, %3f))").arg(uvExpr, F2(sx), F2(sy));
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

    // Выход материала: входы — поля MaterialOutput освещения движка (Блинн-Фонг).
    // Неподключённый вход не пишется: поле сохраняет значение шаблона (цвет и текстура компонента и т.д.).
    struct OutputPin
    {
        const char* pin;
        const char* field;
        bool saturate;
    };
    static const OutputPin outputs[] = {
        {"Albedo", "Albedo", true}, {"Alpha", "Alpha", true}, {"Specular", "SpecularWeight", true},
        {"AmbientModel", "AmbientModel", true}, {"Emissive", "Emissive", false}, {"Normal", "Normal", false}};

    int outputId = 0;
    for (const auto& n : nodes)
    {
        if (n.type == QStringLiteral("MaterialOutput"))
        {
            outputId = n.id;
            break;
        }
    }

    QStringList assignments;
    for (const auto& output : outputs)
    {
        for (const auto& c : connections)
        {
            if (outputId == 0 || c.toNode != outputId || c.toPin != QLatin1String(output.pin))
                continue;
            const QString expr = evalPin(c.fromNode, c.fromPin);
            assignments << (output.saturate ? QStringLiteral("    o.%1 = saturate(%2);").arg(QLatin1String(output.field), expr)
                                            : QStringLiteral("    o.%1 = %2;").arg(QLatin1String(output.field), expr));
            hasEmissive = hasEmissive || QLatin1String(output.pin) == QLatin1String("Emissive");
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
    for (const QString& line : assignments)
        hlsl += line + QLatin1Char('\n');
    hlsl += QStringLiteral("}\n");

    return hlsl;
}

} // namespace EditorCore
