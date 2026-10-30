#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QVariantMap>
#include <functional>

class QGraphicsScene;
class QGraphicsView;
class QToolButton;
class QPushButton;
class QMenu;

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

// Визуальный нодовый редактор материалов в стиле Unreal Engine 5 / Blender Shader Editor.
// Не использует Q_OBJECT, чтобы не требовать moc в vcxproj.
class MaterialGraphWidget : public QWidget
{
public:
    explicit MaterialGraphWidget(QWidget* parent = nullptr);
    ~MaterialGraphWidget() override;

    void SetMaterialPath(const QString& matPath, const QString& hlslPath);
    void LoadGraphFromFile(const QString& graphPath);
    void SaveGraphToFile(const QString& graphPath);

    QString CompileGraphToHlsl();
    QString ExportGraphToJson() const;
    bool ImportGraphFromJson(const QString& jsonString);

    void SetOnHlslGeneratedCallback(std::function<void(const QString& hlsl)> callback)
    {
        onHlslGenerated_ = std::move(callback);
    }

    void LoadPreset(const QString& presetName);
    void RetranslateUi();

    // Scene & View helpers
    void OnNodeMoved(int nodeId, const QPointF& newPos);
    void OnNodeDoubleClicked(int nodeId);
    void OnPinClicked(int nodeId, const QString& pinName, bool isInput, const QPointF& scenePos);
    void OnCanvasContextMenu(const QPoint& screenPos, const QPointF& scenePos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void BuildUi();
    void SetupDefaultMasterNode();
    void AddNode(const QString& type, const QPointF& pos, const QVariantMap& props = {});
    void DeleteSelected();
    void RebuildScene();
    void UpdateConnectionsVisual();

    void CopyGraphForAi();
    void PasteGraphFromAi();

    QGraphicsScene* scene_ = nullptr;
    QGraphicsView* view_ = nullptr;

    QString materialPath_;
    QString shaderPath_;

    QVector<GraphNodeData> nodes_;
    QVector<GraphConnectionData> connections_;
    int nextNodeId_ = 1;

    std::function<void(const QString& hlsl)> onHlslGenerated_;

    // Toolbar buttons
    QToolButton* btnAddNode_ = nullptr;
    QMenu* menuAddNode_ = nullptr;
    QPushButton* btnCompileHlsl_ = nullptr;
    QPushButton* btnCopyAi_ = nullptr;
    QPushButton* btnPasteAi_ = nullptr;
    QToolButton* btnPresets_ = nullptr;
    QMenu* menuPresets_ = nullptr;

    // Interactive connection dragging
    bool isConnecting_ = false;
    int connectStartNodeId_ = 0;
    QString connectStartPinName_;
    bool connectStartIsInput_ = false;
    QPointF connectStartPos_;

    // Panning state
    bool isPanning_ = false;
    QPoint panStartPos_;
};
