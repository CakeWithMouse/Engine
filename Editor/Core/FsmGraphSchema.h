#pragma once

#include "Graph/GraphSchema.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

namespace EditorCore
{

// Схема автомата ИИ (.fsm, "ai.fsm"). Блоки (задачи, условия, службы) и их параметры приходят из каталога движка
// (Engine.Scene.AiMachine describe, Ai::MachineCatalogJson), поэтому редактор и загрузчик Play не расходятся.
// Формат — Engine/docs/AI-FSM.md.
//
// Ноды: Fsm.State (вход in, выход out), Fsm.Finish и Fsm.Next (только вход in). Связь — переход: из out состояния
// в in любой ноды; у пинов сколько угодно связей, циклы разрешены. Свойства перехода (trigger, event, conditions,
// delay, priority) хранятся в свойствах связи. Вложенность — свойство parent состояния; порядок детей — сверху вниз.
class FsmGraphSchema final : public GraphSchema
{
public:
    static constexpr const char* StateType = "Fsm.State";
    static constexpr const char* FinishType = "Fsm.Finish";
    static constexpr const char* NextType = "Fsm.Next";

    struct Param
    {
        QString name, type;       // type: bool, int, float, vec3, string, event, key, key:<тип>, key:number, key:place, value
        QString labelRu, labelEn;
        QJsonValue fallback;      // значение по умолчанию (нет у ключей, событий и value)
        double minimum = -1e12, maximum = 1e12;
        bool required = false;
    };
    struct Block
    {
        QString type, kind;       // kind: task, condition, service
        QString nameRu, nameEn, hintRu, hintEn;
        QVector<Param> params;
        bool engine = false;      // встроенный блок движка ("Ai.*"); иначе блок игры
    };

    // false: не каталог схемы ai.fsm; схема остаётся пустой.
    bool LoadCatalog(const QString& json, QString* error = nullptr);
    bool IsLoaded() const { return !blocks_.isEmpty(); }
    const Block* FindBlock(const QString& type) const;
    QVector<const Block*> Blocks(const QString& kind) const; // по kind, встроенные первыми

    // Переменные документа (ключи памяти): [{"name","type","value"}].
    void SetVariables(const QJsonArray& variables);
    QString VariableType(const QString& name) const;
    QStringList VariableNames() const { return variableOrder_; }
    // Переменные, подходящие параметру-ключу этого типа (key, key:float, key:place…).
    QStringList KeysFor(const QString& paramType) const;
    static QStringList VariableTypes();
    static QString TypeLabel(const QString& type);

    // Имя блока и строка о нём для ноды и списков: «Идти к: Player, 1.5». Неизвестный блок — его тип.
    QString BlockName(const QString& type) const;
    QString BlockSummary(const QVariantMap& block) const;
    // Триггер перехода на языке редактора и подпись на связи («по событию Go · 2 усл. · через 0.5 с»).
    static QStringList Triggers() { return {QStringLiteral("completed"), QStringLiteral("succeeded"), QStringLiteral("failed"),
                                             QStringLiteral("event"), QStringLiteral("tick")}; }
    static QString TriggerLabel(const QString& trigger);

    // Состояния документа: id -> родитель (0 — верхний уровень); дети по порядку (сверху вниз, затем слева направо).
    static QList<int> Children(const GraphDocument& document, int parent);
    static bool IsAncestor(const GraphDocument& document, int ancestor, int node); // ancestor — предок node

    QString Id() const override { return QStringLiteral("ai.fsm"); }
    int Version() const override { return 1; }
    QString ObjectPrefix() const override { return QStringLiteral("FsmGraph"); }
    QString ClipboardMime() const override { return QStringLiteral("application/x-itmoengine-fsm-nodes"); }
    QString ClipboardMarker() const override { return QStringLiteral("fsmGraphNodes"); }

    QVector<GraphNodeGroup> Groups() const override;
    bool IsKnownNode(const QString& type) const override;
    QString DisplayName(const QString& type) const override;
    QString Hint(const QString& type) const override;
    QString SearchKeywords(const QString& type) const override;
    QString SearchPlaceholder() const override;
    QColor PinColor(const GraphPin&) const override { return QColor(0xCB, 0xD5, 0xE1); }
    QColor LinkColor(const GraphPin&) const override { return QColor(0xCB, 0xD5, 0xE1); }

    QVector<GraphPin> Pins(const GraphNodeData& node) const override;
    bool InputTakesOneLink(const GraphPin&) const override { return false; }
    bool OutputTakesOneLink(const GraphPin&) const override { return false; }
    bool AllowsCycle(const GraphPin&, const GraphPin&) const override { return true; }

    QVector<GraphLinkField> LinkFields(const GraphDocument& document, const GraphConnectionData& link) const override;
    QString LinkLabel(const GraphConnectionData& link) const override;

    QString NodeName(const GraphNodeData& node) const override;
    QString NameProblem(const QString& name) const override;
    void SetNodeName(GraphNodeData& node, const QString& name) const override;
    QString NodeTitle(const GraphNodeData& node) const override;
    void PrepareNewNode(const GraphDocument& document, const QString& type, QVariantMap& properties, QString& title) const override;
    void PreparePastedNode(const GraphDocument& document, GraphNodeData& node) const override;

    // Подозрительное, что загрузчик пропустит: нода -> текст на языке редактора.
    QHash<int, QString> Warnings(const GraphDocument& document) const;

private:
    QVector<Block> blocks_;
    QHash<QString, int> index_;
    QHash<QString, QString> variables_;
    QStringList variableOrder_;
};

// Сообщения загрузчика .fsm (Engine/Source/AI/MachineFile.cpp) на языке редактора; незнакомое остаётся как есть.
QString AiMessageRu(const QString& message);
QString LocalizeAiMessage(const QString& message);

} // namespace EditorCore
