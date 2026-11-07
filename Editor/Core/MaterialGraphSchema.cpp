#include "MaterialGraphSchema.h"
#include "EditorLocale.h"
#include "HlslGraphCompiler.h"

#include <QHash>
#include <algorithm>

namespace EditorCore
{

namespace
{
const QString Master = QStringLiteral("MaterialOutput");

// Группы нод: из них строятся меню добавления и по ним красятся заголовки.
struct NodeGroup
{
    const char* ru;
    const char* en;
    QStringList types;
    QColor color;
};

const QVector<NodeGroup>& NodeGroups()
{
    static const QVector<NodeGroup> groups{
        {"Параметры", "Parameters", {"ColorParam", "ScalarParam", "Vector2Param", "Vector3Param"}, QColor(0x15, 0x80, 0x3D)},
        {"Константы и каналы", "Constants and channels", {"Constant", "Split", "Combine"}, QColor(0x4D, 0x7C, 0x0F)},
        {"Входы поверхности", "Surface inputs",
         {"ObjectColor", "SurfaceDefaults", "InstanceData", "WorldPosition", "SurfaceNormal", "ViewDirection", "FrontFace"}, QColor(0xA1, 0x62, 0x07)},
        {"Текстуры", "Textures", {"Texture2D", "NormalMap"}, QColor(0x1D, 0x4E, 0xD8)},
        {"Координаты и время", "Coordinates and time", {"UVCoord", "Panner", "Time"}, QColor(0x0E, 0x74, 0x90)},
        {"Математика", "Math",
         {"Add", "Subtract", "Multiply", "Divide", "Power", "Min", "Max", "Lerp", "Clamp", "Saturate", "OneMinus",
          "Abs", "Frac", "Sine", "Normalize", "Step", "Smoothstep", "Dot", "Fresnel"}, QColor(0x7E, 0x22, 0xCE)}};
    return groups;
}

// Имя ноды для людей, что она делает и слова для поиска (кроме имени типа и группы).
struct NodeInfo
{
    const char* ru;
    const char* hintRu;
    const char* hintEn;
    const char* keywords;
};

const QHash<QString, NodeInfo>& NodeInfos()
{
    static const QHash<QString, NodeInfo> infos{
        {QStringLiteral("MaterialOutput"), {"Выход материала", "Поверхность материала: цвет, прозрачность, блик, свечение, нормаль.",
                      "The material surface: colour, opacity, highlight, glow, normal.", "output result"}},
        {"ColorParam", {"Цвет (параметр)", "Цвет, который настраивается в материале без перекомпиляции.",
                        "A colour tweaked in the material without recompiling.", "colour tint rgb оттенок"}},
        {"ScalarParam", {"Число (параметр)", "Число, которое настраивается в материале без перекомпиляции.",
                         "A number tweaked in the material without recompiling.", "float value scalar значение"}},
        {"Vector2Param", {"Вектор 2 (параметр)", "Два числа, которые настраиваются в материале.", "Two numbers tweaked in the material.", "float2 vector"}},
        {"Vector3Param", {"Вектор 3 (параметр)", "Три числа, которые настраиваются в материале.", "Three numbers tweaked in the material.", "float3 vector"}},
        {"Constant", {"Константа", "Число, вписанное в шейдер.", "A number written into the shader.", "number value число"}},
        {"Split", {"Разделить каналы", "Компоненты вектора по отдельности: R, G, B, A.", "Vector components one by one: R, G, B, A.", "channel mask swizzle канал"}},
        {"Combine", {"Собрать вектор", "Вектор из отдельных чисел.", "A vector from separate numbers.", "append merge channel канал"}},
        {"ObjectColor", {"Цвет объекта", "Цвет компонента объекта (у копий — цвет копии).", "Colour of the object's component (per copy when instanced).", "vertex colour tint"}},
        {"SurfaceDefaults", {"Поверхность по умолчанию", "Цвет объекта × его текстура, альфа и нормаль до правок графа.",
                             "Object colour times its texture, alpha and normal before the graph.", "default base"}},
        {"InstanceData", {"Данные копии", "Данные копии меша при инстансинге.", "Per-copy data of instanced meshes.", "instance custom"}},
        {"WorldPosition", {"Позиция в мире", "Мировые координаты точки поверхности.", "World position of the surface point.", "world position координаты"}},
        {"SurfaceNormal", {"Нормаль поверхности", "Нормаль в мировых координатах.", "World-space normal.", "normal"}},
        {"ViewDirection", {"Направление на камеру", "Единичный вектор от поверхности к камере.", "Unit vector from the surface to the camera.", "camera view eye"}},
        {"FrontFace", {"Лицевая сторона", "1 на лицевой стороне, 0 на обратной (двусторонние материалы).",
                       "1 on the front face, 0 on the back (two-sided materials).", "two sided back face"}},
        {"Texture2D", {"Текстура", "Цвет из текстуры по UV.", "Colour from a texture at UV.", "sample image map картинка"}},
        {"NormalMap", {"Карта нормалей", "Нормаль из карты нормалей, для входа Normal выхода материала.",
                       "Normal from a normal map, for the Normal input of the output.", "bump normal рельеф"}},
        {"UVCoord", {"UV-координаты", "Текстурные координаты с тайлингом.", "Texture coordinates with tiling.", "texcoord tiling тайлинг"}},
        {"Panner", {"Сдвиг UV", "UV, которые сдвигаются со временем: бегущая текстура.", "UV moving over time: a scrolling texture.", "scroll pan move прокрутка"}},
        {"Time", {"Время", "Секунды с запуска и их синус.", "Seconds since start and their sine.", "animation анимация"}},
        {"Add", {"Сложение", "A + B.", "A + B.", "plus sum плюс"}},
        {"Subtract", {"Вычитание", "A − B.", "A - B.", "minus минус"}},
        {"Multiply", {"Умножение", "A × B: например, цвет × текстура.", "A * B, e.g. colour times texture.", "times product"}},
        {"Divide", {"Деление", "A / B.", "A / B.", "ratio"}},
        {"Power", {"Степень", "Base в степени Exp.", "Base to the power Exp.", "pow exponent"}},
        {"Min", {"Минимум", "Меньшее из A и B.", "The smaller of A and B.", "minimum меньше"}},
        {"Max", {"Максимум", "Большее из A и B.", "The larger of A and B.", "maximum больше"}},
        {"Lerp", {"Смешивание (lerp)", "A при Alpha = 0, B при Alpha = 1, между ними — плавно.",
                  "A at Alpha 0, B at Alpha 1, blended in between.", "mix blend interpolate смешать"}},
        {"Clamp", {"Ограничение (clamp)", "In, зажатое между Min и Max.", "In clamped between Min and Max.", "limit предел"}},
        {"Saturate", {"Ограничение 0..1", "In, зажатое между 0 и 1.", "In clamped to 0..1.", "saturate clamp"}},
        {"OneMinus", {"1 − x", "Инверсия: 1 − In.", "Inversion: 1 - In.", "invert one minus инверсия"}},
        {"Abs", {"Модуль", "Абсолютное значение.", "Absolute value.", "absolute"}},
        {"Frac", {"Дробная часть", "Дробная часть: повторение 0..1.", "Fractional part: repeats 0..1.", "fraction repeat"}},
        {"Sine", {"Синус", "sin(In); без входа — пульсация по времени.", "sin(In); without input it pulses over time.", "sin wave волна"}},
        {"Normalize", {"Нормализация", "Вектор единичной длины.", "Vector of unit length.", "unit length"}},
        {"Step", {"Порог (step)", "0, если In < Edge, иначе 1.", "0 if In < Edge, else 1.", "threshold"}},
        {"Smoothstep", {"Плавный порог", "Плавный переход от 0 к 1 между Min и Max.", "Smooth 0 to 1 transition between Min and Max.", "smooth threshold"}},
        {"Dot", {"Скалярное произведение", "dot(A, B).", "dot(A, B).", "dot product"}},
        {"Fresnel", {"Френель", "Яркость края: сильнее там, где поверхность видна под углом.",
                     "Rim brightness: stronger where the surface is seen at a grazing angle.", "rim edge край ободок"}}};
    return infos;
}
} // namespace

const MaterialGraphSchema& MaterialGraphSchema::Instance()
{
    static const MaterialGraphSchema schema;
    return schema;
}

const QString& MaterialGraphSchema::MasterType()
{
    return Master;
}

bool MaterialGraphSchema::IsTextureType(const QString& type)
{
    return type == QStringLiteral("Texture2D") || type == QStringLiteral("NormalMap");
}

bool MaterialGraphSchema::IsParameterType(const QString& type)
{
    return type == QStringLiteral("ColorParam") || type == QStringLiteral("ScalarParam") ||
           type == QStringLiteral("Vector2Param") || type == QStringLiteral("Vector3Param");
}

int MaterialGraphSchema::VectorSize(const QString& type)
{
    return type == QStringLiteral("Vector2Param") ? 2 : type == QStringLiteral("Vector3Param") ? 3 : 0;
}

QVector<GraphNodeGroup> MaterialGraphSchema::Groups() const
{
    QVector<GraphNodeGroup> groups;
    for (const auto& group : NodeGroups())
        groups.append({Tr(group.ru, group.en), QString::fromUtf8(group.ru) + QLatin1Char(' ') + QString::fromUtf8(group.en), group.types, group.color});
    return groups;
}

bool MaterialGraphSchema::IsKnownNode(const QString& type) const
{
    return HlslGraphCompiler::IsKnownNode(type);
}

QString MaterialGraphSchema::DisplayName(const QString& type) const
{
    const auto info = NodeInfos().find(type);
    return info == NodeInfos().end() ? type : Tr(info->ru, type.toUtf8().constData());
}

QString MaterialGraphSchema::Hint(const QString& type) const
{
    const auto info = NodeInfos().find(type);
    return info == NodeInfos().end() ? QString() : Tr(info->hintRu, info->hintEn);
}

QString MaterialGraphSchema::SearchKeywords(const QString& type) const
{
    const auto info = NodeInfos().find(type);
    if (info == NodeInfos().end()) return {};
    return QStringList{QString::fromUtf8(info->ru), QString::fromUtf8(info->keywords), QString::fromUtf8(info->hintRu),
                       QString::fromUtf8(info->hintEn)}.join(QLatin1Char(' '));
}

QString MaterialGraphSchema::SearchPlaceholder() const
{
    return Tr("Поиск: умножение, текстура, fresnel…", "Search: multiply, texture, fresnel…");
}

QColor MaterialGraphSchema::NodeColor(const GraphNodeData& node) const
{
    if (node.type == Master) return QColor(0xC2, 0x41, 0x0C);
    return GraphSchema::NodeColor(node);
}

// Цвет пина по типу: связь допустима любая, цвет подсказывает, что будет приведено.
QColor MaterialGraphSchema::PinColor(const GraphPin& pin) const
{
    if (pin.type == QStringLiteral("float")) return QColor(0x94, 0xA3, 0xB8);
    if (pin.type == QStringLiteral("float2")) return QColor(0x34, 0xD3, 0x99);
    if (pin.type == QStringLiteral("float3")) return QColor(0xFA, 0xCC, 0x15);
    if (pin.type == QStringLiteral("float4")) return QColor(0xF4, 0x72, 0xB6);
    return QColor(0xE2, 0xE8, 0xF0); // any: размер по входам
}

QVector<GraphPin> MaterialGraphSchema::Pins(const GraphNodeData& node) const
{
    return HlslGraphCompiler::NodePins(node.type);
}

void MaterialGraphSchema::EnsureRequiredNodes(GraphDocument& document) const
{
    // Граф без выхода (например, из старого формата) получает его: из меню выход не добавляется.
    const auto& nodes = document.Nodes();
    if (std::none_of(nodes.begin(), nodes.end(), [](const GraphNodeData& n) { return n.type == Master; }))
        document.AddNode(Master, QStringLiteral("Material Output"), QPointF(600, 180));
}

QString MaterialGraphSchema::NodeName(const GraphNodeData& node) const
{
    if (IsParameterType(node.type)) return node.properties.value(QStringLiteral("name")).toString();
    if (IsTextureType(node.type)) return node.properties.value(QStringLiteral("textureName")).toString();
    return {};
}

QString MaterialGraphSchema::NameProblem(const QString& name) const
{
    return HlslGraphCompiler::NameProblem(name);
}

void MaterialGraphSchema::SetNodeName(GraphNodeData& node, const QString& name) const
{
    if (IsTextureType(node.type)) node.properties[QStringLiteral("textureName")] = name;
    else if (IsParameterType(node.type)) node.properties[QStringLiteral("name")] = name;
}

QString MaterialGraphSchema::NodeTitle(const GraphNodeData& node) const
{
    return node.type == Master ? DisplayName(node.type) : GraphSchema::NodeTitle(node);
}

void MaterialGraphSchema::PrepareNewNode(const GraphDocument& document, const QString& type, QVariantMap& properties, QString& title) const
{
    title = type;
    auto named = [&](const char* key, const QString& base) {
        if (!properties.contains(QLatin1String(key))) properties[QLatin1String(key)] = UniqueName(document, base);
        title = properties.value(QLatin1String(key)).toString();
    };
    auto fallback = [&](const char* key, const QVariant& value) {
        if (!properties.contains(QLatin1String(key))) properties[QLatin1String(key)] = value;
    };
    if (type == QStringLiteral("ColorParam"))
    {
        named("name", QStringLiteral("Color"));
        fallback("color", QColor(Qt::white));
    }
    else if (type == QStringLiteral("ScalarParam"))
    {
        named("name", QStringLiteral("Scalar"));
        fallback("value", 0.5);
    }
    else if (const int size = VectorSize(type))
    {
        named("name", QStringLiteral("Vector"));
        fallback("value", QVariantList(size, 0.0));
    }
    else if (type == QStringLiteral("Constant"))
        fallback("value", 1.0);
    else if (type == QStringLiteral("Texture2D"))
        named("textureName", QStringLiteral("BaseMap"));
    else if (type == QStringLiteral("NormalMap"))
        named("textureName", QStringLiteral("NormalMap"));
    else if (type == QStringLiteral("UVCoord"))
    {
        fallback("tilingX", 1.0);
        fallback("tilingY", 1.0);
    }
    else if (type == QStringLiteral("Panner"))
    {
        fallback("speedX", 0.5);
        fallback("speedY", 0.0);
    }
}

void MaterialGraphSchema::PreparePastedNode(const GraphDocument& document, GraphNodeData& node) const
{
    const QString name = NodeName(node);
    if (name.isEmpty()) return;
    QList<const GraphNodeData*> holders;
    for (const auto& existing : document.Nodes())
        if (NodeName(existing) == name) holders << &existing;
    const bool share = IsTextureType(node.type) && !holders.isEmpty() &&
                       std::all_of(holders.begin(), holders.end(), [](const GraphNodeData* n) { return IsTextureType(n->type); });
    if (share)
    {
        const QString texture = holders.first()->properties.value(QStringLiteral("texture")).toString();
        if (texture.isEmpty()) node.properties.remove(QStringLiteral("texture"));
        else node.properties[QStringLiteral("texture")] = texture;
    }
    else if (!holders.isEmpty())
        GraphSchema::PreparePastedNode(document, node);
}

} // namespace EditorCore
