#include "PreviewWidget.h"

#include <QFocusEvent>
#include <QHideEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

PreviewWidget::PreviewWidget(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true); // the engine Presents here
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMinimumSize(240, 240);
    setFocusPolicy(Qt::ClickFocus);

    orbitTimer_ = new QTimer(this);
    orbitTimer_->setSingleShot(true);
    orbitTimer_->setInterval(1000 / maxFps_);
    connect(orbitTimer_, &QTimer::timeout, this, [this] { FlushOrbit(); });
}

PreviewWidget::~PreviewWidget() = default;

void PreviewWidget::SetMaxFps(int fps)
{
    maxFps_ = std::clamp(fps, 1, 240);
    if (orbitTimer_)
        orbitTimer_->setInterval(1000 / maxFps_);
}

uint32_t PreviewWidget::PhysW() const
{
    return static_cast<uint32_t>(std::lround(width() * devicePixelRatioF()));
}

uint32_t PreviewWidget::PhysH() const
{
    return static_cast<uint32_t>(std::lround(height() * devicePixelRatioF()));
}

void PreviewWidget::Attach()
{
    if (attached_ || !attach)
        return;
    attached_ = true;
    attach(reinterpret_cast<void*>(winId()), PhysW(), PhysH()); // winId creates the native window
}

void PreviewWidget::Detach()
{
    if (orbitTimer_)
        orbitTimer_->stop();
    hasPending_ = false;
    pendingYaw_ = pendingPitch_ = pendingZoom_ = 0.0f;
    if (!attached_)
        return;
    attached_ = false;
    if (detach)
        detach();
}

void PreviewWidget::QueueOrbit(float yaw, float pitch, float zoom)
{
    pendingYaw_ += yaw;
    pendingPitch_ += pitch;
    pendingZoom_ += zoom;
    hasPending_ = true;
    UpdateOrbitTimer();
}

void PreviewWidget::FlushOrbit()
{
    if (!hasPending_)
        return;
    const float yaw = pendingYaw_, pitch = pendingPitch_, zoom = pendingZoom_;
    pendingYaw_ = pendingPitch_ = pendingZoom_ = 0.0f;
    hasPending_ = false;
    if (orbit)
        orbit(yaw, pitch, zoom);
}

void PreviewWidget::UpdateOrbitTimer()
{
    if (!orbitTimer_)
        return;
    if (isVisible() && focused_ && hasPending_)
    {
        if (!orbitTimer_->isActive())
            orbitTimer_->start();
    }
    else
    {
        orbitTimer_->stop();
    }
}

void PreviewWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    Attach();
}

void PreviewWidget::hideEvent(QHideEvent* event)
{
    Detach();
    QWidget::hideEvent(event);
}

void PreviewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (attached_ && resized)
        resized(PhysW(), PhysH());
}

void PreviewWidget::mousePressEvent(QMouseEvent* event)
{
    last_ = event->position();
    setFocus(Qt::MouseFocusReason);
}

void PreviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & (Qt::LeftButton | Qt::RightButton)) || !orbit)
        return;
    const QPointF delta = event->position() - last_;
    last_ = event->position();
    QueueOrbit(static_cast<float>(delta.x()) * 0.01f, static_cast<float>(delta.y()) * 0.01f, 0.0f);
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    FlushOrbit();
}

void PreviewWidget::wheelEvent(QWheelEvent* event)
{
    if (!orbit)
        return;
    setFocus(Qt::MouseFocusReason);
    QueueOrbit(0.0f, 0.0f, static_cast<float>(event->angleDelta().y()) / 120.0f);
}

void PreviewWidget::focusInEvent(QFocusEvent* event)
{
    focused_ = true;
    UpdateOrbitTimer();
    QWidget::focusInEvent(event);
}

void PreviewWidget::focusOutEvent(QFocusEvent* event)
{
    focused_ = false;
    FlushOrbit();
    if (orbitTimer_)
        orbitTimer_->stop();
    QWidget::focusOutEvent(event);
}