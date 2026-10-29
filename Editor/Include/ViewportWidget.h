#pragma once

#include <QWidget>
#include <QSet>
#include <QPointF>
#include <QPoint>
#include <cstdint>

#include "QtHost.h"
#include "EditorDialogs.h"

class ViewportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewportWidget(EditorQtHost* host, QWidget* parent = nullptr);
    ~ViewportWidget() override;

    void setGizmoMode(EditorGizmoMode mode);
    EditorGizmoMode gizmoMode() const { return gizmoMode_; }

    std::array<double, 3> ComputeSpawnPosition(const QPoint& viewportPos) const;
    std::array<double, 3> lastSpawnPos() const { return lastSpawnPos_; }
    void ShowAdjustLastOperation(uint64_t id, const QString& name, const EngineSceneTransform& transform);
    void SetAdjustLastOperationObjectId(uint64_t id);

signals:
    void contextMenuRequested(const QPoint& globalPos);
    void addMenuRequested(const QPoint& globalPos);
    void addPrimitiveRequested(const QString& typeName, const QString& typeLabel, const std::array<double, 3>& position);
    void gizmoModeChanged(EditorGizmoMode mode);

protected:
    bool event(QEvent* event) override;
    QPaintEngine* paintEngine() const override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

    QSet<int> keys_;
    QPointF lastMouse_;
    bool looking_ = false;

private:
    void SendAttach();
    void SendResize();

    uint32_t PhysW() const;
    uint32_t PhysH() const;

    bool SelectedTransform(uint64_t& id, EngineSceneTransform& transform);
    bool BeginObjectDrag(const QPointF& pos);
    void EndObjectDrag();
    void ApplyRotateScaleGizmo(const QPointF& pos);
    void PostSelectedTransform(const EngineSceneTransform& transform);
    void updateGizmoToolTip();

    EditorQtHost* host_ = nullptr;

    bool attached_ = false;
    int inputState_ = 0;

    bool rmbPressed_ = false;
    bool rmbMoved_ = false;
    QPoint rmbPressPos_;

    bool mmbPressed_ = false;
    QPoint mmbPressPos_;

    bool lmbPressed_ = false;
    bool lmbMoved_ = false;
    bool draggingSelectedObject_ = false;
    QPoint lmbPressPos_;
    QPointF lastDragPos_;

    EditorGizmoMode gizmoMode_ = EditorGizmoTranslate;

    bool gizmoDragSession_ = false;
    uint64_t dragObjectId_ = 0;
    EngineSceneTransform dragStartTransform_{};
    QPointF dragStartPos_;

    float dragSensitivity_ = 0.002f;
    float rotateSensitivity_ = 0.008f;
    float scaleSensitivity_ = 0.005f;

    ControlScheme controlScheme_ = ControlScheme::UnrealUnity;
    int controlSchemeSubId_ = 0;

    std::array<double, 3> lastSpawnPos_{0.0, 0.0, 0.0};
    float camPos_[3] = {0.0f, 2.5f, 5.0f};
    float camYaw_ = -1.5707963f;
    float camPitch_ = -0.2f;

    class AdjustLastOperationPanel* adjustPanel_ = nullptr;
    class PieMenuWidget* pieMenu_ = nullptr;
};
