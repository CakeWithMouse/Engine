#pragma once

#include <QWidget>
#include <QSet>
#include <QPointF>
#include <QPoint>
#include <cstdint>

#include "QtHost.h"

class ViewportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewportWidget(EditorQtHost* host, QWidget* parent = nullptr);

    void setGizmoMode(EditorGizmoMode mode);
    EditorGizmoMode gizmoMode() const { return gizmoMode_; }

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

    bool rmbPressed_ = false;
    bool rmbMoved_ = false;
    QPoint rmbPressPos_;

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
};
