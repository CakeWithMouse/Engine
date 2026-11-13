#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace EditorCore
{

// Пин ноды. Тип — строка схемы (float3, exec, bool…): ядро графа его не толкует.
struct GraphPin
{
    QString name;
    bool isInput = false;
    QString type = QStringLiteral("float");
    QPointF localPos; // заполняет виджет при раскладке ноды
};

struct GraphNodeData
{
    int id = 0;
    QString type;
    QString title;
    QPointF pos;
    QVariantMap properties;
    QJsonObject extra; // неизвестные поля ноды из файла: записываются обратно как есть
};

struct GraphConnectionData
{
    int fromNode = 0;
    QString fromPin;
    int toNode = 0;
    QString toPin;
    QVariantMap properties; // условия переходов и т.п.; пустые не пишутся в файл
};

// Комментарий на холсте: рамка с подписью вокруг группы нод. На результат графа не влияет.
struct GraphCommentData
{
    int id = 0;
    QRectF rect;
    QString text;
    QColor color = QColor(0x47, 0x55, 0x69);
};

// Документ графа: ноды, связи, переменные и заголовок файла. Правила связей и смысл нод — в GraphSchema.
//
// Два формата файла:
//  * Legacy — граф материала (*.mat.graph.json, готовые графы): {"version":1,"nodes","connections"},
//    цвет QColor пишется массивом [r,g,b,a], массив из 4 чисел читается как QColor.
//  * V1 — {"format":"ITMOEngine.Graph","version":1,"schema","schemaVersion",…}: значения JSON как есть.
class GraphDocument
{
public:
    static constexpr const char* FormatName = "ITMOEngine.Graph";
    static constexpr int FormatVersion = 1;

    enum class Format { Legacy, V1 };

    GraphDocument() = default;
    virtual ~GraphDocument() = default;
    GraphDocument(const GraphDocument&) = default;
    GraphDocument& operator=(const GraphDocument&) = default;

    int AddNode(const QString& type, const QString& title, const QPointF& pos, const QVariantMap& props = {});
    bool RemoveNode(int nodeId); // вместе со связями ноды

    // Только точный дубль отклоняется: сколько связей допускает пин, решает схема.
    bool AddConnection(int fromNode, const QString& fromPin, int toNode, const QString& toPin, const QVariantMap& props = {});
    bool RemoveConnection(int fromNode, const QString& fromPin, int toNode, const QString& toPin);
    // Свойства связи целиком (пустые значения не пишутся в файл); false — связи нет.
    bool SetConnectionProperties(int fromNode, const QString& fromPin, int toNode, const QString& toPin, const QVariantMap& props);

    void Clear(); // ноды, связи и комментарии; заголовок и переменные остаются

    // Комментарии («comments» в файле; пишутся, только когда есть).
    int AddComment(const QRectF& rect, const QString& text, const QColor& color = QColor(0x47, 0x55, 0x69));
    bool RemoveComment(int id);
    GraphCommentData* FindComment(int id);
    const GraphCommentData* FindComment(int id) const;
    const QVector<GraphCommentData>& Comments() const { return comments_; }

    const GraphNodeData* FindNode(int id) const;
    GraphNodeData* FindNode(int id);

    const QVector<GraphNodeData>& Nodes() const { return nodes_; }
    const QVector<GraphConnectionData>& Connections() const { return connections_; }
    int NextNodeId() const { return nextNodeId_; }

    // Заголовок V1. Документ без схемы пишется в Legacy.
    Format FileFormat() const { return format_; }
    void SetSchema(const QString& schema, int schemaVersion);
    const QString& Schema() const { return schema_; }
    int SchemaVersion() const { return schemaVersion_; }
    int Version() const { return version_; }
    // Файл новее этой сборки редактора: показывать можно, записывать нельзя.
    bool IsNewerFormat() const { return format_ == Format::V1 && version_ > FormatVersion; }

    const QJsonArray& Variables() const { return variables_; }
    void SetVariables(const QJsonArray& variables) { variables_ = variables; }

    QString ExportToJson() const;
    // false: не JSON-объект. Прежнее содержимое при ошибке не меняется.
    bool ImportFromJson(const QString& jsonString);

private:
    QVector<GraphNodeData> nodes_;
    QVector<GraphConnectionData> connections_;
    QVector<GraphCommentData> comments_;
    int nextNodeId_ = 1;
    int nextCommentId_ = 1;

    Format format_ = Format::Legacy;
    int version_ = FormatVersion;
    QString schema_;
    int schemaVersion_ = 0;
    QJsonArray variables_;
    QJsonObject extra_; // неизвестные поля корня
};

} // namespace EditorCore
