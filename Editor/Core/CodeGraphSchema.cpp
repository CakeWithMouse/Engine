#include "CodeGraphSchema.h"
#include "EditorLocale.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace EditorCore
{

namespace
{
struct GroupStyle
{
    const char* ru;
    QColor color;
};

// Группы каталога движка (английские ключи) по-русски и их цвет.
const QHash<QString, GroupStyle>& GroupStyles()
{
    static const QHash<QString, GroupStyle> styles{
        {QStringLiteral("Events"), {"События", QColor(0xB9, 0x1C, 0x1C)}},
        {QStringLiteral("Flow"), {"Поток", QColor(0x47, 0x55, 0x69)}},
        {QStringLiteral("Actions"), {"Действия", QColor(0x1D, 0x4E, 0xD8)}},
        {QStringLiteral("Variables"), {"Переменные", QColor(0x7E, 0x22, 0xCE)}},
        {QStringLiteral("Objects"), {"Объекты", QColor(0x0E, 0x74, 0x90)}},
        {QStringLiteral("Constants"), {"Константы", QColor(0x4D, 0x7C, 0x0F)}},
        {QStringLiteral("Vectors"), {"Векторы", QColor(0xA1, 0x62, 0x07)}},
        {QStringLiteral("Math"), {"Математика", QColor(0x15, 0x80, 0x3D)}},
        {QStringLiteral("Logic"), {"Логика", QColor(0xBE, 0x18, 0x5D)}},
        {QStringLiteral("Input"), {"Ввод", QColor(0xC2, 0x41, 0x0C)}}};
    return styles;
}
} // namespace

bool CodeGraphSchema::LoadCatalog(const QString& json, QString* error)
{
    auto fail = [&](const QString& message) {
        if (error) *error = message;
        return false;
    };
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    const QJsonObject root = document.object();
    if (!document.isObject() || root.value(QStringLiteral("schema")).toString() != Id())
        return fail(Tr("Игровой модуль вернул не каталог нод кода.", "The game module did not return a code node catalog."));

    QVector<Node> nodes;
    QHash<QString, int> index;
    QStringList groups;
    for (const auto& value : root.value(QStringLiteral("nodes")).toArray())
    {
        const QJsonObject object = value.toObject();
        Node node;
        node.type = object.value(QStringLiteral("type")).toString();
        node.group = object.value(QStringLiteral("group")).toString();
        node.nameRu = object.value(QStringLiteral("name")).toObject().value(QStringLiteral("ru")).toString();
        node.nameEn = object.value(QStringLiteral("name")).toObject().value(QStringLiteral("en")).toString();
        node.hintRu = object.value(QStringLiteral("hint")).toObject().value(QStringLiteral("ru")).toString();
        node.hintEn = object.value(QStringLiteral("hint")).toObject().value(QStringLiteral("en")).toString();
        node.event = object.value(QStringLiteral("event")).toBool();
        for (const auto& pinValue : object.value(QStringLiteral("pins")).toArray())
        {
            const QJsonObject pin = pinValue.toObject();
            GraphPin graphPin;
            graphPin.name = pin.value(QStringLiteral("name")).toString();
            graphPin.isInput = pin.value(QStringLiteral("input")).toBool();
            graphPin.type = pin.value(QStringLiteral("type")).toString();
            node.pins << graphPin;
        }
        for (const auto& propertyValue : object.value(QStringLiteral("properties")).toArray())
        {
            const QJsonObject property = propertyValue.toObject();
            node.properties << Property{property.value(QStringLiteral("name")).toString(), property.value(QStringLiteral("kind")).toString()};
        }
        if (node.type.isEmpty() || index.contains(node.type)) continue;
        if (!groups.contains(node.group)) groups << node.group;
        index.insert(node.type, nodes.size());
        nodes << node;
    }
    if (nodes.isEmpty()) return fail(Tr("Каталог нод пуст.", "The node catalog is empty."));

    nodes_ = nodes;
    index_ = index;
    groupOrder_ = groups;
    keys_.clear();
    for (const auto& key : root.value(QStringLiteral("keys")).toArray()) keys_ << key.toString();
    maxOutputs_ = root.value(QStringLiteral("maxSequenceOutputs")).toInt(8);
    return true;
}

const CodeGraphSchema::Node* CodeGraphSchema::FindNode(const QString& type) const
{
    const auto found = index_.find(type);
    return found == index_.end() ? nullptr : &nodes_[*found];
}

void CodeGraphSchema::SetVariables(const QJsonArray& variables)
{
    variables_.clear();
    variableOrder_.clear();
    for (const auto& value : variables)
    {
        const QJsonObject variable = value.toObject();
        const QString name = variable.value(QStringLiteral("name")).toString();
        if (name.isEmpty() || variables_.contains(name)) continue;
        variables_.insert(name, variable.value(QStringLiteral("type")).toString());
        variableOrder_ << name;
    }
}

QString CodeGraphSchema::VariableType(const QString& name) const
{
    return variables_.value(name);
}

QVector<GraphNodeGroup> CodeGraphSchema::Groups() const
{
    QVector<GraphNodeGroup> groups;
    for (const QString& key : groupOrder_)
    {
        const GroupStyle style = GroupStyles().value(key, GroupStyle{nullptr, QColor(0x47, 0x55, 0x69)});
        GraphNodeGroup group;
        group.name = style.ru ? Tr(style.ru, key.toUtf8().constData()) : key;
        group.searchText = (style.ru ? QString::fromUtf8(style.ru) : QString()) + QLatin1Char(' ') + key;
        group.color = style.color;
        for (const auto& node : nodes_)
            if (node.group == key) group.types << node.type;
        groups << group;
    }
    return groups;
}

QString CodeGraphSchema::DisplayName(const QString& type) const
{
    const Node* node = FindNode(type);
    if (!node) return type;
    return Tr(node->nameRu.toUtf8().constData(), node->nameEn.toUtf8().constData());
}

QString CodeGraphSchema::Hint(const QString& type) const
{
    const Node* node = FindNode(type);
    if (!node) return {};
    return Tr(node->hintRu.toUtf8().constData(), node->hintEn.toUtf8().constData());
}

QString CodeGraphSchema::SearchKeywords(const QString& type) const
{
    const Node* node = FindNode(type);
    if (!node) return {};
    return QStringList{node->nameRu, node->nameEn, node->hintRu, node->hintEn}.join(QLatin1Char(' '));
}

QString CodeGraphSchema::SearchPlaceholder() const
{
    return Tr("Поиск: каждый кадр, сдвинуть, клавиша…", "Search: every frame, translate, key…");
}

QColor CodeGraphSchema::TypeColor(const QString& type)
{
    if (type == QStringLiteral("exec")) return QColor(0xF8, 0xFA, 0xFC);
    if (type == QStringLiteral("bool")) return QColor(0xEF, 0x44, 0x44);
    if (type == QStringLiteral("float")) return QColor(0x34, 0xD3, 0x99);
    if (type == QStringLiteral("vec3")) return QColor(0xFA, 0xCC, 0x15);
    if (type == QStringLiteral("string")) return QColor(0xF4, 0x72, 0xB6);
    if (type == QStringLiteral("object")) return QColor(0x38, 0xBD, 0xF8);
    return QColor(0x94, 0xA3, 0xB8); // any, неизвестный
}

QString CodeGraphSchema::TypeLabel(const QString& type)
{
    if (type == QStringLiteral("bool")) return Tr("логическое", "boolean");
    if (type == QStringLiteral("float")) return Tr("число", "number");
    if (type == QStringLiteral("vec3")) return Tr("вектор", "vector");
    if (type == QStringLiteral("string")) return Tr("строка", "text");
    if (type == QStringLiteral("object")) return Tr("объект", "object");
    if (type == QStringLiteral("exec")) return Tr("выполнение", "execution");
    return Tr("любое", "any");
}

QColor CodeGraphSchema::PinColor(const GraphPin& pin) const
{
    return TypeColor(pin.type);
}

QVector<GraphPin> CodeGraphSchema::Pins(const GraphNodeData& data) const
{
    const Node* node = FindNode(data.type);
    if (!node) return {};
    QVector<GraphPin> pins = node->pins;
    if (data.type == QStringLiteral("Flow.Sequence"))
    {
        const int outputs = std::clamp(data.properties.value(QStringLiteral("outputs"), 2).toInt(), 1, maxOutputs_);
        for (int i = 0; i < outputs; ++i) pins << GraphPin{QStringLiteral("then%1").arg(i), false, QStringLiteral("exec"), {}};
    }
    const QString variableType = VariableType(data.properties.value(QStringLiteral("variable")).toString());
    for (auto& pin : pins)
        if (pin.type == QStringLiteral("variable")) pin.type = variableType.isEmpty() ? QStringLiteral("any") : variableType;
    return pins;
}

QString CodeGraphSchema::LinkProblem(const GraphPin& output, const GraphPin& input) const
{
    const bool execOut = output.type == QStringLiteral("exec"), execIn = input.type == QStringLiteral("exec");
    if (execOut != execIn)
        return execIn ? Tr("Этот вход принимает поток выполнения, а не значение.", "This input takes the execution flow, not a value.")
                      : Tr("Этот вход принимает значение, а не поток выполнения.", "This input takes a value, not the execution flow.");
    if (execOut || input.type == QStringLiteral("any")) return {};
    if (output.type != input.type)
        return Tr("Вход принимает «%1», а выход даёт «%2».", "The input takes \"%1\", the output gives \"%2\".")
            .arg(TypeLabel(input.type), TypeLabel(output.type));
    return {};
}

void CodeGraphSchema::PrepareNewNode(const GraphDocument&, const QString& type, QVariantMap& properties, QString& title) const
{
    title.clear(); // заголовок — название типа на языке редактора
    const Node* node = FindNode(type);
    if (!node) return;
    auto fallback = [&](const QString& key, const QVariant& value) {
        if (!properties.contains(key)) properties[key] = value;
    };
    for (const auto& property : node->properties)
    {
        if (property.kind == QStringLiteral("key")) fallback(property.name, QStringLiteral("Space"));
        else if (property.kind == QStringLiteral("variable") && !variableOrder_.isEmpty()) fallback(property.name, variableOrder_.first());
        else if (property.kind == QStringLiteral("outputs")) fallback(property.name, 2);
        else if (property.name == QStringLiteral("seconds")) fallback(property.name, 1.0);
        else if (property.name == QStringLiteral("event")) fallback(property.name, QStringLiteral("Event"));
        else if (property.kind == QStringLiteral("float")) fallback(property.name, 0.0);
        else if (property.kind == QStringLiteral("bool")) fallback(property.name, false);
        else if (property.kind == QStringLiteral("vec3")) fallback(property.name, QVariantList{0.0, 0.0, 0.0});
        else if (property.kind == QStringLiteral("string")) fallback(property.name, QString());
    }
    // Входы, у которых 0 по умолчанию бесполезен: «Случайное число» от 0 до 0, задержка 0 с.
    static const QHash<QString, QVariantMap> inputDefaults{
        {QStringLiteral("Math.Random"), {{QStringLiteral("Max"), 1.0}}},
        {QStringLiteral("Flow.Delay"), {{QStringLiteral("Seconds"), 1.0}}},
        {QStringLiteral("Flow.For"), {{QStringLiteral("Last"), 9.0}}},
    };
    if (inputDefaults.contains(type))
    {
        QVariantMap inputs = properties.value(QStringLiteral("inputs")).toMap();
        const QVariantMap defaults = inputDefaults.value(type);
        for (auto it = defaults.begin(); it != defaults.end(); ++it)
            if (!inputs.contains(it.key())) inputs.insert(it.key(), it.value());
        properties[QStringLiteral("inputs")] = inputs;
    }
}


QHash<int, QString> CodeGraphSchema::Warnings(const GraphDocument& document) const
{
    QHash<int, QString> warnings;
    auto warn = [&](int node, const QString& text) {
        if (!warnings.contains(node)) warnings.insert(node, text);
    };
    auto linkedInput = [&](int node, const QString& pin) {
        for (const auto& c : document.Connections())
            if (c.toNode == node && c.toPin == pin) return true;
        return false;
    };
    auto linkedOutput = [&](int node, const QString& pin) {
        for (const auto& c : document.Connections())
            if (c.fromNode == node && c.fromPin == pin) return true;
        return false;
    };
    // Значение входа без связи: из properties.inputs, иначе 0.
    auto inputNumber = [&](const GraphNodeData& node, const QString& pin) {
        return node.properties.value(QStringLiteral("inputs")).toMap().value(pin, 0.0).toDouble();
    };
    for (const auto& node : document.Nodes())
    {
        const Node* info = FindNode(node.type);
        if (!info) continue; // неизвестную ноду отклонит движок
        const QVector<GraphPin> pins = Pins(node);
        bool execIn = false, execOut = false, dataOut = false, anyExecOutLinked = false, anyDataOutLinked = false;
        for (const auto& pin : pins)
        {
            const bool exec = pin.type == QStringLiteral("exec");
            if (pin.isInput && exec) execIn = true;
            if (!pin.isInput && exec)
            {
                execOut = true;
                anyExecOutLinked = anyExecOutLinked || linkedOutput(node.id, pin.name);
            }
            if (!pin.isInput && !exec)
            {
                dataOut = true;
                anyDataOutLinked = anyDataOutLinked || linkedOutput(node.id, pin.name);
            }
        }
        if (node.type == QStringLiteral("Math.Random") && !linkedInput(node.id, QStringLiteral("Min")) && !linkedInput(node.id, QStringLiteral("Max")) &&
            inputNumber(node, QStringLiteral("Min")) == inputNumber(node, QStringLiteral("Max")))
            warn(node.id, Tr("Min и Max равны (%1): число всегда одно и то же. Задайте их двойным щелчком по ноде.",
                             "Min and Max are equal (%1): the number never changes. Set them by double-clicking the node.")
                              .arg(inputNumber(node, QStringLiteral("Min"))));
        if (node.type == QStringLiteral("Math.Divide") && !linkedInput(node.id, QStringLiteral("B")) && inputNumber(node, QStringLiteral("B")) == 0.0)
            warn(node.id, Tr("B не подключён и равен 0: в Play будет ошибка «деление на ноль».",
                             "B is not connected and is 0: Play will stop with \"division by zero\"."));
        if (info->event && execOut && !anyExecOutLinked)
            warn(node.id, Tr("Событие ни к чему не ведёт: подключите к его выходу действие.",
                             "The event leads nowhere: connect an action to its output."));
        else if (execIn && !linkedInput(node.id, QStringLiteral("exec")))
            warn(node.id, Tr("Нода не выполнится: к входу exec ничего не подключено.",
                             "The node never runs: nothing is connected to its exec input."));
        else if (!info->event && !execIn && !execOut && dataOut && !anyDataOutLinked)
            warn(node.id, Tr("Результат никуда не идёт: нода не влияет на скрипт.",
                             "The result goes nowhere: the node does not affect the script."));
    }
    return warnings;
}

} // namespace EditorCore
