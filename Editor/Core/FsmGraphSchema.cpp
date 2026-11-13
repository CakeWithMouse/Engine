#include "FsmGraphSchema.h"
#include "EditorLocale.h"

#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace EditorCore
{
namespace
{
QString Text(const char* ru, const char* en) { return Tr(ru, en); }

QString ValueText(const QVariant& value)
{
    if (value.metaType().id() == QMetaType::Bool) return value.toBool() ? Text("да", "true") : Text("нет", "false");
    if (value.metaType().id() == QMetaType::QVariantList)
    {
        QStringList parts;
        for (const auto& part : value.toList()) parts << QString::number(part.toDouble(), 'g', 4);
        return QStringLiteral("(%1)").arg(parts.join(QStringLiteral(", ")));
    }
    if (value.canConvert<double>() && value.metaType().id() != QMetaType::QString) return QString::number(value.toDouble(), 'g', 5);
    return value.toString();
}
} // namespace

bool FsmGraphSchema::LoadCatalog(const QString& json, QString* error)
{
    const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();
    if (root.value(QStringLiteral("schema")).toString() != Id())
    {
        if (error) *error = Text("Игровой модуль вернул не каталог автоматов ИИ.", "The game module returned something that is not an AI machine catalog.");
        return false;
    }
    blocks_.clear();
    index_.clear();
    for (const auto& value : root.value(QStringLiteral("blocks")).toArray())
    {
        const QJsonObject item = value.toObject();
        Block block;
        block.type = item.value(QStringLiteral("type")).toString();
        block.kind = item.value(QStringLiteral("kind")).toString();
        block.nameRu = item.value(QStringLiteral("nameRu")).toString();
        block.nameEn = item.value(QStringLiteral("nameEn")).toString();
        block.hintRu = item.value(QStringLiteral("hintRu")).toString();
        block.hintEn = item.value(QStringLiteral("hintEn")).toString();
        block.engine = item.value(QStringLiteral("engine")).toBool();
        for (const auto& p : item.value(QStringLiteral("params")).toArray())
        {
            const QJsonObject paramJson = p.toObject();
            Param param;
            param.name = paramJson.value(QStringLiteral("name")).toString();
            param.type = paramJson.value(QStringLiteral("type")).toString();
            param.labelRu = paramJson.value(QStringLiteral("labelRu")).toString();
            param.labelEn = paramJson.value(QStringLiteral("labelEn")).toString();
            param.fallback = paramJson.value(QStringLiteral("default"));
            param.minimum = paramJson.value(QStringLiteral("min")).toDouble(-1e12);
            param.maximum = paramJson.value(QStringLiteral("max")).toDouble(1e12);
            param.required = paramJson.value(QStringLiteral("required")).toBool();
            block.params << param;
        }
        if (block.type.isEmpty() || index_.contains(block.type)) continue;
        index_.insert(block.type, blocks_.size());
        blocks_ << block;
    }
    if (blocks_.isEmpty() && error) *error = Text("В каталоге автоматов ИИ нет блоков.", "The AI machine catalog has no blocks.");
    return !blocks_.isEmpty();
}

const FsmGraphSchema::Block* FsmGraphSchema::FindBlock(const QString& type) const
{
    const auto it = index_.find(type);
    return it == index_.end() ? nullptr : &blocks_[*it];
}

QVector<const FsmGraphSchema::Block*> FsmGraphSchema::Blocks(const QString& kind) const
{
    QVector<const Block*> list;
    for (const auto& block : blocks_)
        if (block.kind == kind) list << &block;
    std::stable_sort(list.begin(), list.end(), [](const Block* a, const Block* b) { return a->engine && !b->engine; });
    return list;
}

void FsmGraphSchema::SetVariables(const QJsonArray& variables)
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

QString FsmGraphSchema::VariableType(const QString& name) const { return variables_.value(name); }

QStringList FsmGraphSchema::KeysFor(const QString& paramType) const
{
    QStringList keys;
    for (const auto& name : variableOrder_)
    {
        const QString type = variables_.value(name);
        bool fits = paramType == QStringLiteral("key");
        if (paramType == QStringLiteral("key:number")) fits = type == QStringLiteral("int") || type == QStringLiteral("float");
        else if (paramType == QStringLiteral("key:place")) fits = type == QStringLiteral("object") || type == QStringLiteral("vec3");
        else if (paramType.startsWith(QStringLiteral("key:"))) fits = type == paramType.mid(4);
        if (fits) keys << name;
    }
    return keys;
}

QStringList FsmGraphSchema::VariableTypes()
{
    return {QStringLiteral("bool"), QStringLiteral("int"), QStringLiteral("float"), QStringLiteral("vec3"), QStringLiteral("string"), QStringLiteral("object")};
}

QString FsmGraphSchema::TypeLabel(const QString& type)
{
    if (type == QStringLiteral("bool")) return Text("Логическое", "Bool");
    if (type == QStringLiteral("int")) return Text("Целое", "Int");
    if (type == QStringLiteral("float")) return Text("Число", "Float");
    if (type == QStringLiteral("vec3")) return Text("Вектор", "Vector");
    if (type == QStringLiteral("string")) return Text("Строка", "String");
    if (type == QStringLiteral("object")) return Text("Объект", "Object");
    return type;
}

QString FsmGraphSchema::BlockName(const QString& type) const
{
    if (type == QStringLiteral("Ai.Any")) return Text("Хотя бы одно из", "Any of");
    if (type == QStringLiteral("Ai.All")) return Text("Все из", "All of");
    const Block* block = FindBlock(type);
    if (!block) return type;
    return EditorLocale::Instance().Language() == EditorLanguage::Ru ? block->nameRu : block->nameEn;
}

QString FsmGraphSchema::BlockSummary(const QVariantMap& block) const
{
    const QString type = block.value(QStringLiteral("type")).toString();
    QString text = BlockName(type);
    if (type == QStringLiteral("Ai.Any") || type == QStringLiteral("Ai.All"))
    {
        QStringList items;
        for (const auto& item : block.value(QStringLiteral("items")).toList()) items << BlockSummary(item.toMap());
        text += QStringLiteral(" (") + items.join(QStringLiteral("; ")) + QLatin1Char(')');
    }
    else if (const Block* info = FindBlock(type))
    {
        QStringList values;
        for (const auto& param : info->params)
            if (block.contains(param.name)) values << ValueText(block.value(param.name));
        if (!values.isEmpty()) text += QStringLiteral(": ") + values.join(QStringLiteral(", "));
    }
    if (block.value(QStringLiteral("not")).toBool()) text = Text("НЕ ", "NOT ") + text;
    if (block.contains(QStringLiteral("interval")))
        text += Text(" · раз в %1 с", " · every %1 s").arg(block.value(QStringLiteral("interval")).toDouble());
    return text;
}

QString FsmGraphSchema::TriggerLabel(const QString& trigger)
{
    if (trigger == QStringLiteral("succeeded")) return Text("успех задачи", "task succeeded");
    if (trigger == QStringLiteral("failed")) return Text("провал задачи", "task failed");
    if (trigger == QStringLiteral("event")) return Text("по событию", "on event");
    if (trigger == QStringLiteral("tick")) return Text("каждое решение (по условиям)", "every think (by conditions)");
    return Text("состояние завершилось", "state completed");
}

QList<int> FsmGraphSchema::Children(const GraphDocument& document, int parent)
{
    QVector<const GraphNodeData*> list;
    for (const auto& node : document.Nodes())
        if (node.type == QLatin1String(StateType) && node.properties.value(QStringLiteral("parent")).toInt() == parent) list << &node;
    std::sort(list.begin(), list.end(), [](const GraphNodeData* a, const GraphNodeData* b) {
        if (a->pos.y() != b->pos.y()) return a->pos.y() < b->pos.y();
        if (a->pos.x() != b->pos.x()) return a->pos.x() < b->pos.x();
        return a->id < b->id;
    });
    QList<int> ids;
    for (const auto* node : list) ids << node->id;
    return ids;
}

bool FsmGraphSchema::IsAncestor(const GraphDocument& document, int ancestor, int node)
{
    QSet<int> seen;
    for (const GraphNodeData* current = document.FindNode(node); current && !seen.contains(current->id);)
    {
        seen.insert(current->id);
        const int parent = current->properties.value(QStringLiteral("parent")).toInt();
        if (!parent) return false;
        if (parent == ancestor) return true;
        current = document.FindNode(parent);
    }
    return false;
}

QVector<GraphNodeGroup> FsmGraphSchema::Groups() const
{
    return {{Text("Состояния", "States"), QStringLiteral("Состояния States"), {QLatin1String(StateType)}, QColor(0x25, 0x63, 0xEB)},
            {Text("Цели переходов", "Transition targets"), QStringLiteral("Цели переходов Transition targets"),
             {QLatin1String(FinishType), QLatin1String(NextType)}, QColor(0x7C, 0x3A, 0xED)}};
}

bool FsmGraphSchema::IsKnownNode(const QString& type) const
{
    return type == QLatin1String(StateType) || type == QLatin1String(FinishType) || type == QLatin1String(NextType);
}

QString FsmGraphSchema::DisplayName(const QString& type) const
{
    if (type == QLatin1String(StateType)) return Text("Состояние", "State");
    if (type == QLatin1String(FinishType)) return Text("Закончить", "Finish");
    if (type == QLatin1String(NextType)) return Text("Следующее", "Next");
    return type;
}

QString FsmGraphSchema::Hint(const QString& type) const
{
    if (type == QLatin1String(StateType))
        return Text("Что делает персонаж: задачи, условия входа, службы. Состояние завершается с первой завершившейся задачей.",
                    "What the character does: tasks, enter conditions, services. A state completes with its first finished task.");
    if (type == QLatin1String(FinishType))
        return Text("Переход сюда останавливает автомат (в связанном автомате — завершает состояние, которое его связало).",
                    "A transition here stops the machine (inside a linked machine it completes the state that links it).");
    if (type == QLatin1String(NextType)) return Text("Переход к следующему по порядку состоянию того же родителя.", "A transition to the next state of the same parent.");
    return {};
}

QString FsmGraphSchema::SearchKeywords(const QString& type) const
{
    if (type == QLatin1String(StateType)) return QStringLiteral("состояние state задача task");
    if (type == QLatin1String(FinishType)) return QStringLiteral("закончить конец стоп finish stop end");
    if (type == QLatin1String(NextType)) return QStringLiteral("следующее далее next");
    return {};
}

QString FsmGraphSchema::SearchPlaceholder() const { return Text("Поиск: состояние, закончить…", "Search: state, finish…"); }

QVector<GraphPin> FsmGraphSchema::Pins(const GraphNodeData& node) const
{
    QVector<GraphPin> pins;
    GraphPin in;
    in.name = QStringLiteral("in");
    in.isInput = true;
    in.type = QStringLiteral("state");
    pins << in;
    if (node.type == QLatin1String(StateType))
    {
        GraphPin out;
        out.name = QStringLiteral("out");
        out.type = QStringLiteral("state");
        pins << out;
    }
    return pins;
}

QVector<GraphLinkField> FsmGraphSchema::LinkFields(const GraphDocument&, const GraphConnectionData&) const
{
    // Окно свойств перехода своё (FsmGraphWidget::EditLink); поля нужны, чтобы меню связи предлагало его.
    return {{QStringLiteral("trigger"), Text("Когда", "When"), QStringLiteral("choice"), QStringLiteral("completed"), Triggers()}};
}

QString FsmGraphSchema::LinkLabel(const GraphConnectionData& link) const
{
    const QString trigger = link.properties.value(QStringLiteral("trigger"), QStringLiteral("completed")).toString();
    QStringList parts;
    if (trigger == QStringLiteral("event"))
    {
        const QString event = link.properties.value(QStringLiteral("event")).toString();
        parts << (event.isEmpty() ? Text("событие?", "event?") : event);
    }
    else if (trigger == QStringLiteral("succeeded")) parts << Text("успех", "success");
    else if (trigger == QStringLiteral("failed")) parts << Text("провал", "failure");
    else if (trigger == QStringLiteral("tick")) parts << Text("если", "if");
    const QVariantList conditions = link.properties.value(QStringLiteral("conditions")).toList();
    if (conditions.size() == 1) parts << BlockSummary(conditions.first().toMap());
    else if (!conditions.isEmpty()) parts << Text("%1 усл.", "%1 cond.").arg(conditions.size());
    const double delay = link.properties.value(QStringLiteral("delay")).toDouble();
    if (delay > 0) parts << Text("через %1 с", "after %1 s").arg(delay);
    if (link.properties.contains(QStringLiteral("priority"))) parts << QStringLiteral("#%1").arg(link.properties.value(QStringLiteral("priority")).toInt());
    return parts.join(QStringLiteral(" · "));
}

QString FsmGraphSchema::NodeName(const GraphNodeData& node) const
{
    return node.type == QLatin1String(StateType) ? node.properties.value(QStringLiteral("name")).toString() : QString();
}

QString FsmGraphSchema::NameProblem(const QString& name) const
{
    if (name.trimmed().isEmpty()) return Text("Назовите состояние.", "Name the state.");
    if (name.contains(QLatin1Char('/'))) return Text("В имени состояния не может быть «/».", "A state name cannot contain '/'.");
    return {};
}

void FsmGraphSchema::SetNodeName(GraphNodeData& node, const QString& name) const
{
    if (node.type != QLatin1String(StateType)) return;
    node.properties[QStringLiteral("name")] = name;
    node.title = name;
}

QString FsmGraphSchema::NodeTitle(const GraphNodeData& node) const
{
    const QString name = NodeName(node);
    return name.isEmpty() ? DisplayName(node.type) : name;
}

void FsmGraphSchema::PrepareNewNode(const GraphDocument& document, const QString& type, QVariantMap& properties, QString& title) const
{
    if (type != QLatin1String(StateType)) return;
    const QString name = UniqueName(document, properties.value(QStringLiteral("name"), QStringLiteral("State")).toString());
    properties[QStringLiteral("name")] = name;
    title = name;
}

void FsmGraphSchema::PreparePastedNode(const GraphDocument& document, GraphNodeData& node) const
{
    GraphSchema::PreparePastedNode(document, node);
    // Родитель из другого графа или не вставленный вместе — состояние встаёт на верхний уровень.
    const int parent = node.properties.value(QStringLiteral("parent")).toInt();
    const GraphNodeData* host = parent ? document.FindNode(parent) : nullptr;
    if (parent && (!host || host->type != QLatin1String(StateType))) node.properties.remove(QStringLiteral("parent"));
}

QHash<int, QString> FsmGraphSchema::Warnings(const GraphDocument& document) const
{
    QHash<int, QString> warnings;
    // Несколько предупреждений одной ноды — строками.
    auto warn = [&](int node, const QString& text) {
        QString& line = warnings[node];
        line = line.isEmpty() ? text : line + QLatin1Char('\n') + text;
    };
    QSet<int> targets;
    for (const auto& link : document.Connections()) targets.insert(link.toNode);
    const QList<int> roots = Children(document, 0);
    // Переход в «Следующее» ведёт в следующего соседа своего состояния.
    for (const auto& link : document.Connections())
    {
        const GraphNodeData* to = document.FindNode(link.toNode);
        const GraphNodeData* from = document.FindNode(link.fromNode);
        if (!to || !from || to->type != QLatin1String(NextType)) continue;
        const int parent = from->properties.value(QStringLiteral("parent")).toInt();
        const QList<int> siblings = parent ? Children(document, parent) : roots;
        const int at = siblings.indexOf(from->id);
        if (at >= 0 && at + 1 < siblings.size()) targets.insert(siblings[at + 1]);
    }
    for (const auto& node : document.Nodes())
    {
        if (node.type == QLatin1String(StateType))
        {
            const int parent = node.properties.value(QStringLiteral("parent")).toInt();
            const QList<int> siblings = parent ? Children(document, parent) : roots;
            const bool first = !siblings.isEmpty() && siblings.first() == node.id;
            // Не первое среди соседей и без входящих переходов: сюда можно попасть только условием входа после первого.
            if (!first && !targets.contains(node.id) && node.properties.value(QStringLiteral("enterIf")).toList().isEmpty())
                warn(node.id, Text("В состояние никто не переходит, и оно не первое среди соседей: оно не запустится.",
                                              "Nothing transitions here and it is not the first of its siblings: it never runs."));
            const bool tasks = !node.properties.value(QStringLiteral("tasks")).toList().isEmpty();
            const bool linked = !node.properties.value(QStringLiteral("linked")).toString().isEmpty();
            if (!tasks && !linked && Children(document, node.id).isEmpty())
                warn(node.id, Text("В состоянии нет задач: оно ничего не делает.", "The state has no tasks: it does nothing."));
        }
        else if (!targets.contains(node.id))
            warn(node.id, Text("К этой ноде не ведёт ни один переход.", "No transition leads to this node."));
    }
    for (const auto& link : document.Connections())
    {
        if (link.properties.value(QStringLiteral("trigger")).toString() == QStringLiteral("tick") &&
            link.properties.value(QStringLiteral("conditions")).toList().isEmpty() && link.properties.value(QStringLiteral("delay")).toDouble() <= 0)
            warn(link.fromNode, Text("Переход «каждое решение» без условий срабатывает сразу: состояние не успеет поработать.",
                                                "An \"every think\" transition without conditions fires at once: the state has no time to work."));
    }
    return warnings;
}

// Перевод сообщений загрузчика. Сообщение составное: «где» (задача 2 "Ai.Wait", переход в "Rest") и «что».
// Замены идут по порядку; незнакомые части остаются по-английски.
QString AiMessageRu(const QString& message)
{
    struct Rule
    {
        QRegularExpression pattern;
        QString replacement;
    };
    static const QVector<Rule> rules = [] {
        QVector<Rule> list;
        auto add = [&](const char* pattern, const char* replacement) {
            list.push_back({QRegularExpression(QString::fromUtf8(pattern)), QString::fromUtf8(replacement)});
        };
        // Где.
        add("^settings: service (\\d+)", "настройки: служба \\1");
        add("^settings: ", "настройки: ");
        add("^task (\\d+)", "задача \\1");
        add("^enter condition (\\d+)", "условие входа \\1");
        add("^service (\\d+)", "служба \\1");
        add("^transition to Finish", "переход к «Закончить»");
        add("^transition to Next", "переход к «Следующее»");
        add("^transition to \"([^\"]*)\"", "переход в «\\1»");
        add(": condition (\\d+)", ": условие \\1");
        add(" item (\\d+)", " пункт \\1");
        // Блоки и параметры.
        add("unknown block \"([^\"]*)\"", "неизвестный блок «\\1»: возможно, игровой модуль проекта устарел");
        add("\"([^\"]*)\" is a (task|condition|service), not a (task|condition|service)", "«\\1» — блок вида «\\2», а здесь нужен «\\3»");
        add("a block must be an object with a \"type\"", "блок должен быть JSON-объектом с полем «type»");
        add("unknown parameter \"([^\"]*)\"", "неизвестный параметр «\\1»");
        add("\"([^\"]*)\" is not set: name a memory key", "«\\1» не задан: выберите ключ памяти");
        add("\"([^\"]*)\" is not set", "«\\1» не задано");
        add("\"([^\"]*)\": no variable \"([^\"]*)\"; declare it in \"variables\"", "«\\1»: нет переменной «\\2» — объявите её в памяти автомата");
        add("\"([^\"]*)\": variable \"([^\"]*)\" is (\\w+), the block needs ([\\w ]+)", "«\\1»: переменная «\\2» типа \\3, а блоку нужен тип \\4");
        add("\"([^\"]*)\" must be ([-\\d.]+) or more", "«\\1» должно быть не меньше \\2");
        add("\"([^\"]*)\" must be ([-\\d.]+) or less", "«\\1» должно быть не больше \\2");
        add("\"([^\"]*)\" must be from ([-\\d.]+) to ([-\\d.]+)", "«\\1» должно быть от \\2 до \\3");
        add("\"([^\"]*)\" must be true or false", "«\\1» должно быть «да» или «нет»");
        add("\"([^\"]*)\" must be a whole number", "«\\1» должно быть целым числом");
        add("\"([^\"]*)\" must be a number, 0 or more", "«\\1» должно быть числом не меньше 0");
        add("\"([^\"]*)\" must be a number", "«\\1» должно быть числом");
        add("\"([^\"]*)\" must be \\[x, y, z\\]", "«\\1» должно быть вектором [x, y, z]");
        add("\"([^\"]*)\" must be text", "«\\1» должно быть текстом");
        add("\"([^\"]*)\" must be a (\\w+) like the key", "«\\1» должно быть типа \\2, как ключ");
        add("the block made no (task|condition|service)", "блок ничего не создал");
        add("\"items\" must be a non-empty array of conditions", "в группе должно быть хотя бы одно условие");
        add("groups are nested deeper than (\\d+)", "группы условий вложены глубже \\1");
        add("\"minimum\" is greater than \"maximum\"", "«От» больше «До»");
        add("an object key cannot be set to a value; use Ai.FindObject", "ключ-объект нельзя задать значением: используйте «Найти объект»");
        add("\"op\" must be one of <, <=, ==, !=, >=, >", "сравнение должно быть одним из <, <=, ==, !=, >=, >");
        // Состояния.
        add("^name the state$", "назовите состояние");
        add("^a state name cannot contain '/'$", "в имени состояния не может быть «/»");
        add("^two states are named \"([^\"]*)\"$", "два состояния с именем «\\1»");
        add("^\"parent\" must be the id of a state node$", "родитель должен быть состоянием");
        add("^parent (\\d+) is not a node$", "родителя \\1 нет в графе");
        add("^the parent must be a state$", "родитель должен быть состоянием");
        add("^states are nested inside each other in a loop$", "состояния вложены друг в друга по кругу");
        add("^a linked state cannot have its own child states$", "у состояния со связанным автоматом не может быть своих детей");
        add("^\"(tasks|services)\" must be an array$", "«\\1» должно быть массивом");
        add("^\"linked\" must be a machine name or a .fsm path$", "связанный автомат — имя из C++ или путь к .fsm");
        add("^no state machine named \"([^\"]*)\" is registered in C\\+\\+$", "в C++ нет автомата с именем «\\1»");
        add("^linked \"([^\"]*)\": linked files cannot be read here$", "связанный «\\1»: здесь нельзя читать связанные файлы");
        add("^linked machines form a loop: (.*)$", "связанные автоматы образуют круг: \\1");
        add("^linked machines are nested deeper than (\\d+)$", "связанные автоматы вложены глубже \\1");
        add("^linked \"([^\"]*)\": cannot read the file$", "связанный «\\1»: файл не читается");
        add("^linked \"([^\"]*)\" has problems: ", "в связанном «\\1» ошибки: ");
        // Переходы.
        add("^a transition can start only at a state$", "переход может начинаться только у состояния");
        add("^a state has only the output \"out\"$", "у состояния один выход «out»");
        add("^the only input is \"in\"$", "у ноды один вход «in»");
        add("unknown trigger \"([^\"]*)\"; use completed, succeeded, failed, event or tick", "неизвестный триггер «\\1»");
        add(": name the event$", ": задайте имя события");
        add(": \"([^\"]*)\" is the last state of its parent$", ": «\\1» — последнее состояние у своего родителя, следующего нет");
        add("^a link to a missing node (\\d+)$", "связь ведёт к несуществующей ноде \\1");
        add("^a link must be an object$", "файл повреждён: связь должна быть JSON-объектом");
        // Файл.
        add("^cannot read the file$", "не удаётся прочитать файл: возможно, его переименовали или удалили");
        add("^invalid JSON: (.*)$", "файл повреждён, ошибка JSON: \\1");
        add("^the root must be a JSON object$", "файл повреждён: в корне должен быть JSON-объект");
        add("^not a graph file: \"format\" must be \"(.*)\"$", "это не файл графа: поле «format» должно быть «\\1»");
        add("^unsupported graph format version (\\d+); update the engine$", "формат графа версии \\1 не поддерживается, обновите движок");
        add("^not a state machine: \"schema\" must be \"(.*)\"$", "это не автомат ИИ: поле «schema» должно быть «\\1»");
        add("^unsupported state machine version (\\d+); update the engine$", "автомат версии \\1 не поддерживается, обновите движок");
        add("^\"(variables|nodes|connections)\" must be an array$", "файл повреждён: «\\1» должно быть массивом");
        add("^\"settings\" must be an object$", "файл повреждён: «settings» должно быть объектом");
        add("^a variable without a name$", "переменная без имени");
        add("^variable \"([^\"]*)\" is declared twice$", "переменная «\\1» объявлена дважды");
        add("^variable \"([^\"]*)\" has unknown type \"([^\"]*)\".*$", "у переменной «\\1» неизвестный тип «\\2»");
        add("^variable \"([^\"]*)\": an object has no starting value$", "переменная «\\1»: у объекта нет начального значения");
        add("^variable \"([^\"]*)\": the value is not a (\\w+)$", "переменная «\\1»: значение не подходит к типу \\2");
        add("\"(targetKey|lastSeenKey)\" names no variable \"([^\"]*)\"", "«\\1»: нет переменной «\\2»");
        add("\"(targetKey|lastSeenKey)\" needs a variable of type (\\w+), \"([^\"]*)\" is (\\w+)", "«\\1»: нужна переменная типа \\2, а «\\3» — \\4");
        add("unknown setting \"([^\"]*)\"", "неизвестная настройка «\\1»");
        add("^a node without a positive integer id$", "нода без целого положительного id");
        add("^node id (\\d+) is used twice$", "id ноды \\1 встречается дважды");
        add("^unknown node type \"([^\"]*)\"$", "неизвестный тип ноды «\\1»");
        add("^the machine has no states$", "в автомате нет состояний");
        add("^invalid state machine file: (.*)$", "файл автомата повреждён: \\1");
        return list;
    }();
    QString text = message;
    for (const auto& rule : rules) text.replace(rule.pattern, rule.replacement);
    return text;
}

QString LocalizeAiMessage(const QString& message)
{
    return EditorLocale::Instance().Language() == EditorLanguage::Ru ? AiMessageRu(message) : message;
}

} // namespace EditorCore
