#pragma once

#include <QPointF>
#include <QWidget>
#include <cstdint>
#include <functional>

class QTimer;
class QShowEvent;
class QHideEvent;
class QResizeEvent;
class QMouseEvent;
class QWheelEvent;
class QFocusEvent;

// Realtime engine preview hosted in a native HWND: the engine Presents into this widget and
// Qt painting is disabled (paintEngine() == nullptr). Rendering is gated on visibility — the
// preview attaches on show and detaches on hide, so a hidden preview costs zero engine frames.
// Orbit/zoom input is coalesced through an FPS-limited timer that runs only while the widget is
// visible and focused with pending input, and stops when idle (no idle repaint timer).
class PreviewWidget final : public QWidget
{
public:
    std::function<void(void* hwnd, uint32_t width, uint32_t height)> attach;
    std::function<void()> detach;
    std::function<void(uint32_t width, uint32_t height)> resized;
    std::function<void(float yaw, float pitch, float zoom)> orbit;

    explicit PreviewWidget(QWidget* parent = nullptr);
    ~PreviewWidget() override;

    QPaintEngine* paintEngine() const override { return nullptr; }
    void SetMaxFps(int fps);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    uint32_t PhysW() const;
    uint32_t PhysH() const;
    void Attach();
    void Detach();
    void QueueOrbit(float yaw, float pitch, float zoom);
    void FlushOrbit();
    void UpdateOrbitTimer();

    bool attached_ = false;
    bool focused_ = false;
    QPointF last_{};
    float pendingYaw_ = 0.0f;
    float pendingPitch_ = 0.0f;
    float pendingZoom_ = 0.0f;
    bool hasPending_ = false;
    int maxFps_ = 60;
    QTimer* orbitTimer_ = nullptr;
};