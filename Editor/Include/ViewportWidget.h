#pragma once

#include <QWidget>
#include <QSet>
#include <QPointF>
#include <QPoint>

class EditorQtHost;

class ViewportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewportWidget(EditorQtHost* host, QWidget* parent = nullptr);

signals:
    void contextMenuRequested(const QPoint& globalPos);

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

    // Подбери под масштаб сцены.
    // Лучше заменить на camera-based scale, см. ниже.
    float dragSensitivity_ = 0.002f;
};
