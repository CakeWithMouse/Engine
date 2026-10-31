#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace EditorCore
{

struct GraphPin
{
    QString name;
    bool isInput = false;
    QString type = QStringLiteral("float"); // float, float2, float3, float4
    QPointF localPos;
};

struct GraphNodeData
{
    int id = 0;
    QString type;   // PBRMasterOutput, ColorParam, ScalarParam, Texture2D, ORMTexture, Time, UVCoord, Panner, Multiply, Add, Lerp, OneMinus, Sine, Fresnel
    QString title;
    QPointF pos;
    QVariantMap properties;
};

struct GraphConnectionData
{
    int fromNode = 0;
    QString fromPin;
    int toNode = 0;
    QString toPin;
};

class MaterialGraphModel
{
public:
    MaterialGraphModel() = default;

    int AddNode(const QString& type, const QString& title, const QPointF& pos, const QVariantMap& props = {});
    bool RemoveNode(int nodeId);

    bool AddConnection(int fromNode, const QString& fromPin, int toNode, const QString& toPin);
    bool RemoveConnection(int fromNode, const QString& fromPin, int toNode, const QString& toPin);

    void Clear();
    void SetupDefaultMasterNode();

    const GraphNodeData* FindNode(int id) const;
    GraphNodeData* FindNode(int id);

    const QVector<GraphNodeData>& Nodes() const { return nodes_; }
    const QVector<GraphConnectionData>& Connections() const { return connections_; }
    int NextNodeId() const { return nextNodeId_; }

    QString ExportToJson() const;
    bool ImportFromJson(const QString& jsonString);

private:
    QVector<GraphNodeData> nodes_;
    QVector<GraphConnectionData> connections_;
    int nextNodeId_ = 1;
};

} // namespace EditorCore
