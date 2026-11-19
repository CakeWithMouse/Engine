#include "MaterialGraphTransform.h"
#include "HlslGraphCompiler.h"

#include <QColor>
#include <QJsonArray>
#include <QSet>
#include <QString>

namespace EditorCore
{

namespace
{
QString UniqueParamName(const GraphDocument& model, QString base)
{
    if (base.isEmpty() || base == QStringLiteral("Constant") || !HlslGraphCompiler::NameProblem(base).isEmpty())
        base = QStringLiteral("Param");
    QSet<QString> used;
    for (const auto& n : model.Nodes())
    {
        QString name;
        if (n.type.endsWith(QStringLiteral("Param"))) name = n.properties.value(QStringLiteral("name")).toString();
        else if (n.type == QStringLiteral("Texture2D") || n.type == QStringLiteral("NormalMap"))
            name = n.properties.value(QStringLiteral("textureName")).toString();
        if (!name.isEmpty()) used.insert(name);
    }
    QString candidate = base;
    int suffix = 2;
    while (used.contains(candidate)) candidate = base + QStringLiteral("_%1").arg(suffix++);
    return candidate;
}
} // namespace

bool MaterialGraphTransform::CanExpose(const GraphDocument& model, const QList<int>& ids)
{
    for (int id : ids)
    {
        const GraphNodeData* node = model.FindNode(id);
        if (node && node->type == QStringLiteral("Constant")) return true;
    }
    return false;
}

MaterialGraphModel MaterialGraphTransform::ExposeConstants(const GraphDocument& model, const QList<int>& ids)
{
    MaterialGraphModel result(model);
    for (int id : ids)
    {
        GraphNodeData* node = result.FindNode(id);
        if (!node || node->type != QStringLiteral("Constant")) continue;
        const QString name = UniqueParamName(result, node->title);
        // Constant.Value и ScalarParam.Value — один пин: связи не меняются.
        node->type = QStringLiteral("ScalarParam");
        node->title = name;
        node->properties[QStringLiteral("name")] = name;
        if (!node->properties.contains(QStringLiteral("value")))
            node->properties[QStringLiteral("value")] = 1.0;
    }
    return result;
}

QJsonObject MaterialGraphTransform::ExtractDefaultParams(const GraphDocument& model)
{
    QJsonObject params;
    for (const auto& n : model.Nodes())
    {
        if (n.type == QStringLiteral("ScalarParam"))
        {
            const QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Param_%1").arg(n.id)).toString();
            const double val = n.properties.value(QStringLiteral("value"), 0.5).toDouble();
            if (!name.isEmpty())
                params.insert(name, val);
        }
        else if (n.type == QStringLiteral("ColorParam"))
        {
            const QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Color_%1").arg(n.id)).toString();
            const QColor col = n.properties.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>();
            if (!name.isEmpty())
            {
                QJsonArray arr;
                arr.append(col.redF());
                arr.append(col.greenF());
                arr.append(col.blueF());
                arr.append(col.alphaF());
                params.insert(name, arr);
            }
        }
        else if (n.type == QStringLiteral("Vector2Param") || n.type == QStringLiteral("Vector3Param"))
        {
            const int size = n.type == QStringLiteral("Vector2Param") ? 2 : 3;
            const QString name = n.properties.value(QStringLiteral("name"), QStringLiteral("Vector_%1").arg(n.id)).toString();
            const QVariantList stored = n.properties.value(QStringLiteral("value")).toList();
            if (!name.isEmpty())
            {
                QJsonArray arr;
                for (int i = 0; i < size; ++i)
                    arr.append(i < stored.size() ? stored[i].toDouble() : 0.0);
                params.insert(name, arr);
            }
        }
    }
    return params;
}

} // namespace EditorCore