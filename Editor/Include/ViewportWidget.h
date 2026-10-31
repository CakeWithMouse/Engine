#pragma once

#include <QWidget>
#include <QSet>
#include <QPointF>
#include <QPoint>
#include <cstdint>

#include "QtHost.h"
#include "EditorSettings.h"
#include "GizmoMath.h"

class ViewportHeaderBar;

class ViewportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewportWidget(EditorQtHost* host, QWidget* parent = nullptr);
    ~ViewportWidget() override;

    void setGizmoMode(EditorGizmoMode mode);
    EditorGizmoMode gizmoMode() const { return gizmoMode_; }

    void SetSnapEnabled(bool enabled);
    bool IsSnapEnabled() const { return snapEnabled_; }
    void ToggleSnap();

    void SetCoordinateSpaceLocal(bool local);
    bool IsCoordinateSpaceLocal() const { return localOrientation_; }
    void ToggleCoordinateSpace();

    void RetranslateUi();
    void AttachHeaderBar(ViewportHeaderBar* bar);

    QSize minimumSizeHint() const override { return QSize(80, 60); }
    QSize sizeHint() const override { return QSize(800, 600); }

signals:
    void contextMenuRequested(const QPoint& globalPos);
    void gizmoModeChanged(EditorGizmoMode mode);

protected:
    bool event(QEvent* event) override;
    QPaintEngine* paintEngine() const override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

    // Split input handlers by method
    bool HandleFocusEvent(QEvent* e);
    bool HandleGameInput(QEvent* e);
    bool HandleKeyPress(QKeyEvent* e);
    bool HandleKeyRelease(QKeyEvent* e);
    bool HandleMousePress(QMouseEvent* e);
    bool HandleMouseMove(QMouseEvent* e);
    bool HandleMouseRelease(QMouseEvent* e);
    bool HandleWheel(QWheelEvent* e);

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
    void CancelObjectDrag();
    void ApplyRotateScaleGizmo(const QPointF& pos);
    void PostSelectedTransform(const EngineSceneTransform& transform);
    void updateGizmoToolTip();

    EditorQtHost* host_ = nullptr;

    bool attached_ = false;
    int inputState_ = 0;

    bool rmbPressed_ = false;
    bool rmbMoved_ = false;
    QPoint rmbPressPos_;

    bool lmbPressed_ = false;
    bool lmbMoved_ = false;
    bool draggingSelectedObject_ = false;
    QPoint lmbPressPos_;
    QPointF lastDragPos_;

    EditorGizmoMode gizmoMode_ = EditorGizmoTranslate;
    GizmoMath::AxisLock activeAxis_ = GizmoMath::AxisLock::All;
    float camYaw_ = -1.5707963f;
    float camPitch_ = 0.0f;
    float camPos_[3] = {0.0f, 0.0f, -5.0f};

    bool gizmoDragSession_ = false;
    uint64_t dragObjectId_ = 0;
    EngineSceneTransform dragStartTransform_{};
    QPointF dragStartPos_;

    float dragSensitivity_ = 0.002f;
    float rotateSensitivity_ = 0.008f;
    float scaleSensitivity_ = 0.005f;

    // S6 Gizmo & Snapping & Space
    bool snapEnabled_ = false;
    double moveSnapStep_ = 0.5;
    double rotateSnapStep_ = 15.0;
    double scaleSnapStep_ = 0.25;
    bool localOrientation_ = false;

    ViewportHeaderBar* headerBar_ = nullptr;
    int schemeSubId_ = 0;
    int langSubId_ = 0;
};
