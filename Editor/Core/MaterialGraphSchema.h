#pragma once

#include "Graph/GraphSchema.h"

namespace EditorCore
{

// Схема графа материала: ноды HlslGraphCompiler, пины float1..4, любая связь допустима (размеры приводит
// компилятор), выход материала обязателен. Файл — Legacy *.mat.graph.json.
class MaterialGraphSchema final : public GraphSchema
{
public:
    static const MaterialGraphSchema& Instance();

    static const QString& MasterType(); // MaterialOutput
    static bool IsTextureType(const QString& type);
    static bool IsParameterType(const QString& type);
    static int VectorSize(const QString& type); // Vector2Param/Vector3Param: 2/3, остальные 0

    QString Id() const override { return {}; }
    QString ObjectPrefix() const override { return QStringLiteral("MaterialGraph"); }
    QString ClipboardMime() const override { return QStringLiteral("application/x-painengine-material-nodes"); }
    QString ClipboardMarker() const override { return QStringLiteral("materialGraphNodes"); }

    QVector<GraphNodeGroup> Groups() const override;
    bool IsKnownNode(const QString& type) const override;
    QString DisplayName(const QString& type) const override;
    QString Hint(const QString& type) const override;
    QString SearchKeywords(const QString& type) const override;
    QString SearchPlaceholder() const override;
    QColor NodeColor(const GraphNodeData& node) const override;
    QColor PinColor(const GraphPin& pin) const override;

    QVector<GraphPin> Pins(const GraphNodeData& node) const override;

    bool IsFixed(const GraphNodeData& node) const override { return node.type == MasterType(); }
    void EnsureRequiredNodes(GraphDocument& document) const override;

    // Имя в HLSL: параметр cbuffer (name) или Texture2D (textureName).
    QString NodeName(const GraphNodeData& node) const override;
    QString NameProblem(const QString& name) const override;
    void SetNodeName(GraphNodeData& node, const QString& name) const override;
    QString NodeTitle(const GraphNodeData& node) const override;

    void PrepareNewNode(const GraphDocument& document, const QString& type, QVariantMap& properties, QString& title) const override;
    // Копия ноды текстуры читает ту же текстуру, если имя свободно или занято текстурой;
    // параметр с тем же именем был бы тем же параметром, поэтому получает новое имя.
    void PreparePastedNode(const GraphDocument& document, GraphNodeData& node) const override;
};

} // namespace EditorCore
