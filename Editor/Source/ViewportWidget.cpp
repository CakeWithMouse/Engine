#include "ViewportWidget.h"
#include "QtHost.h"
#include "CameraKeys.h"

#include <QPainter>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include <cmath>
#include <memory>
#include <algorithm>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QElapsedTimer>

ViewportWidget::ViewportWidget(EditorQtHost* host, QWidget* parent)
    : QWidget(parent), host_(host)
{
    if (host_->HasViewport())
    {
        setAttribute(Qt::WA_NativeWindow, true);
        // Qt не закрашивает и не буферизует: сюда Present'ит движок
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
    }
    setFocusPolicy(Qt::StrongFocus);
    setToolTip("RMB drag: look/orbit | WASD: move | Q/E: down/up | Wheel: zoom | Shift: faster");
    auto* timer = new QTimer(this);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    connect(timer, &QTimer::timeout, this, [this, elapsed] {
        const float dt = std::min(0.05f, float(elapsed->restart()) / 1000.0f);
        if (!attached_ || !hasFocus() || !isActiveWindow()) return;
        EditorCommand c; c.type = EditorCommand::ViewportCamera;
        const float speed = dt * (keys_.contains(Qt::Key_Shift) ? 4.0f : 1.0f);
        c.right = float(keys_.contains(Qt::Key_D) - keys_.contains(Qt::Key_A)) * speed;
        c.up = float(keys_.contains(Qt::Key_E) - keys_.contains(Qt::Key_Q)) * speed;
        c.forward = float(keys_.contains(Qt::Key_W) - keys_.contains(Qt::Key_S)) * speed;
        if (c.right || c.up || c.forward) host_->PostCommand(c);
    });
    timer->start(16);
    setMinimumSize(320, 200);
}

QPaintEngine* ViewportWidget::paintEngine() const
{
    // nullptr только когда контент нативный; в режиме заглушки нужен обычный paintEngine
    return host_->HasViewport() ? nullptr : QWidget::paintEngine();
}

void ViewportWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (host_->HasViewport() && !attached_)
        SendAttach();
}

void ViewportWidget::hideEvent(QHideEvent* e)
{
    if (attached_)
    {
        EditorCommand c; c.type = EditorCommand::ViewportDetach;
        host_->PostCommand(c);
        attached_ = false;
    }
    QWidget::hideEvent(e);
}

void ViewportWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (attached_)
        SendResize();
}

void ViewportWidget::paintEvent(QPaintEvent*)
{
    if (host_->HasViewport())
        return;                                  // нативный контент, не трогаем
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));
    p.setPen(QColor(200, 200, 200));
    p.drawText(rect(), Qt::AlignCenter,
               QStringLiteral("No scene preview is available for this project."));
}

void ViewportWidget::SendAttach()
{
    EditorCommand c;
    c.type = EditorCommand::ViewportAttach;
    c.hwnd = (void*)winId();                     // создаёт нативное окно виджета
    c.w = PhysW(); c.h = PhysH();
    host_->PostCommand(c);
    attached_ = true;
}

void ViewportWidget::SendResize()
{
    EditorCommand c;
    c.type = EditorCommand::ViewportResize;
    c.w = PhysW(); c.h = PhysH();
    host_->PostCommand(c);
}

uint32_t ViewportWidget::PhysW() const { return (uint32_t)std::lround(width()  * devicePixelRatioF()); }
uint32_t ViewportWidget::PhysH() const { return (uint32_t)std::lround(height() * devicePixelRatioF()); }

bool ViewportWidget::event(QEvent* e)
{
    if (e->type() == QEvent::FocusOut || e->type() == QEvent::WindowDeactivate || e->type() == QEvent::Hide)
    {
        looking_ = false;
        keys_.clear();
    }
    if (qEnvironmentVariableIsSet("ENGINE_VIEWPORT_INPUT_TRACE") && e->type() == QEvent::KeyPress)
        host_->Log(qPrintable(QString("InputKey key=%1 focus=%2").arg(static_cast<QKeyEvent*>(e)->key()).arg(hasFocus())));
    if (!attached_) return QWidget::event(e);
    if (e->type() == QEvent::MouseButtonPress)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);
        activateWindow();
        setFocus(Qt::MouseFocusReason);
        if (qEnvironmentVariableIsSet("ENGINE_VIEWPORT_INPUT_TRACE"))
            host_->Log(qPrintable(QString("InputFocus focus=%1 active=%2 policy=%3").arg(hasFocus()).arg(isActiveWindow()).arg(int(focusPolicy()))));
        if (mouse->button() == Qt::RightButton) { looking_ = true; lastMouse_ = mouse->position(); return true; }
    }
    if (e->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent*>(e)->button() == Qt::RightButton)
    { looking_ = false; return true; }
    if (e->type() == QEvent::MouseMove && looking_)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);
        const auto delta = mouse->position() - lastMouse_;
        lastMouse_ = mouse->position();
        EditorCommand c; c.type = EditorCommand::ViewportCamera;
        c.yaw = float(delta.x()) * 0.005f; c.pitch = float(delta.y()) * 0.005f;
        host_->PostCommand(c); return true;
    }
    if (e->type() == QEvent::Wheel && hasFocus())
    {
        EditorCommand c; c.type = EditorCommand::ViewportCamera;
        c.zoom = float(static_cast<QWheelEvent*>(e)->angleDelta().y()) / 120.0f;
        host_->PostCommand(c); return true;
    }
    if ((e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) && hasFocus())
    {
        auto* key = static_cast<QKeyEvent*>(e);
        if (!key->isAutoRepeat())
        {
            const int navigationKey = CameraNavigationKey(key->nativeScanCode(), key->key());
            if (e->type() == QEvent::KeyPress) keys_.insert(navigationKey);
            else keys_.remove(navigationKey);
        }
        return true;
    }
    return QWidget::event(e);
}
