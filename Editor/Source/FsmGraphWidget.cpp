#include "FsmGraphWidget.h"
#include "EditorLocale.h"
#include "FsmBlockListEditor.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

using EditorCore::FsmGraphSchema;
using EditorCore::GraphConnectionData;
using EditorCore::GraphDocument;
using EditorCore::GraphNodeData;

namespace
{
constexpr qreal SummaryLine = 15.0;

bool IsState(const GraphNodeData& node) { return node.type == QLatin1String(FsmGraphSchema::StateType); }

// Ссылки блоков на ключи памяти после переименования переменной (группы условий — вглубь).
void RenameKeys(const FsmGraphSchema& schema, QVariantList& blocks, const QHash<QString, QString>& renames)
{
    for (auto& value : blocks)
    {
        QVariantMap block = value.toMap();
        const QString type = block.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("Ai.Any") || type == QStringLiteral("Ai.All"))
        {
            QVariantList items = block.value(QStringLiteral("items")).toList();
            RenameKeys(schema, items, renames);
            block[QStringLiteral("items")] = items;
        }
        else if (const auto* info = schema.FindBlock(type))
            for (const auto& param : info->params)
                if (param.type.startsWith(QStringLiteral("key")) && renames.contains(block.value(param.name).toString()))
                    block[param.name] = renames.value(block.value(param.name).toString());
        value = block;
    }
}
} // namespace

FsmGraphWidget::FsmGraphWidget(FsmGraphSchema& schema, QWidget* parent)
    : GraphEditorWidget(schema, parent), schema_(schema)
{
    setObjectName(QStringLiteral("FsmGraph"));
    structureChanged = [this] {
        // Удалённый родитель: дети встают на верхний уровень в той же правке (снимок отмены сделан до неё).
        if (FixOrphans()) RebuildScene();
        if (changed) changed();
    };
}

void FsmGraphWidget::LoadDocument(const GraphDocument& document)
{
    schema_.SetVariables(document.Variables());
    SetDocument(document);
    if (panelsChanged) panelsChanged();
}

bool FsmGraphWidget::FixOrphans()
{
    bool fixed = false;
    for (const auto& node : Document().Nodes())
    {
        const int parent = node.properties.value(QStringLiteral("parent")).toInt();
        if (!parent) continue;
        const GraphNodeData* host = Document().FindNode(parent);
        if (host && IsState(*host)) continue;
        MutableDocument().FindNode(node.id)->properties.remove(QStringLiteral("parent"));
        fixed = true;
    }
    return fixed;
}

void FsmGraphWidget::SetVariables(const QJsonArray& variables, const QHash<QString, QString>& renames)
{
    Checkpoint();
    MutableDocument().SetVariables(variables);
    if (!renames.isEmpty())
    {
        for (const auto& node : Document().Nodes())
        {
            GraphNodeData* mutableNode = MutableDocument().FindNode(node.id);
            for (const char* list : {"tasks", "enterIf", "services"})
            {
                QVariantList blocks = mutableNode->properties.value(QLatin1String(list)).toList();
                if (blocks.isEmpty()) continue;
                RenameKeys(schema_, blocks, renames);
                mutableNode->properties[QLatin1String(list)] = blocks;
            }
        }
        for (const auto& link : Document().Connections())
        {
            QVariantList conditions = link.properties.value(QStringLiteral("conditions")).toList();
            if (conditions.isEmpty()) continue;
            RenameKeys(schema_, conditions, renames);
            QVariantMap properties = link.properties;
            properties[QStringLiteral("conditions")] = conditions;
            MutableDocument().SetConnectionProperties(link.fromNode, link.fromPin, link.toNode, link.toPin, properties);
        }
        QJsonObject settings = Settings();
        for (const char* key : {"targetKey", "lastSeenKey"})
            if (renames.contains(settings.value(QLatin1String(key)).toString()))
                settings[QLatin1String(key)] = renames.value(settings.value(QLatin1String(key)).toString());
        MutableDocument().SetExtra(QStringLiteral("settings"), settings);
    }
    schema_.SetVariables(variables);
    RebuildScene();
    if (panelsChanged) panelsChanged();
    NotifyStructure();
}

QJsonObject FsmGraphWidget::Settings() const { return Document().Extra(QStringLiteral("settings")).toObject(); }

void FsmGraphWidget::SetSettings(const QJsonObject& settings)
{
    if (settings == Settings()) return;
    Checkpoint();
    MutableDocument().SetExtra(QStringLiteral("settings"), settings);
    if (panelsChanged) panelsChanged();
    NotifyStructure();
}

void FsmGraphWidget::OnRestored(const GraphDocument&)
{
    schema_.SetVariables(Document().Variables());
    RebuildScene();
    if (panelsChanged) panelsChanged();
}

bool FsmGraphWidget::SetNodeProperties(int nodeId, const QVariantMap& properties)
{
    const GraphNodeData* node = Document().FindNode(nodeId);
    if (!node) return false;
    if (node->properties == properties) return true;
    Checkpoint();
    GraphNodeData* changedNode = MutableDocument().FindNode(nodeId);
    changedNode->properties = properties;
    if (IsState(*changedNode)) changedNode->title = properties.value(QStringLiteral("name")).toString();
    RebuildScene();
    NotifyStructure();
    return true;
}

QStringList FsmGraphWidget::NodeSummary(const GraphNodeData& node) const
{
    if (!IsState(node)) return {};
    QStringList lines;
    for (const auto& task : node.properties.value(QStringLiteral("tasks")).toList())
        lines << QStringLiteral("▶ ") + schema_.BlockSummary(task.toMap());
    for (const auto& condition : node.properties.value(QStringLiteral("enterIf")).toList())
        lines << Tr("если ", "if ") + schema_.BlockSummary(condition.toMap());
    for (const auto& service : node.properties.value(QStringLiteral("services")).toList())
        lines << QStringLiteral("⟳ ") + schema_.BlockSummary(service.toMap());
    const QString linked = node.properties.value(QStringLiteral("linked")).toString();
    if (!linked.isEmpty()) lines << QStringLiteral("⇢ ") + linked;
    if (lines.isEmpty())
        lines << (FsmGraphSchema::Children(Document(), node.id).isEmpty() ? Tr("нет задач", "no tasks") : Tr("состояния внутри рамки", "states inside the frame"));
    return lines;
}

qreal FsmGraphWidget::NodeWidth(const GraphNodeData& node) const { return IsState(node) ? 220 : 130; }

qreal FsmGraphWidget::NodeContentHeight(const GraphNodeData& node) const
{
    const int lines = static_cast<int>(NodeSummary(node).size());
    return lines ? lines * SummaryLine + 4 : 0;
}

void FsmGraphWidget::PaintNodeContent(QPainter* painter, const GraphNodeData& node, const QRectF& content, const QVariant&) const
{
    const QStringList lines = NodeSummary(node);
    for (int i = 0; i < lines.size(); ++i)
    {
        const QRectF row(content.left(), content.top() + 2 + i * SummaryLine, content.width(), SummaryLine);
        painter->drawText(row, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(lines[i], Qt::ElideRight, static_cast<int>(row.width())));
    }
}

QVector<GraphNodeFrame> FsmGraphWidget::NodeFrames() const
{
    QVector<GraphNodeFrame> frames;
    const GraphDocument& document = Document();
    // Глубина вложенных рамок: у листьев 0, у родителя — на 1 больше самого глубокого ребёнка-родителя.
    std::function<int(int, QList<int>&, int)> collect = [&](int id, QList<int>& nodes, int depth) -> int {
        nodes << id;
        int level = -1;
        if (depth > 32) return 0; // петля родителей: загрузчик скажет
        for (const int child : FsmGraphSchema::Children(document, id))
            level = std::max(level, collect(child, nodes, depth + 1));
        return level + 1;
    };
    for (const auto& node : document.Nodes())
    {
        if (!IsState(node) || FsmGraphSchema::Children(document, node.id).isEmpty()) continue;
        GraphNodeFrame frame;
        frame.level = collect(node.id, frame.nodes, 0) - 1;
        frame.color = QColor(0x25, 0x63, 0xEB);
        frames << frame;
    }
    return frames;
}

QString FsmGraphWidget::EditActionText(const GraphNodeData& node) const
{
    return IsState(node) ? Tr("Состояние…", "State…") : QString();
}

void FsmGraphWidget::EditNode(int nodeId)
{
    const GraphNodeData* node = Document().FindNode(nodeId);
    if (!node || !IsState(*node)) return;

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("FsmStateDialog"));
    dialog.setWindowTitle(Tr("Состояние «%1»", "State \"%1\"").arg(schema_.NodeName(*node)));
    dialog.resize(520, 560);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* name = new QLineEdit(schema_.NodeName(*node), &dialog);
    name->setObjectName(QStringLiteral("FsmStateName"));
    form->addRow(Tr("Имя", "Name"), name);
    auto* parent = new QComboBox(&dialog);
    parent->setObjectName(QStringLiteral("FsmStateParent"));
    parent->addItem(Tr("— верхний уровень —", "— top level —"), 0);
    for (const auto& other : Document().Nodes())
        if (IsState(other) && other.id != nodeId && !FsmGraphSchema::IsAncestor(Document(), nodeId, other.id))
            parent->addItem(schema_.NodeName(other), other.id);
    parent->setCurrentIndex(std::max(0, parent->findData(node->properties.value(QStringLiteral("parent")).toInt())));
    parent->setToolTip(Tr("Родитель входит в своего первого допустимого ребёнка (сверху вниз), его переходы действуют для всех детей.",
                          "A parent enters its first allowed child (top to bottom); its transitions apply to all children."));
    form->addRow(Tr("Внутри состояния", "Inside state"), parent);
    auto* linked = new QLineEdit(node->properties.value(QStringLiteral("linked")).toString(), &dialog);
    linked->setObjectName(QStringLiteral("FsmStateLinked"));
    linked->setPlaceholderText(Tr("имя автомата из C++ или /Game/AI/….fsm", "a C++ machine name or /Game/AI/….fsm"));
    linked->setToolTip(Tr("Состояния связанного автомата становятся детьми этого; «Закончить» внутри него завершает это состояние.",
                          "The linked machine's states become children of this one; Finish inside it completes this state."));
    form->addRow(Tr("Связанный автомат", "Linked machine"), linked);
    layout->addLayout(form);

    auto* tabs = new QTabWidget(&dialog);
    auto* tasks = new FsmBlockListEditor(schema_, QStringLiteral("task"), QStringLiteral("FsmTasks"), tabs);
    tasks->SetBlocks(node->properties.value(QStringLiteral("tasks")).toList());
    auto* enter = new FsmBlockListEditor(schema_, QStringLiteral("condition"), QStringLiteral("FsmEnterIf"), tabs);
    enter->SetBlocks(node->properties.value(QStringLiteral("enterIf")).toList());
    auto* services = new FsmBlockListEditor(schema_, QStringLiteral("service"), QStringLiteral("FsmServices"), tabs);
    services->SetBlocks(node->properties.value(QStringLiteral("services")).toList());
    tabs->addTab(tasks, Tr("Задачи", "Tasks"));
    tabs->addTab(enter, Tr("Условия входа", "Enter conditions"));
    tabs->addTab(services, Tr("Службы", "Services"));
    layout->addWidget(tabs, 1);
    auto* note = new QLabel(Tr("Состояние завершается с первой завершившейся задачей (успех — если без провала).",
                               "A state completes with its first finished task (success unless one failed)."), &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const QString wanted = name->text().trimmed();
        QString problem = schema_.NameProblem(wanted);
        for (const auto& other : Document().Nodes())
            if (problem.isEmpty() && other.id != nodeId && schema_.NodeName(other) == wanted)
                problem = Tr("Состояние с именем «%1» уже есть.", "There is already a state named \"%1\".").arg(wanted);
        if (!problem.isEmpty())
        {
            QMessageBox::warning(&dialog, dialog.windowTitle(), problem);
            return;
        }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted || !(node = Document().FindNode(nodeId))) return;

    QVariantMap properties = node->properties;
    properties[QStringLiteral("name")] = name->text().trimmed();
    auto put = [&](const char* key, const QVariant& value, bool empty) {
        if (empty) properties.remove(QLatin1String(key));
        else properties[QLatin1String(key)] = value;
    };
    put("parent", parent->currentData().toInt(), parent->currentData().toInt() == 0);
    put("linked", linked->text().trimmed(), linked->text().trimmed().isEmpty());
    put("tasks", tasks->Blocks(), tasks->Blocks().isEmpty());
    put("enterIf", enter->Blocks(), enter->Blocks().isEmpty());
    put("services", services->Blocks(), services->Blocks().isEmpty());
    SetNodeProperties(nodeId, properties);
}

void FsmGraphWidget::EditLink(const GraphConnectionData& link)
{
    QVariantMap properties = link.properties;
    for (const auto& c : Document().Connections())
        if (c.fromNode == link.fromNode && c.fromPin == link.fromPin && c.toNode == link.toNode && c.toPin == link.toPin) properties = c.properties;
    const GraphNodeData* from = Document().FindNode(link.fromNode);
    const GraphNodeData* to = Document().FindNode(link.toNode);
    if (!from || !to) return;

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("FsmLinkDialog"));
    dialog.setWindowTitle(Tr("Переход: %1 → %2", "Transition: %1 → %2").arg(schema_.NodeTitle(*from), schema_.NodeTitle(*to)));
    dialog.resize(480, 460);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* trigger = new QComboBox(&dialog);
    trigger->setObjectName(QStringLiteral("FsmLinkTrigger"));
    for (const QString& code : FsmGraphSchema::Triggers()) trigger->addItem(FsmGraphSchema::TriggerLabel(code), code);
    trigger->setCurrentIndex(std::max(0, trigger->findData(properties.value(QStringLiteral("trigger"), QStringLiteral("completed")).toString())));
    form->addRow(Tr("Когда", "When"), trigger);
    auto* event = new QLineEdit(properties.value(QStringLiteral("event")).toString(), &dialog);
    event->setObjectName(QStringLiteral("FsmLinkEvent"));
    event->setPlaceholderText(QStringLiteral("Go, Ai.Spotted, Ai.Lost, Ai.Heard, Ai.Damaged"));
    form->addRow(Tr("Событие", "Event"), event);
    auto* delay = new QDoubleSpinBox(&dialog);
    delay->setObjectName(QStringLiteral("FsmLinkDelay"));
    delay->setRange(0, 1e6);
    delay->setDecimals(2);
    delay->setValue(properties.value(QStringLiteral("delay")).toDouble());
    delay->setToolTip(Tr("Переход случится через столько секунд, если состояние ещё активно.", "The transition happens this many seconds later if the state is still active."));
    form->addRow(Tr("Задержка, с", "Delay, s"), delay);
    auto* priority = new QSpinBox(&dialog);
    priority->setObjectName(QStringLiteral("FsmLinkPriority"));
    priority->setRange(-1000, 1000);
    priority->setValue(properties.value(QStringLiteral("priority")).toInt());
    priority->setToolTip(Tr("Меньше — проверяется раньше других переходов этого состояния.", "Lower is checked before the other transitions of this state."));
    form->addRow(Tr("Приоритет", "Priority"), priority);
    layout->addLayout(form);
    auto* conditionsLabel = new QLabel(Tr("Дополнительные условия (все должны выполняться):", "Extra conditions (all must hold):"), &dialog);
    layout->addWidget(conditionsLabel);
    auto* conditions = new FsmBlockListEditor(schema_, QStringLiteral("condition"), QStringLiteral("FsmLinkConditions"), &dialog);
    conditions->SetBlocks(properties.value(QStringLiteral("conditions")).toList());
    layout->addWidget(conditions, 1);
    auto updateEvent = [trigger, event] { event->setEnabled(trigger->currentData().toString() == QStringLiteral("event")); };
    connect(trigger, &QComboBox::currentIndexChanged, &dialog, updateEvent);
    updateEvent();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    const QString code = trigger->currentData().toString();
    properties[QStringLiteral("trigger")] = code;
    if (code == QStringLiteral("event") && !event->text().trimmed().isEmpty()) properties[QStringLiteral("event")] = event->text().trimmed();
    else properties.remove(QStringLiteral("event"));
    if (delay->value() > 0) properties[QStringLiteral("delay")] = delay->value();
    else properties.remove(QStringLiteral("delay"));
    if (priority->value() != 0) properties[QStringLiteral("priority")] = priority->value();
    else properties.remove(QStringLiteral("priority"));
    if (!conditions->Blocks().isEmpty()) properties[QStringLiteral("conditions")] = conditions->Blocks();
    else properties.remove(QStringLiteral("conditions"));
    SetLinkProperties(link, properties);
}
