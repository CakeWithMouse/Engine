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
    setToolTip(
        "Space: Play/Stop | P: Pause | RMB: look | WASD: move | Tab: select object | Arrows/PgUp/PgDn: move object");

    auto* timer = new QTimer(this);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();

    connect(timer, &QTimer::timeout, this, [this, elapsed]
    {
        const float dt = std::min(0.05f, static_cast<float>(elapsed->restart()) / 1000.0f);
        if (!attached_ || !hasFocus() || !isActiveWindow()) return;

        const float speed = dt * (keys_.contains(Qt::Key_Shift) ? 4.0f : 1.0f);

        // Если есть выделенный объект, управляем им, иначе камерой
        if (host_->HasSelection())
        {
            float right = static_cast<float>(keys_.contains(Qt::Key_Right) - keys_.contains(Qt::Key_Left)) * speed;
            float up = static_cast<float>(keys_.contains(Qt::Key_PageUp) - keys_.contains(Qt::Key_PageDown)) * speed;
            float forward = static_cast<float>(keys_.contains(Qt::Key_Up) - keys_.contains(Qt::Key_Down)) * speed;

            if (right != 0.0f || up != 0.0f || forward != 0.0f)
            {
                host_->MoveSelectedObject(right, up, forward);
            }
        }
        else
        {
            EditorCommand c;
            c.type = EditorCommand::ViewportCamera;
            c.right = static_cast<float>(keys_.contains(Qt::Key_D) - keys_.contains(Qt::Key_A)) * speed;
            c.up = static_cast<float>(keys_.contains(Qt::Key_E) - keys_.contains(Qt::Key_Q)) * speed;
            c.forward = static_cast<float>(keys_.contains(Qt::Key_W) - keys_.contains(Qt::Key_S)) * speed;
            if (c.right || c.up || c.forward) host_->PostCommand(c);
        }
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
        EditorCommand c;
        c.type = EditorCommand::ViewportDetach;
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
        return; // нативный контент, не трогаем
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
    c.hwnd = (void*)winId(); // создаёт нативное окно виджета
    c.w = PhysW();
    c.h = PhysH();
    host_->PostCommand(c);
    attached_ = true;
}

void ViewportWidget::SendResize()
{
    EditorCommand c;
    c.type = EditorCommand::ViewportResize;
    c.w = PhysW();
    c.h = PhysH();
    host_->PostCommand(c);
}

uint32_t ViewportWidget::PhysW() const { return static_cast<uint32_t>(std::lround(width() * devicePixelRatioF())); }
uint32_t ViewportWidget::PhysH() const { return static_cast<uint32_t>(std::lround(height() * devicePixelRatioF())); }

bool ViewportWidget::event(QEvent* e)
{
    if (e->type() == QEvent::FocusOut || e->type() == QEvent::WindowDeactivate || e->type() == QEvent::Hide)
    {
        looking_ = false;
        rmbPressed_ = false;
        rmbMoved_ = false;
        keys_.clear();
    }

    if (qEnvironmentVariableIsSet("ENGINE_VIEWPORT_INPUT_TRACE") && e->type() == QEvent::KeyPress)
        host_->Log(qPrintable(
            QString("InputKey key=%1 focus=%2").arg(static_cast<QKeyEvent*>(e)->key()).arg(hasFocus())));

    // Глушим стандартное контекстное меню, чтобы показать своё.
    if (e->type() == QEvent::ContextMenu)
    {
        e->accept();
        return true;
    }

    if (e->type() == QEvent::MouseButtonPress)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);

        activateWindow();
        setFocus(Qt::MouseFocusReason);

        if (qEnvironmentVariableIsSet("ENGINE_VIEWPORT_INPUT_TRACE"))
            host_->Log(qPrintable(
                QString("InputFocus focus=%1 active=%2 policy=%3").arg(hasFocus()).arg(isActiveWindow()).arg(focusPolicy
                    ())));

        if (mouse->button() == Qt::RightButton)
        {
            rmbPressed_ = true;
            rmbMoved_ = false;
            rmbPressPos_ = mouse->position().toPoint();
            lastMouse_ = mouse->position();
            return true;
        }
    }

    if (e->type() == QEvent::MouseMove)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);

        if (rmbPressed_ && !rmbMoved_)
        {
            const QPoint delta = mouse->position().toPoint() - rmbPressPos_;

            // Пока движение маленькое — считаем это обычным кликом ПКМ.
            // Если движение началось — включаем режим камеры.
            if (delta.manhattanLength() > 6)
            {
                rmbMoved_ = true;
                looking_ = attached_;
                lastMouse_ = mouse->position();
            }
        }

        if (looking_)
        {
            const auto delta = mouse->position() - lastMouse_;
            lastMouse_ = mouse->position();

            EditorCommand c;
            c.type = EditorCommand::ViewportCamera;
            c.yaw = static_cast<float>(delta.x()) * 0.005f;
            c.pitch = static_cast<float>(delta.y()) * 0.005f;
            host_->PostCommand(c);

            return true;
        }
    }

    if (e->type() == QEvent::MouseButtonRelease)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);

        if (mouse->button() == Qt::RightButton)
        {
            looking_ = false;

            // Если не было движения мышью — показываем меню.
            if (rmbPressed_ && !rmbMoved_)
                emit contextMenuRequested(mapToGlobal(mouse->position().toPoint()));

            rmbPressed_ = false;
            rmbMoved_ = false;

            return true;
        }
    }

    if (!attached_)
        return QWidget::event(e);

    if (e->type() == QEvent::Wheel && hasFocus())
    {
        EditorCommand c;
        c.type = EditorCommand::ViewportCamera;
        c.zoom = static_cast<float>(static_cast<QWheelEvent*>(e)->angleDelta().y()) / 120.0f;
        host_->PostCommand(c);
        return true;
    }

    if ((e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) && hasFocus())
    {
        auto* key = static_cast<QKeyEvent*>(e);

        if (!key->isAutoRepeat())
        {
            // Обработка Tab для циклического выбора объектов
            if (e->type() == QEvent::KeyPress && key->key() == Qt::Key_Tab)
            {
                host_->FetchSceneAndSelectNext();
                return true;
            }
            
            const int navigationKey = CameraNavigationKey(key->nativeScanCode(), key->key());

            if ((navigationKey == Qt::Key_Space || navigationKey == Qt::Key_P) && isActiveWindow())
            {
                if (e->type() == QEvent::KeyPress)
                {
                    EditorCommand c;
                    c.type = navigationKey == Qt::Key_Space ? EditorCommand::TogglePlay : EditorCommand::TogglePause;
                    host_->PostCommand(c);
                }

                return true;
            }

            if (e->type() == QEvent::KeyPress)
                keys_.insert(navigationKey);
            else
                keys_.remove(navigationKey);
        }

        return true;
    }

    return QWidget::event(e);
}
