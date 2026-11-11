#pragma once

#include "Graph/GraphSchema.h"

#include <QHash>
#include <QJsonArray>
#include <QStringList>

namespace EditorCore
{

// Схема графа кода (визуальный скрипт, .vscript). Ноды, пины и свойства берутся из каталога движка
// (Engine.Scene.Script describe, Script::CatalogJson): редактор и интерпретатор не расходятся.
//
// Правила: у exec-выхода одна связь, у exec-входа много, циклы по exec разрешены; у входа данных одна
// связь, тип выхода совпадает с типом входа (вход any принимает любой), циклы по данным запрещены.
// Пины Variable.Get/Set берут тип выбранной переменной, поэтому схема знает переменные документа.
class CodeGraphSchema final : public GraphSchema
{
public:
    struct Property
    {
        QString name, kind; // kind: key, variable, outputs, bool, float, vec3, string
    };
    struct Node
    {
        QString type, group, nameRu, nameEn, hintRu, hintEn;
        QVector<GraphPin> pins;
        QVector<Property> properties;
        bool event = false;
    };

    // false: не каталог схемы code; схема остаётся пустой.
    bool LoadCatalog(const QString& json, QString* error = nullptr);
    bool IsLoaded() const { return !nodes_.isEmpty(); }
    const Node* FindNode(const QString& type) const;
    const QStringList& KeyNames() const { return keys_; }
    int MaxSequenceOutputs() const { return maxOutputs_; }

    // Переменные документа: [{"name","type","value"}]. Пины переменных берут отсюда тип.
    void SetVariables(const QJsonArray& variables);
    QString VariableType(const QString& name) const; // пусто: нет такой переменной
    QStringList VariableNames() const { return variableOrder_; }
    static QStringList VariableTypes() { return {QStringLiteral("float"), QStringLiteral("bool"), QStringLiteral("vec3"), QStringLiteral("string")}; }

    QString Id() const override { return QStringLiteral("code"); }
    int Version() const override { return 1; }
    QString ObjectPrefix() const override { return QStringLiteral("ScriptGraph"); }
    QString ClipboardMime() const override { return QStringLiteral("application/x-itmoengine-script-nodes"); }
    QString ClipboardMarker() const override { return QStringLiteral("scriptGraphNodes"); }

    QVector<GraphNodeGroup> Groups() const override;
    bool IsKnownNode(const QString& type) const override { return FindNode(type) != nullptr; }
    QString DisplayName(const QString& type) const override;
    QString Hint(const QString& type) const override;
    QString SearchKeywords(const QString& type) const override;
    QString SearchPlaceholder() const override;
    QColor PinColor(const GraphPin& pin) const override;
    QColor LinkColor(const GraphPin& output) const override { return PinColor(output); }

    QVector<GraphPin> Pins(const GraphNodeData& node) const override;

    bool InputTakesOneLink(const GraphPin& input) const override { return input.type != QStringLiteral("exec"); }
    bool OutputTakesOneLink(const GraphPin& output) const override { return output.type == QStringLiteral("exec"); }
    bool AllowsCycle(const GraphPin& output, const GraphPin&) const override { return output.type == QStringLiteral("exec"); }
    QString LinkProblem(const GraphPin& output, const GraphPin& input) const override;

    void PrepareNewNode(const GraphDocument& document, const QString& type, QVariantMap& properties, QString& title) const override;

    static QColor TypeColor(const QString& type);
    static QString TypeLabel(const QString& type); // имя типа на языке редактора

private:
    QVector<Node> nodes_;
    QHash<QString, int> index_;
    QStringList groupOrder_;
    QStringList keys_;
    int maxOutputs_ = 8;
    QHash<QString, QString> variables_; // имя -> тип
    QStringList variableOrder_;
};

} // namespace EditorCore
