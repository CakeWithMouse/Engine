#include "ScriptGraphWidget.h"
#include "EditorLocale.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QSpinBox>
#include <algorithm>
#include <functional>

using EditorCore::CodeGraphSchema;
using EditorCore::GraphDocument;
using EditorCore::GraphNodeData;
using EditorCore::GraphPin;

namespace
{
constexpr qreal SummaryLine = 15.0;

QString ValueText(const QVariant& value, const QString& type)
{
    if (type == QStringLiteral("bool")) return value.toBool() ? Tr("да", "true") : Tr("нет", "false");
    if (type == QStringLiteral("vec3"))
    {
        const QVariantList list = value.toList();
        QStringList parts;
        for (int i = 0; i < 3; ++i) parts << QString::number(i < list.size() ? list[i].toDouble() : 0.0, 'g', 4);
        return QStringLiteral("(%1)").arg(parts.join(QStringLiteral(", ")));
    }
    if (type == QStringLiteral("float")) return QString::number(value.toDouble(), 'g', 5);
    if (value.metaType().id() == QMetaType::QVariantList) return ValueText(value, QStringLiteral("vec3"));
    if (value.metaType().id() == QMetaType::Bool) return ValueText(value, QStringLiteral("bool"));
    if (value.metaType().id() == QMetaType::Double || value.metaType().id() == QMetaType::LongLong || value.metaType().id() == QMetaType::Int)
        return ValueText(value, QStringLiteral("float"));
    return QStringLiteral("«%1»").arg(value.toString());
}

// Нулевое значение типа: такие значения входов не записываются (движок подставит то же самое).
bool IsDefault(const QVariant& value, const QString& type)
{
    if (type == QStringLiteral("bool")) return !value.toBool();
    if (type == QStringLiteral("float")) return value.toDouble() == 0.0;
    if (type == QStringLiteral("vec3"))
    {
        const QVariantList list = value.toList();
        return std::all_of(list.begin(), list.end(), [](const QVariant& v) { return v.toDouble() == 0.0; });
    }
    return value.toString().isEmpty();
}

QDoubleSpinBox* NumberBox(QWidget* parent, const QString& name, double value)
{
    auto* box = new QDoubleSpinBox(parent);
    box->setObjectName(name);
    box->setRange(-1e9, 1e9);
    box->setDecimals(3);
    box->setValue(value);
    return box;
}

// Редактор значения по типу; read возвращает значение в виде свойства JSON.
QWidget* ValueEditor(QWidget* parent, const QString& name, const QString& type, const QVariant& value, std::function<QVariant()>& read)
{
    if (type == QStringLiteral("bool"))
    {
        auto* box = new QCheckBox(parent);
        box->setObjectName(name);
        box->setChecked(value.toBool());
        read = [box] { return QVariant(box->isChecked()); };
        return box;
    }
    if (type == QStringLiteral("float"))
    {
        auto* box = NumberBox(parent, name, value.toDouble());
        read = [box] { return QVariant(box->value()); };
        return box;
    }
    if (type == QStringLiteral("vec3"))
    {
        auto* row = new QWidget(parent);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        const QVariantList list = value.toList();
        QVector<QDoubleSpinBox*> boxes;
        for (int i = 0; i < 3; ++i)
        {
            boxes << NumberBox(row, QStringLiteral("%1_%2").arg(name).arg(i), i < list.size() ? list[i].toDouble() : 0.0);
            layout->addWidget(boxes.last());
        }
        read = [boxes] { return QVariant(QVariantList{boxes[0]->value(), boxes[1]->value(), boxes[2]->value()}); };
        return row;
    }
    auto* line = new QLineEdit(parent);
    line->setObjectName(name);
    line->setText(value.toString());
    read = [line] { return QVariant(line->text()); };
    return line;
}
} // namespace

ScriptGraphWidget::ScriptGraphWidget(CodeGraphSchema& schema, QWidget* parent)
    : GraphEditorWidget(schema, parent), schema_(schema)
{
    setObjectName(QStringLiteral("ScriptGraph"));
}

void ScriptGraphWidget::LoadDocument(const GraphDocument& document)
{
    schema_.SetVariables(document.Variables());
    SetDocument(document);
    if (variablesChanged) variablesChanged();
}

void ScriptGraphWidget::SetVariables(const QJsonArray& variables, const QHash<QString, QString>& renames)
{
    Checkpoint();
    MutableDocument().SetVariables(variables);
    for (const auto& node : Document().Nodes())
    {
        const QString variable = node.properties.value(QStringLiteral("variable")).toString();
        if (renames.contains(variable)) MutableDocument().FindNode(node.id)->properties[QStringLiteral("variable")] = renames.value(variable);
    }
    schema_.SetVariables(variables);
    RebuildScene();
    if (variablesChanged) variablesChanged();
    NotifyStructure();
}

void ScriptGraphWidget::OnRestored(const GraphDocument&)
{
    // Пины переменных зависят от их типов: схема узнаёт восстановленные переменные, сцена строится заново.
    schema_.SetVariables(Document().Variables());
    RebuildScene();
    if (variablesChanged) variablesChanged();
}

bool ScriptGraphWidget::SetNodeProperties(int nodeId, const QVariantMap& properties)
{
    const GraphNodeData* node = Document().FindNode(nodeId);
    if (!node) return false;
    if (node->properties == properties) return true;
    Checkpoint();
    MutableDocument().FindNode(nodeId)->properties = properties;
    RebuildScene();
    NotifyStructure();
    return true;
}

QVector<GraphPin> ScriptGraphWidget::EditableInputs(const GraphNodeData& node) const
{
    QVector<GraphPin> inputs;
    for (const auto& pin : schema_.Pins(node))
    {
        if (!pin.isInput || pin.type == QStringLiteral("exec") || pin.type == QStringLiteral("object")) continue;
        const auto& links = Document().Connections();
        if (std::any_of(links.begin(), links.end(), [&](const auto& c) { return c.toNode == node.id && c.toPin == pin.name; })) continue;
        inputs << pin;
    }
    return inputs;
}

QStringList ScriptGraphWidget::NodeSummary(const GraphNodeData& node) const
{
    QStringList lines;
    const CodeGraphSchema::Node* info = schema_.FindNode(node.type);
    if (!info) return {Tr("неизвестная нода", "unknown node")};
    for (const auto& property : info->properties)
    {
        const QVariant value = node.properties.value(property.name);
        if (property.kind == QStringLiteral("key")) lines << Tr("Клавиша: %1", "Key: %1").arg(value.toString());
        else if (property.kind == QStringLiteral("variable"))
            lines << (value.toString().isEmpty() ? Tr("Переменная не выбрана", "No variable") : value.toString());
        else if (property.kind != QStringLiteral("outputs")) lines << ValueText(value, property.kind);
    }
    const QVariantMap defaults = node.properties.value(QStringLiteral("inputs")).toMap();
    for (const auto& pin : EditableInputs(node))
        if (defaults.contains(pin.name)) lines << QStringLiteral("%1 = %2").arg(pin.name, ValueText(defaults.value(pin.name), pin.type));
    return lines;
}

qreal ScriptGraphWidget::NodeContentHeight(const GraphNodeData& node) const
{
    const int lines = static_cast<int>(NodeSummary(node).size());
    return lines ? lines * SummaryLine + 4 : 0;
}

void ScriptGraphWidget::PaintNodeContent(QPainter* painter, const GraphNodeData& node, const QRectF& content, const QVariant&) const
{
    const QStringList lines = NodeSummary(node);
    for (int i = 0; i < lines.size(); ++i)
    {
        const QRectF row(content.left(), content.top() + 2 + i * SummaryLine, content.width(), SummaryLine);
        painter->drawText(row, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(lines[i], Qt::ElideRight, static_cast<int>(row.width())));
    }
}

QString ScriptGraphWidget::EditActionText(const GraphNodeData& node) const
{
    const CodeGraphSchema::Node* info = schema_.FindNode(node.type);
    const bool editable = (info && !info->properties.isEmpty()) || !EditableInputs(node).isEmpty();
    return editable ? GraphEditorWidget::EditActionText(node) : QString();
}

void ScriptGraphWidget::EditNode(int nodeId)
{
    const GraphNodeData* node = Document().FindNode(nodeId);
    const CodeGraphSchema::Node* info = node ? schema_.FindNode(node->type) : nullptr;
    if (!node || !info) return;
    const QVector<GraphPin> inputs = EditableInputs(*node);
    if (info->properties.isEmpty() && inputs.isEmpty()) return;

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("ScriptNodeDialog"));
    dialog.setWindowTitle(schema_.DisplayName(node->type));
    auto* form = new QFormLayout(&dialog);
    QVector<std::pair<QString, std::function<QVariant()>>> properties, values;

    for (const auto& property : info->properties)
    {
        const QVariant current = node->properties.value(property.name);
        const QString name = QStringLiteral("ScriptProp_") + property.name;
        std::function<QVariant()> read;
        QWidget* editor = nullptr;
        QString label;
        if (property.kind == QStringLiteral("key") || property.kind == QStringLiteral("variable"))
        {
            auto* combo = new QComboBox(&dialog);
            combo->setObjectName(name);
            combo->addItems(property.kind == QStringLiteral("key") ? schema_.KeyNames() : schema_.VariableNames());
            combo->setCurrentText(current.toString());
            read = [combo] { return QVariant(combo->currentText()); };
            editor = combo;
            label = property.kind == QStringLiteral("key") ? Tr("Клавиша", "Key") : Tr("Переменная", "Variable");
        }
        else if (property.kind == QStringLiteral("outputs"))
        {
            auto* spin = new QSpinBox(&dialog);
            spin->setObjectName(name);
            spin->setRange(1, schema_.MaxSequenceOutputs());
            spin->setValue(current.toInt() > 0 ? current.toInt() : 2);
            read = [spin] { return QVariant(spin->value()); };
            editor = spin;
            label = Tr("Выходов", "Outputs");
        }
        else
        {
            editor = ValueEditor(&dialog, name, property.kind, current, read);
            label = Tr("Значение", "Value");
        }
        form->addRow(label, editor);
        properties.append({property.name, read});
    }
    const QVariantMap defaults = node->properties.value(QStringLiteral("inputs")).toMap();
    if (!inputs.isEmpty())
        form->addRow(new QLabel(Tr("Значения входов без связи:", "Values of unconnected inputs:"), &dialog));
    for (const auto& pin : inputs)
    {
        const QString type = pin.type == QStringLiteral("any") ? QStringLiteral("string") : pin.type;
        std::function<QVariant()> read;
        form->addRow(pin.name, ValueEditor(&dialog, QStringLiteral("ScriptInput_") + pin.name, type, defaults.value(pin.name), read));
        values.append({pin.name + QLatin1Char('\n') + type, read});
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || !(node = Document().FindNode(nodeId))) return;

    QVariantMap result = node->properties;
    for (const auto& [name, read] : properties) result[name] = read();
    QVariantMap inputValues = defaults;
    for (const auto& [key, read] : values)
    {
        const QString pin = key.section(QLatin1Char('\n'), 0, 0), type = key.section(QLatin1Char('\n'), 1, 1);
        const QVariant value = read();
        if (IsDefault(value, type)) inputValues.remove(pin);
        else inputValues[pin] = value;
    }
    if (inputValues.isEmpty()) result.remove(QStringLiteral("inputs"));
    else result[QStringLiteral("inputs")] = inputValues;
    SetNodeProperties(nodeId, result);
}
